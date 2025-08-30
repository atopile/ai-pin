# Firmware (Zephyr, ESP32‑C3)

This directory will contain the Zephyr application for the AI Pin firmware.

Quickstart

- Ensure you have a Zephyr workspace (e.g., `~/zephyrproject`) and Zephyr SDK installed (e.g., `~/zephyr-sdk-0.17.2`).
- This app targets ESP32‑C3 DevKitM and uses a device‑tree overlay to enable I2S and SPI SD (CS on GPIO4).

Environment setup (recommended once)

```
# Create/use a Python venv for west/cmake integration
/opt/homebrew/bin/python3 -m venv "$HOME/zephyr-venv"
source "$HOME/zephyr-venv/bin/activate"
pip install -U pip west

# Use your Zephyr workspace and SDK
cd "$HOME/zephyrproject"
export ZEPHYR_SDK_INSTALL_DIR="$HOME/zephyr-sdk-0.17.2"
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
west zephyr-export
```

Build

```
source "$HOME/zephyr-venv/bin/activate"
cd "$HOME/zephyrproject"
west build -b esp32c3_devkitm /Users/nicholaskrstevski/github/ai_pin/firmware/zephyr/app -p always -- \
  -DDTC_OVERLAY_FILE=boards/esp32c3_devkitm.overlay
```

Flash

```
west flash
```

Notes

- `prj.conf` enables I2S, SPI, FATFS, Wi‑Fi, Logging, and Settings. DMA is enabled for I2S.
- `boards/esp32c3_devkitm.overlay` maps SPI microSD CS to GPIO4 and enables I2S (`&i2s`). Update MOSI/MISO/SCLK to match your board.
- Wi‑Fi provisioning: copy `firmware/zephyr/wifi.txt.example` to the SD card root and rename to `wifi.txt`:
  - `ssid=YourNetwork`
  - `psk=YourPassword`
    The firmware reads `/SD:/wifi.txt` at boot and attempts to connect.

Troubleshooting

- If build fails to find west from CMake: pass the venv python
  - Add to the build command tail: `-DPython3_EXECUTABLE="$HOME/zephyr-venv/bin/python" -DWEST_PYTHON="$HOME/zephyr-venv/bin/python"`
- If blobs missing for ESP32: run `west blobs fetch hal_espressif` in `~/zephyrproject`.
- If you see "DMA peripheral is not enabled!" from I2S: ensure `CONFIG_DMA=y` and `CONFIG_DMA_ESP32=y` are set in `prj.conf`.
- For FatFS volume errors: use `zephyr,sdhc-spi-slot` with `mmc { compatible = "zephyr,sdmmc-disk"; }` in the overlay. Avoid hard‑coding `FS_FATFS_VOLUME_STRS` in `prj.conf`.

Run & monitor

- UART 115200‑8‑N‑1. Example: `screen /dev/tty.usbserial* 115200`
- Expected boot logs:
  - `AI Pin firmware (Zephyr) booting`
  - SD: `Mounted FAT at /SD:` (when card present)
  - Wi‑Fi: `Read Wi‑Fi credentials ...` then `Got IPv4 address`

Testing checklist (abridged)

- Build & flash: no immediate errors on boot
- Audio: no persistent I2S errors; ring overflows rare at idle
- Storage: `.wav` files in SD root; valid PCM S16LE @ 16kHz; rotation works
- Wi‑Fi: connects using `/SD:/wifi.txt` and obtains IPv4 address

## Monitoring serial output

Use one of the following options to view logs at 115200 baud.

Option A: west monitor (recommended if west is in your venv PATH)

```
source "$HOME/zephyr-venv/bin/activate"
cd "$HOME/zephyrproject"
west espressif monitor -p /dev/cu.usbmodem1101 -b 115200
# Quit with: Ctrl-]
```

Option B: Python miniterm (no west required)

```
python -m serial.tools.miniterm /dev/cu.usbmodem1101 115200
# Quit with: Ctrl-]
```

Option C: screen (preinstalled on macOS)

```
screen /dev/cu.usbmodem1101 115200
# Quit with: Ctrl-A then K, then Y
```

## Troubleshooting serial

- If you see only ROM boot messages and no Zephyr logs, the port may be reset into bootloader. Try another app that does not toggle DTR/RTS, such as miniterm or screen.
- If `west` is not found, ensure your venv is active: `source "$HOME/zephyr-venv/bin/activate"` and that west is installed: `pip install -U west` then `west --version`.
- To list ports: `ls /dev/cu.*` and pick the `usbmodem` (ESP32-C3 USB CDC) device.

## Expected boot logs

- `AI Pin firmware (Zephyr) booting`
- `Mounted FAT at /SD:` once the SD card is detected
- `Read Wi‑Fi credentials ...` then `Got IPv4 address` on successful Wi‑Fi

## Pinout (current build)

Note: Console is routed to on‑chip USB CDC; I2S is currently disabled to avoid SPI2 pin conflicts.

| Function                        | GPIO    | Notes                            |
| ------------------------------- | ------- | -------------------------------- |
| LED (red)                       | 20      | Active‑high heartbeat            |
| Addressable LED (SK6805/WS2812) | 21      | Data‑in                          |
| SPI2 SCLK                       | 6       | Board `spim2_default`            |
| SPI2 MOSI                       | 7       | Board `spim2_default`            |
| SPI2 MISO                       | 2       | Board `spim2_default`            |
| microSD CS                      | 4       | Active‑low (overlay `cs-gpios`)  |
| Console                         | USB CDC | `&usb_serial` (no external pins) |

## I2S mapping (disabled)

If I2S is enabled on this board configuration, pins map as:

- I2S MCLK: GPIO6
- I2S WS/LRCLK: GPIO5
- I2S BCLK: GPIO4
- I2S SD Out (to DAC): GPIO18
- I2S SD In (from mic): GPIO19

Conflict note: I2S BCLK on GPIO4 conflicts with the SD CS on GPIO4, and MCLK on GPIO6 conflicts with SPI2 SCLK on GPIO6. That’s why I2S is disabled in the current overlay.
