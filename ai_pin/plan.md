# AI Pin PCBA – Planning Document

This plan outlines the architecture, chosen packages, and key design decisions for an ESP32-C3 based “AI pin” with an I2S microphone, microSD storage, LiPo battery power with USB‑C charging, and a regulated 3.1 V rail for the MCU.

## Goals and constraints
- ESP32‑C3 microcontroller as main MCU
- Digital microphone for voice commands (I2S preferred)
- External microSD card for audio storage (SPI interface)
- Battery powered (single‑cell LiPo)
- USB‑C for charging and power
- Power path: either battery or USB 5 V feeds an LDO set to 3.1 V to supply the ESP32
- Follow `@.cursor/rules/how_to_build_packages.mdc`; use registry packages; build must be error‑free
- Do NOT implement code yet; this file is the plan to review before implementation

## Selected atopile packages (registry)
- MCU: `atopile/espressif-esp32-c3@0.1.4`
  - Use `ESP32_C3_MINI_1_driver` (compact module, good community support)
  - Provides `power`, `i2c`, `spi`, `i2s`, `gpio[N]` interfaces; example pin mux in README
- Microphone: `atopile/microphones@0.2.0`
  - Pick I2S mic: `TDK_InvenSense_ICS_43434`
  - Supply 1.62–3.6 V (OK at 3.1 V)
- microSD card: `atopile/sd-card@0.1.6` + socket `atopile/sd-card-slots@0.1.4`
  - Use SPI mode with ESP32‑C3 `spi` interface
  - VDD 2.7–3.6 V; 3.1 V is within spec
- USB‑C connector: `atopile/usb-connectors@0.3.0`
  - Use `USB2_0TypeCHorizontal_driver` (or vertical if preferred)
  - Exposes VBUS and USB2.0 D+/D- if needed
- Charger with power‑path: `atopile/ti-bq25185@0.1.0`
  - Single‑cell Li‑ion charger with power path; I2C control/status
  - Input from USB‑C VBUS; battery to BAT; system rail from charger goes to LDO input
- LDO (adjustable to 3.1 V): `atopile/st-ldk220@0.2.1`
  - 200 mA low‑IQ LDO; input up to 13.2 V; dropout 100–350 mV
  - Configure output via assertion to 3.1 V (+/‑ 5%)

## Power architecture
- `power_vbus_5v`: from USB‑C VBUS (5 V)
- `power_batt`: single‑cell LiPo (nominal ~3.7 V; 3.0–4.2 V range)
- `charger_power_path_out`: system rail from `BQ25185` (sourced from VBUS or battery via charger’s power path)
- `power_3v1`: LDO output set to 3.1 V (feeds logic and MCU)

Connections (conceptual):
- USB‑C VBUS → BQ25185 VIN
- LiPo (via 2‑pin battery connector, see below) → BQ25185 BAT
- BQ25185 system/power‑path output → LDK220M_R input
- LDK220M_R output (3.1 V) → ESP32‑C3 power, microphone power, microSD VDD, I2C pull‑ups, etc.

Notes:
- 3.1 V is chosen to reduce MCU power while keeping peripherals in spec (microSD 2.7–3.6 V, I2S mic 1.62–3.6 V)
- If battery drops below (3.1 V + LDO dropout), rail will fall out of regulation; plan brown‑out threshold accordingly in firmware

## Interfaces and routing
- I2S Microphone
  - `i2s ~ microphone.i2s`
  - Route `i2s.ws`, `i2s.sck`, `i2s.sd` per ESP32‑C3 pin mux (see driver README example)
  - Mic power from 3.1 V
- microSD over SPI
  - `spi ~ sd.spi` and one GPIO for `cs`
  - VDD from 3.1 V; ensure adequate decoupling and short traces
  - Consider small series resistors (22–33 Ω) on SPI lines if needed (SI integrity)
- I2C bus (for charger config/telemetry; optional other sensors later)
  - `i2c ~ charger.i2c`
  - Add pull‑ups to 3.1 V (10 kΩ 0402 typical); the ESP32‑C3 example shows a pattern for this
- USB data (optional)
  - If desired for CDC/DFU, connect USB D+/D- from connector to MCU USB interface (ESP32‑C3 supports native USB‑CDC on some SKUs). Not strictly required for MVP; serial flashing also possible via UART.

## Connectors and passives
- Battery connector: JST‑PH‑2 (2.0 mm) or JST‑SH‑2 (1.0 mm) for compactness
  - If no registry package exists, use a simple 2‑pin connector package later; wire to BQ25185 BAT/GND
- Power rail decoupling
  - Follow each package recommendations; ensure local 100 nF near each IC, plus bulk caps near LDO output and charger input
- Pull‑ups / bias
  - I2C: 2× 10 kΩ to 3.1 V (SDA/SCL)
  - Charger: check BQ25185 pin requirements (ILIM, STAT, EN, thermistor/NTC). Driver likely models these; otherwise add passives per datasheet.

## Current budget (rough)
- ESP32‑C3 active wifi/BT: 80–240 mA peaks (depends on mode)
- I2S mic: a few mA
- microSD active write: 50–100 mA peaks
- LDO LDK220 limit: 200 mA — Risk: may be insufficient during simultaneous Wi‑Fi peaks + SD activity
  - Mitigations:
    - Reduce RF TX power / duty cycling
    - Switch to higher‑current LDO if needed (alternative regulator TBD)
    - Use local bulk capacitance on 3.1 V rail to absorb transients

## Software‑visible constraints
- Run ESP32 at 3.1 V; ensure brown‑out detector set appropriately (~2.9–3.0 V)
- SPI clock to SD tuned for stable operation at 3.1 V and board trace lengths
- I2S sample rate and mic gain set in firmware
- Charger configuration via I2C: set charge current per chosen battery and USB constraints

## Open questions / risks
- LDO headroom vs. ESP32 + SD peaks: 200 mA may be tight → be ready to swap for higher‑current LDO if needed
- Exact ESP32‑C3 module variant (Mini‑1 vs WROOM‑02): Mini‑1 is planned; verify antenna keepout and footprint
- Battery selection and connector footprint (capacity, discharge rate)
- microSD slot mechanical choice (push‑push vs push‑pull; top vs bottom mount)
- Do we expose USB D+/D- to ESP32 for CDC/flashing?
- BQ25185 details: confirm the driver’s power‑path output node to use for the LDO input; validate NTC/ILIM pins handling

## Implementation plan (after approval)
1. Create `ai_pin/ai-pin.ato` top‑level module
2. Install packages:
   - `atopile add atopile/espressif-esp32-c3`
   - `atopile add atopile/microphones`
   - `atopile add atopile/sd-card`
   - `atopile add atopile/sd-card-slots`
   - `atopile add atopile/usb-connectors`
   - `atopile add atopile/ti-bq25185`
   - `atopile add atopile/st-ldk220`
3. Wire power tree: USB‑C → BQ25185 → LDO → 3.1 V rail → loads
4. Wire I2S mic, SPI SD, I2C to charger; add pull‑ups
5. Add decoupling caps per package guidance
6. Build and resolve compiler/LSP issues; iterate on pin mux and constraints
7. Review DRC/ERC; refine footprints and mechanical choices

## Acceptance checklist prior to coding
- Package set agreed
- 3.1 V rail confirmed acceptable for ESP32, mic, and SD
- Current budget acceptable or alternative LDO identified
- Decision on USB data connectivity
- Battery capacity and connector style chosen