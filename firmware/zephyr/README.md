# Firmware (Zephyr, ESP32‑C3)

This directory will contain the Zephyr application for the AI Pin firmware.

Quickstart

- Ensure you have Zephyr SDK and west installed.
- Initialize a Zephyr workspace (or add this app to an existing one).
- Configure board overlays for ESP32‑C3 and build/flash.

Build

```
west build -b esp32c3_devkitm /Users/narayanpowderly/projects/ai-pin/firmware/zephyr/app -p always -- \\
  -DDTC_OVERLAY_FILE=boards/esp32c3_devkitm.overlay
```

Flash

```
west flash
```

Notes

- `prj.conf` enables I2S, SPI, FATFS, Wi‑Fi, Logging, and Settings.
- `boards/esp32c3_devkitm.overlay` maps SPI microSD CS to GPIO4 and enables I2S. Adjust pinctrl to your board.
- Wi‑Fi provisioning: place a `wifi.txt` file on the SD card root with the content:
  - `ssid=YourNetwork`
  - `psk=YourPassword`
    The firmware will read `/SD:/wifi.txt` at boot and attempt to connect.
