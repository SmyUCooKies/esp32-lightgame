#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sim.h"
#include "sweep.h"

typedef struct {
    sim_phase_t phase;
    int score;
    int high_score;
    int combo;
    sim_settings_t settings;
    float enemy_speed;
    uint32_t spawn_interval_ms;
    sim_color_t leak_color;
    uint32_t controls;
    uint32_t clock_ms;
    uint32_t phase_clock_ms;
    uint32_t kill_flash_ms;
    float last_kill_x;
    sim_color_t last_shot;
    bool has_last_shot;
    const sim_mover_t *enemies;
    const sim_mover_t *bullets;
} present_in_t;

void present_fill(const present_in_t *in, sim_snapshot_t *out);
