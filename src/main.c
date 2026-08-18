#include <stdio.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "buttons.h"
#include "config.h"
#include "game.h"
#include "leds.h"
#include "web_server.h"

static const char *TAG = "main";

static void wifi_init_ap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
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
    uint32_t last_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    while (true) {
        uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        float dt = (float)(now_ms - last_ms) / 1000.0f;
        if (dt > 0.1f) {
            dt = 0.1f;
        }
        last_ms = now_ms;

        buttons_poll(now_ms);
        game_tick(now_ms, dt);

        game_t *g = game_lock();
        leds_set_count(g->settings.led_count);

        switch (g->phase) {
        case GAME_PHASE_IDLE:
            leds_show_idle(now_ms);
            break;
        case GAME_PHASE_PLAYING:
        case GAME_PHASE_PAUSED:
            leds_render(g);
            break;
        case GAME_PHASE_GAME_OVER:
            leds_show_game_over(now_ms, g->score);
            break;
        }

        game_unlock();
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

    game_init();

    const game_settings_t *settings = game_get_settings();
    if (!leds_init(settings->led_count)) {
        ESP_LOGE(TAG, "LED init failed");
        return;
    }

    buttons_init();
    wifi_init_ap();
    web_server_start();

    xTaskCreate(game_task, "game", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Light strip shooter ready");
}
