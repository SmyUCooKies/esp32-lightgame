#include "present.h"

#include <math.h>
#include <string.h>

static const uint8_t k_rgb[SIM_COLOR_COUNT][3] = {
    {255, 0, 0},
    {0, 255, 0},
    {0, 80, 255},
};

static uint8_t sat_add(uint8_t a, uint8_t b)
{
    unsigned s = (unsigned)a + (unsigned)b;
    return s > 255u ? 255u : (uint8_t)s;
}

static void add_rgb(sim_px_t *p, uint8_t r, uint8_t g, uint8_t b)
{
    p->r = sat_add(p->r, r);
    p->g = sat_add(p->g, g);
    p->b = sat_add(p->b, b);
}

static uint8_t scale8(uint8_t v, uint8_t brightness)
{
    return (uint8_t)((unsigned)v * (unsigned)brightness / 255u);
}

static void splat(sim_px_t *px, uint16_t n, float x, uint8_t r, uint8_t g, uint8_t b)
{
    if (n == 0) {
        return;
    }
    const float xf = x;
    int i0 = (int)floorf(xf);
    float frac = xf - (float)i0;
    if (frac < 0.0f) {
        frac = 0.0f;
    }
    int i1 = i0 + 1;
    uint8_t w0 = (uint8_t)((1.0f - frac) * 255.0f);
    uint8_t w1 = (uint8_t)(frac * 255.0f);
    if (i0 >= 0 && i0 < (int)n) {
        add_rgb(&px[i0], scale8(r, w0), scale8(g, w0), scale8(b, w0));
    }
    if (i1 >= 0 && i1 < (int)n) {
        add_rgb(&px[i1], scale8(r, w1), scale8(g, w1), scale8(b, w1));
    }
}

static uint32_t controls_for(sim_phase_t phase)
{
    switch (phase) {
    case SIM_PHASE_IDLE:
        return SIM_CTRL_SHOOT | SIM_CTRL_START;
    case SIM_PHASE_PLAYING:
        return SIM_CTRL_SHOOT | SIM_CTRL_PAUSE | SIM_CTRL_RESET;
    case SIM_PHASE_PAUSED:
        return SIM_CTRL_RESUME | SIM_CTRL_RESET;
    case SIM_PHASE_GAME_OVER:
        return SIM_CTRL_SHOOT | SIM_CTRL_START | SIM_CTRL_RESET;
    default:
        return 0;
    }
}

static uint16_t count_live(const sim_mover_t *m, int cap)
{
    uint16_t n = 0;
    for (int i = 0; i < cap; i++) {
        if (m[i].live) {
            n++;
        }
    }
    return n;
}

static void hue_rgb(float t, uint8_t *r, uint8_t *g, uint8_t *b)
{
    float h = t - floorf(t);
    float x = 1.0f - fabsf(fmodf(h * 6.0f, 2.0f) - 1.0f);
    uint8_t c = 80;
    uint8_t m = (uint8_t)(x * 80.0f);
    if (h < 1.0f / 6.0f) {
        *r = c;
        *g = m;
        *b = 0;
    } else if (h < 2.0f / 6.0f) {
        *r = m;
        *g = c;
        *b = 0;
    } else if (h < 3.0f / 6.0f) {
        *r = 0;
        *g = c;
        *b = m;
    } else if (h < 4.0f / 6.0f) {
        *r = 0;
        *g = m;
        *b = c;
    } else if (h < 5.0f / 6.0f) {
        *r = m;
        *g = 0;
        *b = c;
    } else {
        *r = c;
        *g = 0;
        *b = m;
    }
}

static void draw_idle(sim_px_t *px, uint16_t n, uint32_t clock_ms)
{
    const float t = (float)(clock_ms % 3000u) / 3000.0f;
    for (uint16_t i = 0; i < n; i++) {
        float pos = (float)i / (float)(n > 1 ? n : 1) + t;
        uint8_t r, g, b;
        hue_rgb(pos, &r, &g, &b);
        px[i].r = r;
        px[i].g = g;
        px[i].b = b;
    }
}

static void draw_game_over(sim_px_t *px, uint16_t n, const present_in_t *in)
{
    const uint8_t *c = k_rgb[in->leak_color % SIM_COLOR_COUNT];
    if (in->phase_clock_ms < 600u) {
        for (uint16_t i = 0; i < n; i++) {
            px[i].r = scale8(c[0], 140);
            px[i].g = scale8(c[1], 140);
            px[i].b = scale8(c[2], 140);
        }
        return;
    }

    int left = in->score;
    uint16_t i = 0;
    while (i < n && left > 0) {
        int run = left > 5 ? 5 : left;
        if ((int)i + run > (int)n) {
            break;
        }
        for (int k = 0; k < run; k++) {
            px[i].r = scale8(c[0], 180);
            px[i].g = scale8(c[1], 180);
            px[i].b = scale8(c[2], 180);
            i++;
        }
        left -= run;
        if (left > 0 && i < n) {
            i++;
        }
    }

    if (left > 0 && ((in->phase_clock_ms / 400u) % 2u) == 0u) {
        for (uint16_t k = 0; k < n; k++) {
            add_rgb(&px[k], scale8(c[0], 40), scale8(c[1], 40), scale8(c[2], 40));
        }
    }
}

static void draw_play(sim_px_t *px, uint16_t n, const present_in_t *in, uint8_t *near_out)
{
    if (in->has_last_shot) {
        const uint8_t *c = k_rgb[in->last_shot % SIM_COLOR_COUNT];
        add_rgb(&px[0], scale8(c[0], 20), scale8(c[1], 20), scale8(c[2], 20));
    } else if (n > 0) {
        add_rgb(&px[0], 12, 12, 12);
    }

    float zone = 3.0f;
    if (n / 8u < 3u) {
        zone = (float)(n / 8u);
    }
    if (zone < 1.0f) {
        zone = 1.0f;
    }

    float near_f = 0.0f;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const sim_mover_t *e = &in->enemies[i];
        if (!e->live) {
            continue;
        }
        if (e->x >= 0.0f && e->x < zone) {
            float v = (zone - e->x) / zone;
            if (v > near_f) {
                near_f = v;
            }
        }
    }
    uint8_t near = (uint8_t)(near_f * 255.0f);
    *near_out = near;
    if (near > 0) {
        float pulse = 0.55f + 0.45f * sinf((float)in->clock_ms / 180.0f);
        uint8_t warm = (uint8_t)((float)near * pulse / 255.0f * 90.0f);
        int span = (int)zone;
        if (span < 1) {
            span = 1;
        }
        if (span > (int)n) {
            span = (int)n;
        }
        for (int i = 0; i < span; i++) {
            add_rgb(&px[i], warm, warm / 3, 0);
        }
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        const sim_mover_t *e = &in->enemies[i];
        if (!e->live) {
            continue;
        }
        const uint8_t *c = k_rgb[e->color % SIM_COLOR_COUNT];
        splat(px, n, e->x, scale8(c[0], 160), scale8(c[1], 160), scale8(c[2], 160));
    }
    for (int i = 0; i < MAX_BULLETS; i++) {
        const sim_mover_t *b = &in->bullets[i];
        if (!b->live) {
            continue;
        }
        const uint8_t *c = k_rgb[b->color % SIM_COLOR_COUNT];
        splat(px, n, b->x, c[0], c[1], c[2]);
    }

    if (in->kill_flash_ms > 0) {
        splat(px, n, in->last_kill_x, 255, 255, 255);
    }
}

void present_fill(const present_in_t *in, sim_snapshot_t *out)
{
    memset(out, 0, sizeof(*out));
    out->phase = in->phase;
    out->score = in->score;
    out->high_score = in->high_score;
    out->combo = in->combo;
    out->enemy_speed = in->enemy_speed;
    out->spawn_interval_ms = in->spawn_interval_ms;
    out->settings = in->settings;
    out->leak_color = in->leak_color;
    out->controls = in->controls ? in->controls : controls_for(in->phase);
    out->led_count = in->settings.led_count;
    if (out->led_count > LED_STRIP_MAX_COUNT) {
        out->led_count = LED_STRIP_MAX_COUNT;
    }
    out->enemy_count = count_live(in->enemies, MAX_ENEMIES);
    out->bullet_count = count_live(in->bullets, MAX_BULLETS);

    const uint16_t n = out->led_count;
    switch (in->phase) {
    case SIM_PHASE_IDLE:
        draw_idle(out->pixels, n, in->clock_ms);
        break;
    case SIM_PHASE_GAME_OVER:
        draw_game_over(out->pixels, n, in);
        break;
    case SIM_PHASE_PLAYING:
    case SIM_PHASE_PAUSED:
        draw_play(out->pixels, n, in, &out->near_base);
        break;
    }

    const uint8_t br = in->settings.brightness;
    for (uint16_t i = 0; i < n; i++) {
        out->pixels[i].r = scale8(out->pixels[i].r, br);
        out->pixels[i].g = scale8(out->pixels[i].g, br);
        out->pixels[i].b = scale8(out->pixels[i].b, br);
    }
}
