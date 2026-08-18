#include "leds.h"

#include <math.h>
#include <string.h>

#include "esp_log.h"
#include "led_strip.h"

static const char *TAG = "leds";

static led_strip_handle_t s_strip;
static uint16_t s_led_count = LED_STRIP_DEFAULT_COUNT;

static void scale_rgb(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t brightness)
{
    *r = (uint8_t)((uint16_t)(*r) * brightness / 255);
    *g = (uint8_t)((uint16_t)(*g) * brightness / 255);
    *b = (uint8_t)((uint16_t)(*b) * brightness / 255);
}

bool leds_init(uint16_t count)
{
    if (count < 10) {
        count = 10;
    }
    if (count > LED_STRIP_MAX_COUNT) {
        count = LED_STRIP_MAX_COUNT;
    }
    s_led_count = count;

    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = LED_STRIP_MAX_COUNT,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags = {
            .invert_out = false,
        },
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags = {
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
    ESP_LOGI(TAG, "LED strip ready, %u LEDs on GPIO %d", s_led_count, LED_STRIP_GPIO);
    return true;
}

void leds_set_count(uint16_t count)
{
    if (count >= 10 && count <= LED_STRIP_MAX_COUNT) {
        s_led_count = count;
    }
}

void leds_show_idle(uint32_t now_ms)
{
    led_strip_clear(s_strip);
    float pulse = (sinf((float)now_ms / 400.0f) + 1.0f) / 2.0f;
    uint8_t v = (uint8_t)(20 + pulse * 40);
    led_strip_set_pixel(s_strip, 0, v, v, v);
    led_strip_set_pixel(s_strip, 1, v / 3, v / 3, v);
    led_strip_refresh(s_strip);
}

void leds_show_game_over(uint32_t now_ms, int score)
{
    uint32_t phase = (now_ms / 300) % 6;
    led_strip_clear(s_strip);

    if (phase < 3) {
        for (int i = 0; i < s_led_count; i++) {
            led_strip_set_pixel(s_strip, i, 80, 0, 0);
        }
    } else {
        int flashes = score;
        if (flashes > s_led_count) {
            flashes = s_led_count;
        }
        for (int i = 0; i < flashes; i++) {
            led_strip_set_pixel(s_strip, i, 0, 0, 60);
        }
        led_strip_set_pixel(s_strip, 0, 60, 60, 60);
    }

    led_strip_refresh(s_strip);
}

void leds_render(const game_t *game)
{
    const uint8_t brightness = game->settings.brightness;
    led_strip_clear(s_strip);

    uint8_t br, bg, bb;
    game_get_rgb(GAME_COLOR_RED, &br, &bg, &bb);
    scale_rgb(&br, &bg, &bb, 8);
    led_strip_set_pixel(s_strip, 0, br, bg, bb);

    for (int i = 0; i < MAX_ENEMIES; i++) {
        const enemy_t *e = &game->enemies[i];
        if (!e->active) {
            continue;
        }
        int idx = (int)(e->pos + 0.5f);
        if (idx < 0 || idx >= s_led_count) {
            continue;
        }
        uint8_t r, g, b;
        game_get_rgb(e->color, &r, &g, &b);
        scale_rgb(&r, &g, &b, brightness);
        led_strip_set_pixel(s_strip, idx, r, g, b);
    }

    for (int i = 0; i < MAX_BULLETS; i++) {
        const bullet_t *b = &game->bullets[i];
        if (!b->active) {
            continue;
        }
        int idx = (int)(b->pos + 0.5f);
        if (idx < 0 || idx >= s_led_count) {
            continue;
        }
        uint8_t r, g, bl;
        game_get_rgb(b->color, &r, &g, &bl);
        scale_rgb(&r, &g, &bl, brightness);
        r = r > 180 ? 255 : r + 75;
        g = g > 180 ? 255 : g + 75;
        bl = bl > 180 ? 255 : bl + 75;
        led_strip_set_pixel(s_strip, idx, r, g, bl);
    }

    if (game->kill_flash) {
        uint8_t r, g, b;
        game_get_rgb(game->kill_flash_color, &r, &g, &b);
        scale_rgb(&r, &g, &b, 255);
        if (game->kill_flash_pos >= 0 && game->kill_flash_pos < s_led_count) {
            led_strip_set_pixel(s_strip, game->kill_flash_pos, r, g, b);
        }
    }

    led_strip_refresh(s_strip);
}
