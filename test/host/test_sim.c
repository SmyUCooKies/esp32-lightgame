#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim.h"
#include "sim_test.h"

#define CHECK(cond, msg)                                                                                               \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            fprintf(stderr, "FAIL: %s\n", msg);                                                                        \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

static uint32_t rng_zero(void *ctx)
{
    (void)ctx;
    return 0;
}

static sim_t *boot_red(sim_storage_t *storage)
{
    sim_t *sim = sim_from_storage(storage);
    sim_rng_t rng = {.next_u32 = rng_zero, .ctx = NULL};
    sim_persist_t persist = sim_persist_defaults();
    (void)sim_boot(sim, &persist, rng, LED_STRIP_DEFAULT_COUNT);
    return sim;
}

static void step_one(sim_t *sim, uint32_t dt_ms, sim_cmd_t cmd, sim_out_t *out)
{
    sim_step(sim, dt_ms, &cmd, 1, out);
}

static void test_immediate_spawn(void)
{
    sim_storage_t storage;
    sim_t *sim = boot_red(&storage);
    sim_out_t out;
    step_one(sim, 0, sim_cmd_start(), &out);
    CHECK(out.snapshot.phase == SIM_PHASE_PLAYING, "start enters playing");
    CHECK(out.snapshot.enemy_count == 1, "immediate spawn");
    CHECK(sim_test_enemy_count(sim) == 1, "one live enemy after start");
}

static void test_matching_kill_skip_through(void)
{
    sim_storage_t storage;
    sim_t *sim = boot_red(&storage);
    sim_out_t out;
    step_one(sim, 0, sim_cmd_start(), &out);
    sim_test_clear_movers(sim);
    CHECK(sim_test_place_enemy(sim, SIM_COLOR_RED, 12.0f), "place enemy");
    CHECK(sim_test_place_bullet(sim, SIM_COLOR_RED, 0.0f), "place bullet");
    sim_step(sim, 1000, NULL, 0, &out);
    CHECK(out.snapshot.score == 1, "matching skip-through scores 1");
    CHECK(out.snapshot.enemy_count == 0, "enemy gone after match");
    CHECK(out.snapshot.bullet_count == 0, "bullet gone after match");
}

static void test_mismatch_absorb(void)
{
    sim_storage_t storage;
    sim_t *sim = boot_red(&storage);
    sim_out_t out;
    step_one(sim, 0, sim_cmd_start(), &out);
    sim_test_clear_movers(sim);
    CHECK(sim_test_place_enemy(sim, SIM_COLOR_RED, 12.0f), "place enemy");
    CHECK(sim_test_place_bullet(sim, SIM_COLOR_GREEN, 0.0f), "place bullet");
    sim_step(sim, 1000, NULL, 0, &out);
    CHECK(out.snapshot.score == 0, "mismatch does not score");
    CHECK(out.snapshot.enemy_count == 1, "enemy survives mismatch");
    CHECK(out.snapshot.bullet_count == 0, "bullet absorbed");
    float x = sim_test_first_enemy_x(sim);
    CHECK(x > 9.0f && x < 11.0f, "enemy continued past contact");
}

static void test_leak_color(void)
{
    sim_storage_t storage;
    sim_t *sim = boot_red(&storage);
    sim_out_t out;
    step_one(sim, 0, sim_cmd_start(), &out);
    sim_test_clear_movers(sim);
    CHECK(sim_test_place_enemy(sim, SIM_COLOR_BLUE, 0.2f), "place leaking enemy");
    sim_step(sim, 200, NULL, 0, &out);
    CHECK(out.snapshot.phase == SIM_PHASE_GAME_OVER, "leak ends the run");
    CHECK(out.snapshot.leak_color == SIM_COLOR_BLUE, "leak color recorded");
}

static void test_difficulty_after_ten_kills(void)
{
    sim_storage_t storage;
    sim_t *sim = boot_red(&storage);
    sim_out_t out;
    step_one(sim, 0, sim_cmd_start(), &out);
    CHECK(out.snapshot.enemy_count == 1, "first enemy present");

    for (int i = 0; i < 10; i++) {
        step_one(sim, 2000, sim_cmd_shoot(SIM_COLOR_RED), &out);
        CHECK(out.snapshot.score == i + 1, "kill via shoot and step");
        if (i < 9) {
            sim_step(sim, out.snapshot.spawn_interval_ms, NULL, 0, &out);
            CHECK(out.snapshot.enemy_count >= 1, "next enemy spawned");
        }
    }

    CHECK(out.snapshot.score == 10, "ten kills");
    CHECK(fabsf(out.snapshot.enemy_speed - (DEFAULT_ENEMY_SPEED + 10.0f * 0.12f)) < 0.001f,
          "enemy speed after 10 kills");
    CHECK(out.snapshot.spawn_interval_ms == (DEFAULT_SPAWN_INTERVAL_MS - 10u * 45u),
          "spawn interval after 10 kills");
}

static void test_pause_does_not_move(void)
{
    sim_storage_t storage;
    sim_t *sim = boot_red(&storage);
    sim_out_t out;
    step_one(sim, 0, sim_cmd_start(), &out);
    sim_test_clear_movers(sim);
    CHECK(sim_test_place_enemy(sim, SIM_COLOR_RED, 15.0f), "place enemy");
    step_one(sim, 0, sim_cmd_set_paused(true), &out);
    CHECK(out.snapshot.phase == SIM_PHASE_PAUSED, "paused");
    sim_step(sim, 1000, NULL, 0, &out);
    CHECK(fabsf(sim_test_first_enemy_x(sim) - 15.0f) < 0.0001f, "pause does not move");
    CHECK(out.snapshot.phase == SIM_PHASE_PAUSED, "still paused");
}

static void test_shoot_from_idle(void)
{
    sim_storage_t storage;
    sim_t *sim = boot_red(&storage);
    sim_out_t out;
    CHECK(sim_test_enemy_count(sim) == 0, "idle has no enemies");
    step_one(sim, 0, sim_cmd_shoot(SIM_COLOR_RED), &out);
    CHECK(out.snapshot.phase == SIM_PHASE_PLAYING, "shoot-from-idle starts");
    CHECK(out.snapshot.enemy_count == 1, "shoot-from-idle spawns");
    CHECK(out.snapshot.bullet_count == 1, "shoot-from-idle fires");
}

int main(void)
{
    test_immediate_spawn();
    test_matching_kill_skip_through();
    test_mismatch_absorb();
    test_leak_color();
    test_difficulty_after_ten_kills();
    test_pause_does_not_move();
    test_shoot_from_idle();
    printf("ok\n");
    return 0;
}
