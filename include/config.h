#pragma once

#define LED_STRIP_GPIO       18
#define LED_STRIP_MAX_COUNT  60
#define LED_STRIP_DEFAULT_COUNT 30

#define BTN_RED_GPIO    13
#define BTN_GREEN_GPIO  12
#define BTN_BLUE_GPIO   14

#define WIFI_AP_SSID     "ESP32-LightGame"
#define WIFI_AP_PASSWORD "12345678"
#define WIFI_AP_CHANNEL  1
#define WIFI_AP_MAX_CONN 4

#define GAME_TICK_MS         16
#define MAX_ENEMIES          16
#define MAX_BULLETS          8
#define DEBOUNCE_MS          50

#define DEFAULT_ENEMY_SPEED       2.0f
#define DEFAULT_BULLET_SPEED     24.0f
#define DEFAULT_SPAWN_INTERVAL_MS 2500
#define DEFAULT_BRIGHTNESS         64
