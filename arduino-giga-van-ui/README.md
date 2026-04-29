# Arduino GIGA Van UI

This project is a touchscreen application for an `Arduino GIGA R1 WiFi` with a `GIGA Display Shield`.

It includes:

- A temperature tab that uses your `vanVector_1.png` artwork as the background and overlays temperature values in the existing box locations.
- A furnace tab that acts like a simple thermostat and drives one digital output for your triac thermostat interface.
- A lights tab with `Red`, `Green`, `Blue`, and `Brightness` sliders.
- An auto-sleep toggle that turns the display backlight off after 1 minute until the screen is touched again.
- An `LVGL 8` user interface running through `Arduino_H7_Video`.

## Files

- [arduino-giga-van-ui.ino](/Users/jabbott/Library/CloudStorage/Dropbox/ShareToJohn-NotAnyMore/The BV/programming/codex1/arduino-giga-van-ui/arduino-giga-van-ui.ino)
- [van_background.h](/Users/jabbott/Library/CloudStorage/Dropbox/ShareToJohn-NotAnyMore/The BV/programming/codex1/arduino-giga-van-ui/van_background.h)
- [tools/png_to_rgb565.py](/Users/jabbott/Library/CloudStorage/Dropbox/ShareToJohn-NotAnyMore/The BV/programming/codex1/arduino-giga-van-ui/tools/png_to_rgb565.py)

## Required Arduino Libraries

Install these with Arduino IDE Library Manager:

- `lvgl` version `8.x`
- `Arduino_GigaDisplay`
- `Arduino_GigaDisplayTouch`
- `DallasTemperature`
- `OneWire`

You also need the `Arduino Mbed OS Giga Boards` core installed in Board Manager.

This version was compile-checked with:

- `arduino-cli 1.4.1`
- `arduino:mbed_giga 4.4.1`
- `lvgl 8.3.11`

## Default Wiring

- `DS18B20` data bus on `D2`
- Furnace control output on `D4`
- LED PWM outputs on `D6`, `D7`, `D8`

For the DS18B20 bus, use a `4.7k` pull-up resistor from the data line to `3.3V`.

## Important Notes

- The current sketch uses `Arduino_H7_Video` so the display is driven through LVGL instead of the previous hand-drawn GFX layout.
- The temperature tab uses your `vanVector_1.png` artwork as an embedded `RGB565` background image.
- If you update the van artwork, regenerate `van_background.h` with:
  `python3 tools/png_to_rgb565.py "/Users/jabbott/Library/CloudStorage/Dropbox/ShareToJohn-NotAnyMore/The BV/programming/stealthVanV5/assets/vanVector_1.png" van_background.h`
- Touch on the GIGA shield often needs coordinate transforms in landscape mode. The sketch exposes those settings near the top:
  - `TOUCH_SWAP_XY`
  - `TOUCH_INVERT_X`
  - `TOUCH_INVERT_Y`
- The furnace controller currently uses sensor slot `5` (`Center`) as the control sensor because `FURNACE_SENSOR_INDEX` is zero-based and set to `4`.
- The LED page assumes three PWM outputs with brightness applied in software. If your LED hardware uses a different driver, this page is the part to adapt.

## What To Customize First

Edit the constants near the top of the sketch:

- Pin assignments
- Touch orientation flags
- `FURNACE_SENSOR_INDEX`
- `SENSOR_LAYOUT`
- `DEFAULT_SETPOINT_C`
- `FURNACE_ACTIVE_HIGH`
- `LEDS_ACTIVE_HIGH`

## Suggested Next Improvements

- Bind DS18B20 sensors by ROM address instead of discovery order.
- Add Fahrenheit / Celsius switching.
- Add minimum furnace off-time / on-time protection.
- Save settings in flash so setpoint, sleep preference, and light values survive reboot.
- Replace the vector-drawn van with the exact image asset if you want a pixel-perfect display.
