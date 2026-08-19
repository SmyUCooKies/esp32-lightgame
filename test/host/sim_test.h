#pragma once

#include <string.h>

#include "sim.h"
#include "sim_priv.h"

static inline void sim_test_clear_movers(sim_t *s)
{
    memset(s->enemies, 0, sizeof(s->enemies));
    memset(s->bullets, 0, sizeof(s->bullets));
}

static inline bool sim_test_place_enemy(sim_t *s, sim_color_t c, float x)
{
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!s->enemies[i].live) {
            s->enemies[i].live = true;
            s->enemies[i].color = c;
            s->enemies[i].x = x;
            return true;
        }
    }
    return false;
}

static inline bool sim_test_place_bullet(sim_t *s, sim_color_t c, float x)
{
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!s->bullets[i].live) {
            s->bullets[i].live = true;
            s->bullets[i].color = c;
            s->bullets[i].x = x;
            return true;
        }
    }
    return false;
}

static inline int sim_test_enemy_count(const sim_t *s)
{
    int n = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (s->enemies[i].live) {
            n++;
        }
    }
    return n;
}

static inline int sim_test_bullet_count(const sim_t *s)
{
    int n = 0;
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (s->bullets[i].live) {
            n++;
        }
    }
    return n;
}

static inline float sim_test_first_enemy_x(const sim_t *s)
{
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (s->enemies[i].live) {
            return s->enemies[i].x;
        }
    }
    return 0.0f;
}
