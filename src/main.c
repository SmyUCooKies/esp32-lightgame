#include <stdio.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "buttons.h"
#include "cmdq.h"
#include "config.h"
#include "leds.h"
#include "persist.h"
#include "sim.h"
#include "web_server.h"

static const char *TAG = "main";

static sim_storage_t s_storage;
static sim_t *s_sim;

static uint32_t rng_next(void *ctx)
{
    (void)ctx;
    return esp_random();
}

static void wifi_init_ap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap =
            {
                .ssid = WIFI_AP_SSID,
                .ssid_len = 0,
                .password = WIFI_AP_PASSWORD,
                .channel = WIFI_AP_CHANNEL,
                .max_connection = WIFI_AP_MAX_CONN,
                .authmode = WIFI_AUTH_WPA2_PSK,
            },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP started. SSID: %s  password: %s", WIFI_AP_SSID, WIFI_AP_PASSWORD);
    ESP_LOGI(TAG, "Open http://192.168.4.1/ in a browser");
}

static void game_task(void *arg)
{
    (void)arg;
    uint32_t last_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    sim_cmd_t batch[CMDQ_LEN];
    sim_out_t out;

    while (true) {
        uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t dt_ms = now_ms - last_ms;
        if (dt_ms > 100) {
            dt_ms = 100;
        }
        last_ms = now_ms;

        buttons_poll(now_ms);
        size_t n = cmdq_drain(batch, CMDQ_LEN);
        sim_step(s_sim, dt_ms, batch, n, &out);
        snapshot_publish(&out.snapshot);
        leds_show(&out.snapshot);
        if (out.persist_dirty) {
            persist_save(sim_persist_of(s_sim));
        }
        vTaskDelay(pdMS_TO_TICKS(GAME_TICK_MS));
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    if (!cmdq_init()) {
        ESP_LOGE(TAG, "cmdq init failed");
        return;
    }

    sim_persist_t persist = persist_load();
    s_sim = sim_from_storage(&s_storage);
    sim_rng_t rng = {.next_u32 = rng_next, .ctx = NULL};
    sim_snapshot_t boot = sim_boot(s_sim, &persist, rng, persist.settings.led_count);
    snapshot_publish(&boot);

    if (!leds_init(boot.led_count)) {
        ESP_LOGE(TAG, "LED init failed");
        return;
    }
    leds_show(&boot);

    buttons_init();
    wifi_init_ap();
    web_server_start();

    xTaskCreate(game_task, "game", 8192, NULL, 5, NULL);
    ESP_LOGI(TAG, "Light strip shooter ready");
}
