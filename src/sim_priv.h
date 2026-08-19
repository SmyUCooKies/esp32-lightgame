#pragma once

#include "sim.h"
#include "sweep.h"

struct sim {
    sim_phase_t phase;
    sim_settings_t settings;
    sim_rng_t rng;
    int score;
    int high_score;
    int combo;
    uint32_t clock_ms;
    uint32_t phase_clock_ms;
    uint32_t spawn_age_ms;
    bool spawn_due;
    sim_color_t leak_color;
    sim_color_t last_shot;
    bool has_last_shot;
    float last_kill_x;
    uint32_t kill_flash_ms;
    sim_mover_t enemies[MAX_ENEMIES];
    sim_mover_t bullets[MAX_BULLETS];
};
