# ESP32 light strip shooter

A 1-D arcade shooter on a WS2812 strip. Enemies spawn at the far end in red, green, or blue and walk toward your end. Shoot the matching color before they reach LED 0.

The cabinet plays with three buttons and no network. The SoftAP page adds stats, settings, a strip preview, and keyboard controls.

## Wiring

| Component | Connection |
|-----------|------------|
| LED strip red wire | ESP32 5V. Use an external 5V supply for long strips. |
| LED strip white wire | GND |
| LED strip green wire (data) | GPIO 18 |
| Red shoot button | GPIO 13 to GND. The pin uses the internal pull-up. |
| Green shoot button | GPIO 12 to GND |
| Blue shoot button | GPIO 14 to GND |

Each button sits between its GPIO pin and GND. A press pulls the pin low.

```
ESP32                         LED strip (WS2812)
-----                         ------------------
GPIO 18 --------------------> Green (DIN)
5V      --------------------> Red (+5V)
GND     --------------------> White (GND)

GPIO 13 ---[ Red button ]--- GND
GPIO 12 ---[ Green button ]-- GND
GPIO 14 ---[ Blue button ]--- GND
```

Change pins or the default LED count in `include/config.h`. The strip cap is 60 LEDs. The default is 30.

## How to play

1. Power the board. The strip shows a slow RGB march while idle.
2. Press any color button to start. The first enemy appears at once.
3. Press the button that matches the enemy color. Shots leave LED 0.
4. A matching hit scores one point and raises difficulty. A mismatch eats the shot. The enemy keeps walking.
5. If an enemy reaches LED 0, the run ends. The strip flashes the leaked color, then shows the score in groups of five.
6. Press any color button to start again.

Pause exists on the web page. Physical buttons only shoot.

## How to use the web page

1. Join WiFi AP `ESP32-LightGame`. The password is `12345678`.
2. Open `http://192.168.4.1/` in a browser.
3. Use the on-page shoot, start, pause, resume, and reset buttons.
4. Press `1` `2` `3` or `Z` `X` `C` to shoot red, green, and blue.
5. Press Space to pause or resume from the last seen phase.
6. Drag the sliders, including bullet speed, then click **Save settings**. Polling does not overwrite a slider you are dragging.

The page is optional. Buttons keep working if no client is connected.

## How to build and flash

Install [PlatformIO](https://platformio.org/). In `esp32-lightgame` run:

```bash
pio run -t upload
pio device monitor
```

If `pio` is not on PATH, run the `pio.exe` inside the PlatformIO penv `Scripts` folder.

## How to run host tests

The rules compile on the host with no ESP-IDF. From `esp32-lightgame` run:

```bash
clang -std=c11 -Wall -Wextra -Werror -Iinclude -Isrc test/host/test_sim.c src/sim.c src/sweep.c src/present.c -o ../.tmp/test_sim.exe
../.tmp/test_sim.exe
```

On Windows with MSVC as the linker, omit `-lm`. The CRT already provides the math functions.

## Pin summary

| Function | GPIO |
|----------|------|
| LED data | 18 |
| Button red | 13 |
| Button green | 12 |
| Button blue | 14 |
