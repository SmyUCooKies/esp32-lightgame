#include "game.h"

#include <stdlib.h>
#include <string.h>

#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"

static game_t s_game;
static SemaphoreHandle_t s_mutex;

static const uint8_t s_color_rgb[GAME_COLOR_COUNT][3] = {
    {255, 0, 0},
    {0, 255, 0},
    {0, 80, 255},
};

static void apply_difficulty(game_t *g)
{
    g->current_enemy_speed = g->settings.enemy_speed + g->score * 0.12f;
    uint32_t interval = g->settings.spawn_interval_ms;
    if (g->score > 0) {
        interval = (interval > (uint32_t)(g->score * 45)) ? interval - g->score * 45 : 600;
    }
    if (interval < 600) {
        interval = 600;
    }
    g->current_spawn_interval_ms = interval;
}

static void clear_entities(game_t *g)
{
    memset(g->enemies, 0, sizeof(g->enemies));
    memset(g->bullets, 0, sizeof(g->bullets));
}

static enemy_t *alloc_enemy(game_t *g)
{
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!g->enemies[i].active) {
            return &g->enemies[i];
        }
    }
    return NULL;
}

static bullet_t *alloc_bullet(game_t *g)
{
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!g->bullets[i].active) {
            return &g->bullets[i];
        }
    }
    return NULL;
}

static void spawn_enemy(game_t *g)
{
    enemy_t *e = alloc_enemy(g);
    if (!e) {
        return;
    }

    e->active = true;
    e->pos = (float)(g->settings.led_count - 1);
    e->color = (game_color_t)(esp_random() % GAME_COLOR_COUNT);
}

static void load_settings(game_settings_t *s)
{
    s->led_count = LED_STRIP_DEFAULT_COUNT;
    s->brightness = DEFAULT_BRIGHTNESS;
    s->enemy_speed = DEFAULT_ENEMY_SPEED;
    s->bullet_speed = DEFAULT_BULLET_SPEED;
    s->spawn_interval_ms = DEFAULT_SPAWN_INTERVAL_MS;

    nvs_handle_t nvs;
    if (nvs_open("lightgame", NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }

    uint16_t led_count = s->led_count;
    if (nvs_get_u16(nvs, "led_count", &led_count) == ESP_OK) {
        s->led_count = led_count;
    }
    uint8_t brightness = s->brightness;
    if (nvs_get_u8(nvs, "brightness", &brightness) == ESP_OK) {
        s->brightness = brightness;
    }
    uint32_t spawn_ms = s->spawn_interval_ms;
    if (nvs_get_u32(nvs, "spawn_ms", &spawn_ms) == ESP_OK) {
        s->spawn_interval_ms = spawn_ms;
    }
    int32_t enemy_speed_x10 = (int32_t)(s->enemy_speed * 10.0f);
    if (nvs_get_i32(nvs, "enemy_spd", &enemy_speed_x10) == ESP_OK) {
        s->enemy_speed = enemy_speed_x10 / 10.0f;
    }
    int high = 0;
    if (nvs_get_i32(nvs, "high_score", (int32_t *)&high) == ESP_OK) {
        (void)high;
    }
    nvs_close(nvs);
}

void game_save_settings(const game_settings_t *s)
{
    nvs_handle_t nvs;
    if (nvs_open("lightgame", NVS_READWRITE, &nvs) != ESP_OK) {
        return;
    }
    nvs_set_u16(nvs, "led_count", s->led_count);
    nvs_set_u8(nvs, "brightness", s->brightness);
    nvs_set_u32(nvs, "spawn_ms", s->spawn_interval_ms);
    nvs_set_i32(nvs, "enemy_spd", (int32_t)(s->enemy_speed * 10.0f));
    nvs_commit(nvs);
    nvs_close(nvs);
}

void game_save_high_score(int score)
{
    nvs_handle_t nvs;
    if (nvs_open("lightgame", NVS_READWRITE, &nvs) != ESP_OK) {
        return;
    }
    nvs_set_i32(nvs, "high_score", score);
    nvs_commit(nvs);
    nvs_close(nvs);
}

static int load_high_score(void)
{
    nvs_handle_t nvs;
    int32_t high = 0;
    if (nvs_open("lightgame", NVS_READONLY, &nvs) != ESP_OK) {
        return 0;
    }
    nvs_get_i32(nvs, "high_score", &high);
    nvs_close(nvs);
    return (int)high;
}

void game_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    memset(&s_game, 0, sizeof(s_game));
    load_settings(&s_game.settings);
    if (s_game.settings.led_count < 10) {
        s_game.settings.led_count = 10;
    }
    if (s_game.settings.led_count > LED_STRIP_MAX_COUNT) {
        s_game.settings.led_count = LED_STRIP_MAX_COUNT;
    }
    s_game.high_score = load_high_score();
    s_game.phase = GAME_PHASE_IDLE;
    apply_difficulty(&s_game);
}

game_t *game_lock(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    return &s_game;
}

void game_unlock(void)
{
    xSemaphoreGive(s_mutex);
}

void game_start(void)
{
    game_t *g = game_lock();
    clear_entities(g);
    g->score = 0;
    g->spawn_timer_ms = 0;
    g->kill_flash = false;
    g->phase = GAME_PHASE_PLAYING;
    apply_difficulty(g);
    game_unlock();
}

void game_pause(void)
{
    game_t *g = game_lock();
    if (g->phase == GAME_PHASE_PLAYING) {
        g->phase = GAME_PHASE_PAUSED;
    }
    game_unlock();
}

void game_resume(void)
{
    game_t *g = game_lock();
    if (g->phase == GAME_PHASE_PAUSED) {
        g->phase = GAME_PHASE_PLAYING;
    }
    game_unlock();
}

void game_reset(void)
{
    game_t *g = game_lock();
    clear_entities(g);
    g->score = 0;
    g->spawn_timer_ms = 0;
    g->kill_flash = false;
    g->phase = GAME_PHASE_IDLE;
    apply_difficulty(g);
    game_unlock();
}

bool game_shoot(game_color_t color)
{
    if (color >= GAME_COLOR_COUNT) {
        return false;
    }

    game_t *g = game_lock();

    if (g->phase == GAME_PHASE_IDLE || g->phase == GAME_PHASE_GAME_OVER) {
        clear_entities(g);
        g->score = 0;
        g->spawn_timer_ms = 0;
        g->kill_flash = false;
        g->phase = GAME_PHASE_PLAYING;
        apply_difficulty(g);
    }

    if (g->phase != GAME_PHASE_PLAYING) {
        game_unlock();
        return false;
    }

    bullet_t *b = alloc_bullet(g);
    if (!b) {
        game_unlock();
        return false;
    }

    b->active = true;
    b->color = color;
    b->pos = 0.0f;
    game_unlock();
    return true;
}

void game_get_rgb(game_color_t color, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (color >= GAME_COLOR_COUNT) {
        *r = *g = *b = 0;
        return;
    }
    *r = s_color_rgb[color][0];
    *g = s_color_rgb[color][1];
    *b = s_color_rgb[color][2];
}

static void check_collisions(game_t *g, uint32_t now_ms)
{
    for (int i = 0; i < MAX_BULLETS; i++) {
        bullet_t *b = &g->bullets[i];
        if (!b->active) {
            continue;
        }
        int b_idx = (int)(b->pos + 0.5f);

        for (int j = 0; j < MAX_ENEMIES; j++) {
            enemy_t *e = &g->enemies[j];
            if (!e->active) {
                continue;
            }
            int e_idx = (int)(e->pos + 0.5f);
            if (b_idx == e_idx && b->color == e->color) {
                g->kill_flash = true;
                g->kill_flash_until_ms = now_ms + 120;
                g->kill_flash_pos = e_idx;
                g->kill_flash_color = e->color;
                e->active = false;
                b->active = false;
                g->score++;
                if (g->score > g->high_score) {
                    g->high_score = g->score;
                    game_save_high_score(g->high_score);
                }
                apply_difficulty(g);
                break;
            }
        }
    }
}

void game_tick(uint32_t now_ms, float dt_sec)
{
    game_t *g = game_lock();

    if (g->phase != GAME_PHASE_PLAYING) {
        game_unlock();
        return;
    }

    g->spawn_timer_ms += (uint32_t)(dt_sec * 1000.0f);
    if (g->spawn_timer_ms >= g->current_spawn_interval_ms) {
        g->spawn_timer_ms = 0;
        spawn_enemy(g);
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemy_t *e = &g->enemies[i];
        if (!e->active) {
            continue;
        }
        e->pos -= g->current_enemy_speed * dt_sec;
        if (e->pos <= 0.0f) {
            g->phase = GAME_PHASE_GAME_OVER;
            game_unlock();
            return;
        }
    }

    for (int i = 0; i < MAX_BULLETS; i++) {
        bullet_t *b = &g->bullets[i];
        if (!b->active) {
            continue;
        }
        b->pos += g->settings.bullet_speed * dt_sec;
        if (b->pos >= (float)(g->settings.led_count - 1)) {
            b->active = false;
        }
    }

    check_collisions(g, now_ms);

    if (g->kill_flash && now_ms >= g->kill_flash_until_ms) {
        g->kill_flash = false;
    }

    game_unlock();
}

bool game_update_settings(const game_settings_t *new_settings)
{
    game_t *g = game_lock();
    g->settings = *new_settings;
    if (g->settings.led_count < 10) {
        g->settings.led_count = 10;
    }
    if (g->settings.led_count > LED_STRIP_MAX_COUNT) {
        g->settings.led_count = LED_STRIP_MAX_COUNT;
    }
    apply_difficulty(g);
    game_save_settings(&g->settings);
    game_unlock();
    return true;
}

const game_settings_t *game_get_settings(void)
{
    return &s_game.settings;
}
