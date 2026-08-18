#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "game.h"

bool leds_init(uint16_t count);
void leds_render(const game_t *game);
void leds_show_idle(uint32_t now_ms);
void leds_show_game_over(uint32_t now_ms, int score);
void leds_set_count(uint16_t count);
