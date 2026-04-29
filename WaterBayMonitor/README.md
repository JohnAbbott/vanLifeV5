# Water Bay Monitor

Arduino UNO R4 WiFi firmware for monitoring four DS18B20 temperature sensors near the van water pump, accumulator tank, and water lines.

## Hardware

- Arduino UNO R4 WiFi
- Seeed Studio CAN-BUS Shield V2.0
- Four DS18B20 temperature sensors on one OneWire bus
- Noctua NF-A8 PWM fan

## Wiring

- DS18B20 data bus: Arduino `D4`
- DS18B20 pull-up: 4.7k from data to 5V
- Fan PWM control: Arduino `D5`
- Seeed CAN shield CS: `D9`
- Seeed CAN shield INT: `D2`
- CAN speed: 500 kbps

The Noctua fan motor power should come from a suitable 12V supply. The fan PWM control wire should not power the fan. For a permanent install, drive the PWM control line through the interface recommended by Noctua/Intel 4-wire PWM fan guidance rather than loading the Arduino pin directly.

## Temperature Behavior

- Fan is off above 42 F.
- Fan starts below 40 F at a slow PWM value.
- Fan ramps to full speed by 35 F.
- SMS alert is attempted below 34 F.
- SMS alerts re-arm once the coldest valid sensor rises to 36 F.
- SMS alerts are rate-limited to once every 6 hours.

## CAN Frames

All multi-byte signed values are little-endian Fahrenheit tenths.

### `0x540` Status

| Byte | Meaning |
| --- | --- |
| 0-1 | Coldest valid temperature in tenths F, or `INT16_MIN` if none valid |
| 2 | Fan PWM, 0-255 |
| 3 | Number of discovered sensors |
| 4 | Flags: bit 0 Wi-Fi connected, bit 1 SMS armed |
| 5-7 | Reserved |

### `0x541` through `0x544` Sensor Temperatures

| Byte | Meaning |
| --- | --- |
| 0 | Sensor index, 0-3 |
| 1-2 | Temperature in tenths F, or `INT16_MIN` if invalid/missing |
| 3 | Valid flag, 1 valid or 0 invalid |
| 4 | Fan PWM, 0-255 |
| 5 | Number of discovered sensors |
| 6-7 | Reserved |

## Secrets

Credentials live in `WaterBayMonitor/arduino_secrets.h`, which is ignored by git. `arduino_secrets.h.example` shows the expected shape.

Set `TWILIO_TO_NUMBER` before relying on SMS alerts. The Twilio phone number is only the sender number; Twilio also needs a destination number.

Because the Twilio token was shared in chat, rotate it in Twilio after the first successful test and update `arduino_secrets.h`.

## Build

```sh
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi WaterBayMonitor
```

## Upload

```sh
arduino-cli upload -p /dev/cu.usbmodemXXXX --fqbn arduino:renesas_uno:unor4wifi WaterBayMonitor
```

