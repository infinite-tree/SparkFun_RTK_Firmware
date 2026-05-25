# BNO086 terrain compensation

Optional pole-height terrain compensation using a SparkFun BNO086 on Qwiic.

## Hardware

- [SparkFun VR IMU Breakout BNO086 (Qwiic)](https://www.sparkfun.com/products/22857)
- Qwiic cable to the RTK unit Qwiic port
- **INT** → ESP32 **GPIO 18**
- **RST** → ESP32 **GPIO 19**
- Mount the IMU rigidly with the receiver enclosure

Default I2C address: **0x4B** (jumper for **0x4A** supported in settings).

## Enable

1. Serial menu: `i) Configure Terrain Compensation` → enable, set pole height (m)
2. WiFi AP: **Terrain Compensation** section
3. Place unit stationary on flat ground → **Level Zero**
4. Reboot or power-cycle if BNO086 was connected after boot with feature enabled

When active, corrected **GGA** and **RMC** sentences are emitted on Bluetooth and SD log. NTRIP GGA uses corrected coordinates when the NTRIP client is active.

## Library

Install [SparkFun BNO08x Arduino Library](https://github.com/sparkfun/SparkFun_BNO08x_Arduino_Library) or use the copy in `lib/SparkFun_BNO08x_Arduino_Library`.

## Test sketch

`Firmware/Test Sketches/BNO086_Qwiic/` prints roll/pitch over USB serial for wiring verification.

## Compile flag

Comment out `#define COMPILE_TERRAIN_COMP` in `RTK_Surveyor.ino` to omit this feature from the build.

## Bench validation

With RTK fix and a known tilt angle θ, horizontal shift should approximate `poleHeight × sin(θ)`. Compare raw vs compensated GGA on uneven ground for jitter reduction.

Corrected NMEA reports a **ground-referenced** point below the antenna, not the raw antenna phase center.
