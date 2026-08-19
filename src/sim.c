#include "sim.h"

#include <string.h>

#include "present.h"
#include "sim_priv.h"
#include "sweep.h"

_Static_assert(sizeof(struct sim) <= SIM_STORAGE_BYTES, "increase SIM_STORAGE_BYTES");
_Static_assert(_Alignof(struct sim) <= _Alignof(sim_storage_t), "sim_storage_t alignment is too small");

static sim_settings_t clamp_settings(sim_settings_t s)
{
    if (s.led_count < 10) {
        s.led_count = 10;
    }
    if (s.led_count > LED_STRIP_MAX_COUNT) {
        s.led_count = LED_STRIP_MAX_COUNT;
    }
    if (s.brightness < 1) {
        s.brightness = 1;
    }
    if (s.enemy_speed < 0.1f) {
        s.enemy_speed = 0.1f;
    }
    if (s.bullet_speed < 0.1f) {
        s.bullet_speed = 0.1f;
    }
    if (s.spawn_interval_ms < 600) {
        s.spawn_interval_ms = 600;
    }
    return s;
}

static float live_enemy_speed(const sim_t *s)
{
    return s->settings.enemy_speed + (float)s->score * 0.12f;
}

static uint32_t live_spawn_interval_ms(const sim_t *s)
{
    uint32_t base = s->settings.spawn_interval_ms;
    uint32_t cut = (uint32_t)s->score * 45u;
    uint32_t ms = (base > cut) ? base - cut : 600u;
    return ms < 600u ? 600u : ms;
}

static void clear_movers(sim_t *s)
{
    memset(s->enemies, 0, sizeof(s->enemies));
    memset(s->bullets, 0, sizeof(s->bullets));
}

static sim_mover_t *alloc_mover(sim_mover_t *bank, int cap)
{
    for (int i = 0; i < cap; i++) {
        if (!bank[i].live) {
            return &bank[i];
        }
    }
    return NULL;
}

static bool far_end_blocked(const sim_t *s)
{
    const float far_x = (float)(s->settings.led_count - 1u);
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (s->enemies[i].live) {
            float d = s->enemies[i].x - far_x;
            if (d < 0.0f) {
                d = -d;
            }
            if (d <= 0.5f) {
                return true;
            }
        }
    }
    return false;
}

static uint32_t rng_u32(sim_t *s)
{
    if (!s->rng.next_u32) {
        return 0;
    }
    return s->rng.next_u32(s->rng.ctx);
}

static bool try_spawn(sim_t *s)
{
    if (far_end_blocked(s)) {
        return false;
    }
    sim_mover_t *e = alloc_mover(s->enemies, MAX_ENEMIES);
    if (!e) {
        return false;
    }
    e->live = true;
    e->x = (float)(s->settings.led_count - 1u);
    e->color = (sim_color_t)(rng_u32(s) % (uint32_t)SIM_COLOR_COUNT);
    return true;
}

static void enter_playing(sim_t *s)
{
    clear_movers(s);
    s->score = 0;
    s->combo = 0;
    s->spawn_age_ms = 0;
    s->spawn_due = true;
    s->kill_flash_ms = 0;
    s->phase_clock_ms = 0;
    s->phase = SIM_PHASE_PLAYING;
}

static void capture(const sim_t *s, sim_snapshot_t *out)
{
    present_in_t in;
    memset(&in, 0, sizeof(in));
    in.phase = s->phase;
    in.score = s->score;
    in.high_score = s->high_score;
    in.combo = s->combo;
    in.settings = s->settings;
    in.enemy_speed = live_enemy_speed(s);
    in.spawn_interval_ms = live_spawn_interval_ms(s);
    in.leak_color = s->leak_color;
    in.clock_ms = s->clock_ms;
    in.phase_clock_ms = s->phase_clock_ms;
    in.kill_flash_ms = s->kill_flash_ms;
    in.last_kill_x = s->last_kill_x;
    in.last_shot = s->last_shot;
    in.has_last_shot = s->has_last_shot;
    in.enemies = s->enemies;
    in.bullets = s->bullets;
    present_fill(&in, out);
}

static void apply_cmd(sim_t *s, sim_cmd_t cmd, bool *persist_dirty)
{
    switch (cmd.kind) {
    case SIM_CMD_START:
        if (s->phase == SIM_PHASE_IDLE || s->phase == SIM_PHASE_GAME_OVER) {
            enter_playing(s);
        }
        break;
    case SIM_CMD_SHOOT:
        if (cmd.color >= SIM_COLOR_COUNT) {
            break;
        }
        if (s->phase == SIM_PHASE_IDLE || s->phase == SIM_PHASE_GAME_OVER) {
            enter_playing(s);
        }
        if (s->phase != SIM_PHASE_PLAYING) {
            break;
        }
        {
            sim_mover_t *b = alloc_mover(s->bullets, MAX_BULLETS);
            if (!b) {
                break;
            }
            b->live = true;
            b->color = cmd.color;
            b->x = 0.0f;
            s->last_shot = cmd.color;
            s->has_last_shot = true;
        }
        break;
    case SIM_CMD_SET_PAUSED:
        if (cmd.paused && s->phase == SIM_PHASE_PLAYING) {
            s->phase = SIM_PHASE_PAUSED;
            s->phase_clock_ms = 0;
        } else if (!cmd.paused && s->phase == SIM_PHASE_PAUSED) {
            s->phase = SIM_PHASE_PLAYING;
            s->phase_clock_ms = 0;
        }
        break;
    case SIM_CMD_RESET:
        clear_movers(s);
        s->score = 0;
        s->combo = 0;
        s->spawn_age_ms = 0;
        s->spawn_due = false;
        s->kill_flash_ms = 0;
        s->phase_clock_ms = 0;
        s->phase = SIM_PHASE_IDLE;
        break;
    case SIM_CMD_SET_SETTINGS:
        s->settings = clamp_settings(cmd.settings);
        *persist_dirty = true;
        break;
    }
}

static void maybe_spawn(sim_t *s, uint32_t dt_ms)
{
    const uint32_t interval = live_spawn_interval_ms(s);
    bool due = s->spawn_due;
    if (!due) {
        s->spawn_age_ms += dt_ms;
        due = s->spawn_age_ms >= interval;
    }
    if (!due) {
        return;
    }
    if (!try_spawn(s)) {
        s->spawn_due = true;
        return;
    }
    s->spawn_due = false;
    s->spawn_age_ms = 0;
}

sim_persist_t sim_persist_defaults(void)
{
    sim_persist_t p;
    memset(&p, 0, sizeof(p));
    p.settings.led_count = LED_STRIP_DEFAULT_COUNT;
    p.settings.brightness = DEFAULT_BRIGHTNESS;
    p.settings.enemy_speed = DEFAULT_ENEMY_SPEED;
    p.settings.bullet_speed = DEFAULT_BULLET_SPEED;
    p.settings.spawn_interval_ms = DEFAULT_SPAWN_INTERVAL_MS;
    p.high_score = 0;
    return p;
}

sim_cmd_t sim_cmd_start(void)
{
    sim_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.kind = SIM_CMD_START;
    return c;
}

sim_cmd_t sim_cmd_shoot(sim_color_t color)
{
    sim_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.kind = SIM_CMD_SHOOT;
    c.color = color;
    return c;
}

sim_cmd_t sim_cmd_set_paused(bool paused)
{
    sim_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.kind = SIM_CMD_SET_PAUSED;
    c.paused = paused;
    return c;
}

sim_cmd_t sim_cmd_reset(void)
{
    sim_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.kind = SIM_CMD_RESET;
    return c;
}

sim_cmd_t sim_cmd_set_settings(sim_settings_t settings)
{
    sim_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.kind = SIM_CMD_SET_SETTINGS;
    c.settings = settings;
    return c;
}

sim_t *sim_from_storage(sim_storage_t *storage)
{
    return storage ? (sim_t *)storage : NULL;
}

sim_snapshot_t sim_boot(sim_t *sim, const sim_persist_t *persist, sim_rng_t rng, uint16_t led_count)
{
    memset(sim, 0, sizeof(*sim));
    sim_persist_t p = persist ? *persist : sim_persist_defaults();
    sim->settings = clamp_settings(p.settings);
    if (led_count != 0) {
        sim_settings_t s = sim->settings;
        s.led_count = led_count;
        sim->settings = clamp_settings(s);
    }
    sim->high_score = p.high_score < 0 ? 0 : p.high_score;
    sim->rng = rng;
    sim->phase = SIM_PHASE_IDLE;
    sim_snapshot_t snap;
    capture(sim, &snap);
    return snap;
}

void sim_step(sim_t *sim, uint32_t dt_ms, const sim_cmd_t *cmds, size_t ncmds, sim_out_t *out)
{
    bool persist_dirty = false;

    for (size_t i = 0; i < ncmds; i++) {
        apply_cmd(sim, cmds[i], &persist_dirty);
    }

    sim->clock_ms += dt_ms;
    sim->phase_clock_ms += dt_ms;
    if (sim->kill_flash_ms > dt_ms) {
        sim->kill_flash_ms -= dt_ms;
    } else {
        sim->kill_flash_ms = 0;
    }

    if (sim->phase == SIM_PHASE_PLAYING) {
        maybe_spawn(sim, dt_ms);

        sweep_in_t sin;
        memset(&sin, 0, sizeof(sin));
        sin.enemies = sim->enemies;
        sin.bullets = sim->bullets;
        sin.dt_sec = (float)dt_ms / 1000.0f;
        sin.enemy_speed = live_enemy_speed(sim);
        sin.bullet_speed = sim->settings.bullet_speed;
        sin.x_min = 0.0f;
        sin.x_max = (float)(sim->settings.led_count - 1u);

        sweep_out_t sout;
        sweep_advance(&sin, &sout);

        if (sout.kills > 0) {
            sim->score += sout.kills;
            sim->combo += sout.kills;
            sim->last_kill_x = sout.last_kill_x;
            sim->kill_flash_ms = 120;
            if (sim->score > sim->high_score) {
                sim->high_score = sim->score;
                persist_dirty = true;
            }
        }
        if (sout.absorbs > 0) {
            sim->combo = 0;
        }
        if (sout.leaked) {
            sim->phase = SIM_PHASE_GAME_OVER;
            sim->leak_color = sout.leak_color;
            sim->phase_clock_ms = 0;
            if (sim->score > sim->high_score) {
                sim->high_score = sim->score;
                persist_dirty = true;
            }
        }
    }

    out->persist_dirty = persist_dirty;
    capture(sim, &out->snapshot);
}

sim_persist_t sim_persist_of(const sim_t *sim)
{
    sim_persist_t p;
    p.settings = sim->settings;
    p.high_score = sim->high_score;
    return p;
}
