#include "persist.h"

#include "nvs.h"
#include "nvs_flash.h"

#define NVS_NS "lightgame"

sim_persist_t persist_load(void)
{
    sim_persist_t p = sim_persist_defaults();
    nvs_handle_t nvs;
    if (nvs_open(NVS_NS, NVS_READONLY, &nvs) != ESP_OK) {
        return p;
    }

    uint16_t led_count = p.settings.led_count;
    if (nvs_get_u16(nvs, "led_count", &led_count) == ESP_OK) {
        p.settings.led_count = led_count;
    }
    uint8_t brightness = p.settings.brightness;
    if (nvs_get_u8(nvs, "brightness", &brightness) == ESP_OK) {
        p.settings.brightness = brightness;
    }
    uint32_t spawn_ms = p.settings.spawn_interval_ms;
    if (nvs_get_u32(nvs, "spawn_ms", &spawn_ms) == ESP_OK) {
        p.settings.spawn_interval_ms = spawn_ms;
    }
    int32_t enemy_speed_x10 = (int32_t)(p.settings.enemy_speed * 10.0f);
    if (nvs_get_i32(nvs, "enemy_spd", &enemy_speed_x10) == ESP_OK) {
        p.settings.enemy_speed = (float)enemy_speed_x10 / 10.0f;
    }
    int32_t bullet_speed_x10 = (int32_t)(p.settings.bullet_speed * 10.0f);
    if (nvs_get_i32(nvs, "bullet_spd", &bullet_speed_x10) == ESP_OK) {
        p.settings.bullet_speed = (float)bullet_speed_x10 / 10.0f;
    }
    int32_t high = 0;
    if (nvs_get_i32(nvs, "high_score", &high) == ESP_OK && high > 0) {
        p.high_score = (int)high;
    }
    nvs_close(nvs);
    return p;
}

void persist_save(sim_persist_t persist)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NS, NVS_READWRITE, &nvs) != ESP_OK) {
        return;
    }
    nvs_set_u16(nvs, "led_count", persist.settings.led_count);
    nvs_set_u8(nvs, "brightness", persist.settings.brightness);
    nvs_set_u32(nvs, "spawn_ms", persist.settings.spawn_interval_ms);
    nvs_set_i32(nvs, "enemy_spd", (int32_t)(persist.settings.enemy_speed * 10.0f));
    nvs_set_i32(nvs, "bullet_spd", (int32_t)(persist.settings.bullet_speed * 10.0f));
    nvs_set_i32(nvs, "high_score", (int32_t)persist.high_score);
    nvs_commit(nvs);
    nvs_close(nvs);
}
