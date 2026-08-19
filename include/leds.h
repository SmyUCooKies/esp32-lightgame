#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sim.h"

bool leds_init(uint16_t count);
void leds_show(const sim_snapshot_t *snap);
