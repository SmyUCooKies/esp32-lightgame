#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

#define SIM_STORAGE_BYTES 512

typedef enum {
    SIM_COLOR_RED = 0,
    SIM_COLOR_GREEN = 1,
    SIM_COLOR_BLUE = 2,
    SIM_COLOR_COUNT = 3,
} sim_color_t;

typedef enum {
    SIM_PHASE_IDLE = 0,
    SIM_PHASE_PLAYING,
    SIM_PHASE_PAUSED,
    SIM_PHASE_GAME_OVER,
} sim_phase_t;

typedef enum {
    SIM_CMD_START = 0,
    SIM_CMD_SHOOT,
    SIM_CMD_SET_PAUSED,
    SIM_CMD_RESET,
    SIM_CMD_SET_SETTINGS,
} sim_cmd_kind_t;

enum {
    SIM_CTRL_SHOOT = 1u << 0,
    SIM_CTRL_START = 1u << 1,
    SIM_CTRL_RESET = 1u << 2,
    SIM_CTRL_PAUSE = 1u << 3,
    SIM_CTRL_RESUME = 1u << 4,
};

typedef struct {
    uint16_t led_count;
    uint8_t brightness;
    float enemy_speed;
    float bullet_speed;
    uint32_t spawn_interval_ms;
} sim_settings_t;

typedef struct {
    sim_settings_t settings;
    int high_score;
} sim_persist_t;

typedef struct {
    sim_cmd_kind_t kind;
    sim_color_t color;
    bool paused;
    sim_settings_t settings;
} sim_cmd_t;

typedef struct {
    uint32_t (*next_u32)(void *ctx);
    void *ctx;
} sim_rng_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} sim_px_t;

typedef struct {
    sim_phase_t phase;
    int score;
    int high_score;
    int combo;
    float enemy_speed;
    uint32_t spawn_interval_ms;
    sim_settings_t settings;
    sim_color_t leak_color;
    uint16_t enemy_count;
    uint16_t bullet_count;
    uint32_t controls;
    uint16_t led_count;
    sim_px_t pixels[LED_STRIP_MAX_COUNT];
    uint8_t near_base;
} sim_snapshot_t;

typedef struct {
    sim_snapshot_t snapshot;
    bool persist_dirty;
} sim_out_t;

typedef union {
    max_align_t align;
    unsigned char bytes[SIM_STORAGE_BYTES];
} sim_storage_t;

typedef struct sim sim_t;

sim_persist_t sim_persist_defaults(void);

sim_cmd_t sim_cmd_start(void);
sim_cmd_t sim_cmd_shoot(sim_color_t color);
sim_cmd_t sim_cmd_set_paused(bool paused);
sim_cmd_t sim_cmd_reset(void);
sim_cmd_t sim_cmd_set_settings(sim_settings_t settings);

sim_t *sim_from_storage(sim_storage_t *storage);

sim_snapshot_t sim_boot(sim_t *sim, const sim_persist_t *persist, sim_rng_t rng, uint16_t led_count);

void sim_step(sim_t *sim, uint32_t dt_ms, const sim_cmd_t *cmds, size_t ncmds, sim_out_t *out);

sim_persist_t sim_persist_of(const sim_t *sim);
