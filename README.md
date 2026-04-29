# vanLifeV5

`vanLifeV5` is a home for the electronics and control software used in the van project.

## Current Project

The main projects in this repository are:

- [`arduino-giga-van-ui/`](./arduino-giga-van-ui/)
- [`WaterBayMonitor/`](./WaterBayMonitor/)

`arduino-giga-van-ui/` contains an `Arduino GIGA R1 WiFi` + `GIGA Display Shield` touchscreen application with:

- A van temperature map overlay
- A furnace controller page
- RGB + brightness light controls
- A pump relay toggle
- Auto-sleep / touch-to-wake display behavior

`WaterBayMonitor/` contains an `Arduino UNO R4 WiFi` + `Seeed Studio CAN-BUS Shield V2.0` application with:

- Four DS18B20 water bay temperature sensors
- CAN messages for the display app
- PWM fan control for freeze protection
- Twilio SMS alerting for low-temperature events

## Main Files

- [`arduino-giga-van-ui/arduino-giga-van-ui.ino`](./arduino-giga-van-ui/arduino-giga-van-ui.ino)
- [`arduino-giga-van-ui/README.md`](./arduino-giga-van-ui/README.md)
- [`WaterBayMonitor/WaterBayMonitor.ino`](./WaterBayMonitor/WaterBayMonitor.ino)
- [`WaterBayMonitor/README.md`](./WaterBayMonitor/README.md)

## Hardware Summary

- `Arduino GIGA R1 WiFi`
- `GIGA Display Shield`
- `Arduino UNO R4 WiFi`
- `Seeed Studio CAN-BUS Shield V2.0`
- `DS18B20` temperature sensors
- Furnace relay/triac interface
- Pump relay output
- RGB lighting outputs
- Noctua PWM fan freeze-protection output

## Next Steps

- Update the display app to parse and show `WaterBayMonitor` CAN frames
- Document sensor wiring and relay wiring in more detail
- Add setup, calibration, and deployment notes over time
