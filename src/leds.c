#include "leds.h"

#include "esp_log.h"
#include "led_strip.h"

#include "config.h"

static const char *TAG = "leds";

static led_strip_handle_t s_strip;

bool leds_init(uint16_t count)
{
    (void)count;

    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = LED_STRIP_MAX_COUNT,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags =
            {
                .invert_out = false,
            },
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags =
            {
                .with_dma = false,
            },
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led strip init failed: %s", esp_err_to_name(err));
        return false;
    }

    led_strip_clear(s_strip);
    led_strip_refresh(s_strip);
    ESP_LOGI(TAG, "LED strip ready on GPIO %d", LED_STRIP_GPIO);
    return true;
}

void leds_show(const sim_snapshot_t *snap)
{
    if (!s_strip || !snap) {
        return;
    }

    led_strip_clear(s_strip);
    uint16_t n = snap->led_count;
    if (n > LED_STRIP_MAX_COUNT) {
        n = LED_STRIP_MAX_COUNT;
    }
    for (uint16_t i = 0; i < n; i++) {
        led_strip_set_pixel(s_strip, i, snap->pixels[i].r, snap->pixels[i].g, snap->pixels[i].b);
    }
    led_strip_refresh(s_strip);
}
