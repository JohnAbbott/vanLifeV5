# vanLifeV5

`vanLifeV5` is a home for the electronics and control software used in the van project.

## Current Project

The main project in this repository is:

- [`arduino-giga-van-ui/`](./arduino-giga-van-ui/)

That project contains an `Arduino GIGA R1 WiFi` + `GIGA Display Shield` touchscreen application with:

- A van temperature map overlay
- A furnace controller page
- RGB + brightness light controls
- A pump relay toggle
- Auto-sleep / touch-to-wake display behavior

## Main Files

- [`arduino-giga-van-ui/arduino-giga-van-ui.ino`](./arduino-giga-van-ui/arduino-giga-van-ui.ino)
- [`arduino-giga-van-ui/README.md`](./arduino-giga-van-ui/README.md)

## Hardware Summary

- `Arduino GIGA R1 WiFi`
- `GIGA Display Shield`
- `DS18B20` temperature sensors
- Furnace relay/triac interface
- Pump relay output
- RGB lighting outputs

## Next Steps

- Add more modules to this repo as the van electronics project grows
- Document sensor wiring and relay wiring in more detail
- Add setup, calibration, and deployment notes over time
