#include "cmdq.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

static QueueHandle_t s_cmds;
static SemaphoreHandle_t s_snap_mu;
static sim_snapshot_t s_snap;

bool cmdq_init(void)
{
    s_cmds = xQueueCreate(CMDQ_LEN, sizeof(sim_cmd_t));
    s_snap_mu = xSemaphoreCreateMutex();
    return s_cmds && s_snap_mu;
}

bool cmdq_push(sim_cmd_t cmd)
{
    if (!s_cmds) {
        return false;
    }
    return xQueueSend(s_cmds, &cmd, 0) == pdTRUE;
}

size_t cmdq_drain(sim_cmd_t *out, size_t max)
{
    size_t n = 0;
    if (!s_cmds || !out) {
        return 0;
    }
    while (n < max && xQueueReceive(s_cmds, &out[n], 0) == pdTRUE) {
        n++;
    }
    return n;
}

void snapshot_publish(const sim_snapshot_t *snap)
{
    if (!snap || !s_snap_mu) {
        return;
    }
    xSemaphoreTake(s_snap_mu, portMAX_DELAY);
    memcpy(&s_snap, snap, sizeof(s_snap));
    xSemaphoreGive(s_snap_mu);
}

void snapshot_copy(sim_snapshot_t *out)
{
    if (!out || !s_snap_mu) {
        return;
    }
    xSemaphoreTake(s_snap_mu, portMAX_DELAY);
    memcpy(out, &s_snap, sizeof(s_snap));
    xSemaphoreGive(s_snap_mu);
}
