# Robocoders ESP32-S3 firmware

Firmware for an ESP32-S3 (DevKitC-1) board. It controls the onboard RGB LED and speaks the
Robocoders text protocol (pairing, pin commands, telemetry, failsafe) over USB serial and
WebSocket (port 81).

## Setup

You need [PlatformIO](https://platformio.org/) (the VS Code extension is easiest).

1. Clone this repo and open it in VS Code.
2. Create `secrets.ini` by copying `secrets.ini.example`, and choose an OTA password:
   ```ini
   [secrets]
   ota_password = your-password-here
   ```
3. Create `src/secrets.h` by copying `src/secrets.h.example`, and enter your WiFi details:
   ```cpp
   #define WIFI_SSID "your-wifi-name"
   #define WIFI_PASSWORD "your-wifi-password"
   ```
   Both files are gitignored, so your passwords are never committed.

## Flashing

- **First upload (USB):** plug the board in and upload with the default env
  `esp32-s3-devkitc-1`, or run `pio run -t upload`.
- **Later uploads (WiFi / OTA):** once the board is on your network, pick the `ota` env,
  or run `pio run -e ota -t upload`. It finds the board at `ledboard.local`.
  If the first USB upload hasn't happened yet, OTA won't work.

## Monitor

`pio device monitor` (115200 baud) shows boot messages and accepts serial commands.
