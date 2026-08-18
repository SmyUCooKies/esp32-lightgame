#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

typedef enum {
    GAME_COLOR_RED = 0,
    GAME_COLOR_GREEN = 1,
    GAME_COLOR_BLUE = 2,
    GAME_COLOR_COUNT = 3,
} game_color_t;

typedef enum {
    GAME_PHASE_IDLE = 0,
    GAME_PHASE_PLAYING,
    GAME_PHASE_PAUSED,
    GAME_PHASE_GAME_OVER,
} game_phase_t;

typedef struct {
    float pos;
    game_color_t color;
    bool active;
} enemy_t;

typedef struct {
    float pos;
    game_color_t color;
    bool active;
} bullet_t;

typedef struct {
    uint16_t led_count;
    uint8_t brightness;
    float enemy_speed;
    float bullet_speed;
    uint32_t spawn_interval_ms;
} game_settings_t;

typedef struct {
    enemy_t enemies[MAX_ENEMIES];
    bullet_t bullets[MAX_BULLETS];
    game_phase_t phase;
    int score;
    int high_score;
    uint32_t spawn_timer_ms;
    float current_enemy_speed;
    uint32_t current_spawn_interval_ms;
    game_settings_t settings;
    bool kill_flash;
    uint32_t kill_flash_until_ms;
    int kill_flash_pos;
    game_color_t kill_flash_color;
} game_t;

void game_init(void);
game_t *game_lock(void);
void game_unlock(void);

void game_start(void);
void game_pause(void);
void game_resume(void);
void game_reset(void);
bool game_shoot(game_color_t color);

void game_tick(uint32_t now_ms, float dt_sec);
void game_get_rgb(game_color_t color, uint8_t *r, uint8_t *g, uint8_t *b);

bool game_update_settings(const game_settings_t *new_settings);
const game_settings_t *game_get_settings(void);
void game_save_settings(const game_settings_t *s);
void game_save_high_score(int score);
