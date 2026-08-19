#include "sweep.h"

#include <stdlib.h>
#include <string.h>

#define SWEEP_EVENT_MAX (MAX_BULLETS * MAX_ENEMIES + MAX_ENEMIES + MAX_BULLETS)

typedef enum {
    EV_CONTACT = 0,
    EV_LEAK = 1,
    EV_EXIT = 2,
} ev_kind_t;

typedef struct {
    float x0;
    float x1;
    sim_color_t color;
    bool live;
} flight_t;

typedef struct {
    float tau;
    ev_kind_t kind;
    uint8_t bullet;
    uint8_t enemy;
} event_t;

static int ev_cmp(const void *a, const void *b)
{
    const event_t *ea = (const event_t *)a;
    const event_t *eb = (const event_t *)b;
    if (ea->tau < eb->tau) {
        return -1;
    }
    if (ea->tau > eb->tau) {
        return 1;
    }
    if (ea->kind < eb->kind) {
        return -1;
    }
    if (ea->kind > eb->kind) {
        return 1;
    }
    if (ea->enemy < eb->enemy) {
        return -1;
    }
    if (ea->enemy > eb->enemy) {
        return 1;
    }
    if (ea->bullet < eb->bullet) {
        return -1;
    }
    if (ea->bullet > eb->bullet) {
        return 1;
    }
    return 0;
}

static float clampf(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

void sweep_advance(sweep_in_t *in, sweep_out_t *out)
{
    memset(out, 0, sizeof(*out));
    if (!in) {
        return;
    }

    flight_t efl[MAX_ENEMIES];
    flight_t bfl[MAX_BULLETS];
    memset(efl, 0, sizeof(efl));
    memset(bfl, 0, sizeof(bfl));

    const float dt = in->dt_sec;
    const float ve = in->enemy_speed;
    const float vb = in->bullet_speed;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        const sim_mover_t *e = &in->enemies[i];
        efl[i].live = e->live;
        efl[i].color = e->color;
        efl[i].x0 = e->x;
        efl[i].x1 = e->x - ve * dt;
    }
    for (int i = 0; i < MAX_BULLETS; i++) {
        const sim_mover_t *b = &in->bullets[i];
        bfl[i].live = b->live;
        bfl[i].color = b->color;
        bfl[i].x0 = b->x;
        bfl[i].x1 = b->x + vb * dt;
    }

    event_t ev[SWEEP_EVENT_MAX];
    int ne = 0;

    if (dt > 0.0f) {
        for (int bi = 0; bi < MAX_BULLETS; bi++) {
            if (!bfl[bi].live) {
                continue;
            }
            for (int ei = 0; ei < MAX_ENEMIES; ei++) {
                if (!efl[ei].live) {
                    continue;
                }
                const float closing = vb + ve;
                if (closing <= 0.0f) {
                    continue;
                }
                const float tau = (efl[ei].x0 - bfl[bi].x0) / (closing * dt);
                if (tau >= 0.0f && tau <= 1.0f && ne < SWEEP_EVENT_MAX) {
                    ev[ne].tau = tau;
                    ev[ne].kind = EV_CONTACT;
                    ev[ne].bullet = (uint8_t)bi;
                    ev[ne].enemy = (uint8_t)ei;
                    ne++;
                }
            }
        }

        for (int ei = 0; ei < MAX_ENEMIES; ei++) {
            if (!efl[ei].live) {
                continue;
            }
            if (efl[ei].x1 <= in->x_min && ne < SWEEP_EVENT_MAX) {
                const float denom = ve * dt;
                float tau = denom > 0.0f ? (efl[ei].x0 - in->x_min) / denom : 0.0f;
                tau = clampf(tau, 0.0f, 1.0f);
                ev[ne].tau = tau;
                ev[ne].kind = EV_LEAK;
                ev[ne].bullet = 0;
                ev[ne].enemy = (uint8_t)ei;
                ne++;
            }
        }

        for (int bi = 0; bi < MAX_BULLETS; bi++) {
            if (!bfl[bi].live) {
                continue;
            }
            if (bfl[bi].x1 >= in->x_max && ne < SWEEP_EVENT_MAX) {
                const float denom = vb * dt;
                float tau = denom > 0.0f ? (in->x_max - bfl[bi].x0) / denom : 0.0f;
                tau = clampf(tau, 0.0f, 1.0f);
                ev[ne].tau = tau;
                ev[ne].kind = EV_EXIT;
                ev[ne].bullet = (uint8_t)bi;
                ev[ne].enemy = 0;
                ne++;
            }
        }
    }

    qsort(ev, (size_t)ne, sizeof(event_t), ev_cmp);

    bool enemy_dead[MAX_ENEMIES];
    bool bullet_dead[MAX_BULLETS];
    memset(enemy_dead, 0, sizeof(enemy_dead));
    memset(bullet_dead, 0, sizeof(bullet_dead));

    bool stop = false;
    for (int k = 0; k < ne && !stop; k++) {
        const event_t *e = &ev[k];
        switch (e->kind) {
        case EV_CONTACT:
            if (enemy_dead[e->enemy] || bullet_dead[e->bullet]) {
                break;
            }
            if (!efl[e->enemy].live || !bfl[e->bullet].live) {
                break;
            }
            bullet_dead[e->bullet] = true;
            if (efl[e->enemy].color == bfl[e->bullet].color) {
                enemy_dead[e->enemy] = true;
                out->kills++;
                out->last_kill_x = efl[e->enemy].x0 + (efl[e->enemy].x1 - efl[e->enemy].x0) * e->tau;
            } else {
                out->absorbs++;
            }
            break;
        case EV_LEAK:
            if (enemy_dead[e->enemy] || !efl[e->enemy].live) {
                break;
            }
            enemy_dead[e->enemy] = true;
            out->leaks++;
            out->leak_color = efl[e->enemy].color;
            out->leaked = true;
            stop = true;
            break;
        case EV_EXIT:
            if (bullet_dead[e->bullet] || !bfl[e->bullet].live) {
                break;
            }
            bullet_dead[e->bullet] = true;
            out->exits++;
            break;
        }
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!in->enemies[i].live) {
            continue;
        }
        if (enemy_dead[i]) {
            in->enemies[i].live = false;
        } else {
            in->enemies[i].x = efl[i].x1;
        }
    }
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!in->bullets[i].live) {
            continue;
        }
        if (bullet_dead[i]) {
            in->bullets[i].live = false;
        } else {
            in->bullets[i].x = bfl[i].x1;
        }
    }
}
