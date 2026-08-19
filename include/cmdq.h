#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "sim.h"

#define CMDQ_LEN 16

bool cmdq_init(void);
bool cmdq_push(sim_cmd_t cmd);
size_t cmdq_drain(sim_cmd_t *out, size_t max);

void snapshot_publish(const sim_snapshot_t *snap);
void snapshot_copy(sim_snapshot_t *out);
