#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sim.h"

typedef struct {
    bool live;
    sim_color_t color;
    float x;
} sim_mover_t;

typedef struct {
    sim_mover_t *enemies;
    sim_mover_t *bullets;
    float dt_sec;
    float enemy_speed;
    float bullet_speed;
    float x_min;
    float x_max;
} sweep_in_t;

typedef struct {
    int kills;
    int absorbs;
    int leaks;
    int exits;
    float last_kill_x;
    sim_color_t leak_color;
    bool leaked;
} sweep_out_t;

void sweep_advance(sweep_in_t *in, sweep_out_t *out);
