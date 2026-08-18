# ESP32 Light Strip Shooter

A 1-D arcade shooter on a WS2812 LED strip. Enemies spawn at the far end in red, green, or blue and march toward your end. Shoot the matching color before they reach you.

The game runs fully on the ESP32 with three physical buttons. A local web UI adds stats, settings, and virtual buttons.

## Wiring

| Component | Connection |
|-----------|------------|
| LED strip red wire | ESP32 **5V** (use external 5V supply for long strips) |
| LED strip white wire | **GND** |
| LED strip green wire (data) | **GPIO 18** |
| Red shoot button | **GPIO 13** to GND (uses internal pull-up) |
| Green shoot button | **GPIO 12** to GND |
| Blue shoot button | **GPIO 14** to GND |

Each button is one side on the GPIO pin and the other on GND. Pressing pulls the pin low.

```
ESP32                         LED Strip (WS2812)
-----                         ------------------
GPIO 18 --------------------> Green (DIN)
5V      --------------------> Red (+5V)
GND     --------------------> White (GND)

GPIO 13 ---[ Red Button ]--- GND
GPIO 12 ---[ Green Button ]-- GND
GPIO 14 ---[ Blue Button ]--- GND
```

## How to play (arcade mode, no web)

1. Power on. The first LEDs pulse while idle.
2. Press any color button to start.
3. Press the button that matches the enemy color to fire from your end (LED 0).
4. Each kill adds one point and speeds up spawns.
5. If an enemy reaches your end, game over. The strip flashes red, then shows your score as blue LEDs.
6. Press any button to start again.

## Web control

1. Connect to WiFi AP **ESP32-LightGame** (password **12345678**).
2. Open **http://192.168.4.1/** in a browser.
3. Use virtual shoot buttons, start/pause/reset, and change LED count, brightness, and difficulty.

The web UI does not block arcade play. Physical buttons always work.

## Build and flash

Requires [PlatformIO](https://platformio.org/).

```bash
pio run -t upload
pio device monitor
```

Adjust `LED_STRIP_DEFAULT_COUNT` and GPIO pins in `include/config.h` if needed.

## Pin summary

| Function | GPIO |
|----------|------|
| LED data | 18 |
| Button red | 13 |
| Button green | 12 |
| Button blue | 14 |
