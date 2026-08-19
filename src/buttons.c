#include "buttons.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include "cmdq.h"
#include "config.h"
#include "sim.h"

static const char *TAG = "buttons";

static const gpio_num_t s_btn_pins[SIM_COLOR_COUNT] = {
    BTN_RED_GPIO,
    BTN_GREEN_GPIO,
    BTN_BLUE_GPIO,
};

static uint32_t s_last_press_ms[SIM_COLOR_COUNT];
static bool s_was_pressed[SIM_COLOR_COUNT];

void buttons_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask =
            (1ULL << BTN_RED_GPIO) | (1ULL << BTN_GREEN_GPIO) | (1ULL << BTN_BLUE_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    ESP_LOGI(TAG, "Buttons on GPIO %d (red), %d (green), %d (blue)", BTN_RED_GPIO, BTN_GREEN_GPIO,
             BTN_BLUE_GPIO);
}

void buttons_poll(uint32_t now_ms)
{
    for (int i = 0; i < SIM_COLOR_COUNT; i++) {
        bool pressed = gpio_get_level(s_btn_pins[i]) == 0;
        if (pressed && !s_was_pressed[i]) {
            if (now_ms - s_last_press_ms[i] >= DEBOUNCE_MS) {
                s_last_press_ms[i] = now_ms;
                cmdq_push(sim_cmd_shoot((sim_color_t)i));
            }
        }
        s_was_pressed[i] = pressed;
    }
}
