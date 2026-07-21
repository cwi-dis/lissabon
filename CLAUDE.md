# lissabon

Off-grid lighting control framework built on [iotsa](../iotsa). Controls 12V LED lights (and experimentally 220V) via BLE and WiFi. Target application: solar-powered, battery-backed premises (allotments, boats, caravans).

## Repository layout

### Shared library

**libLissabon/** — C++ library shared by all appliances. Included as source (not a pre-compiled library). Contains:

- `AbstractDimmer` — base class for all dimmer types; handles level, on/off, gamma, animation, temperature
- `BLEDimmer` — BLE client dimmer (used by remotes/controllers to control a remote appliance)
- `DimmerBLEServer` — BLE server side (used by dimmers/ledstrips to advertise themselves)
- `DimmerUI` — maps input events (buttons, touchpads, rotary encoder) to dimmer actions
- `DimmerCollection`, `DimmerDynamicCollection` — managing sets of remote dimmers
- `LissabonBLE` — BLE service/characteristic UUIDs shared across all appliances

Generic BLE client infrastructure (`IotsaBLEClientMod`, `IotsaBLEClientConnection` —
scan orchestration, device registry, connection handling) moved to iotsa core in
cwi-dis/iotsa#138; see `iotsa/CLAUDE.md` and `iotsa/docs/module-interface-status.md`.
`BLEDimmer` here is the lissabon-specific adapter built on top of it.

`libLissabon/platformio.ini` previously built test/example programs (`lissabonExample`, `lissabonSwitch`) via PlatformIO `src_filter`. Both were removed 2026-07-21: `lissabonSwitch` had bit-rotted from lacking CI coverage, and `lissabonExample` (which had absorbed its functionality) was judged too complex to serve as an example. Simpler replacement examples are planned but not yet written; `libLissabon/platformio.ini` currently has no `env:` sections.

### Firmware appliances

**lissabonDimmer/** — PWM dimmer (12V LED, incandescent, MR-16, etc.). `PWMDimmer.cpp/h` lives here (not in libLissabon). Hardware variant selected at compile time via `-DVARIANT="defs_xxx.h"`:

| defs file | UI | Notes |
|---|---|---|
| `defs_touchpads.h` | 2 touchpads (up/down) | |
| `defs_buttons.h` | 2 buttons (up/down) | |
| `defs_1button.h` | 1 cycling button | short=toggle, long=cycle level |
| `defs_rotary.h` | rotary encoder + push button | |
| `defs_hidden.h` | none | remote-only, for ceiling/wall installs |
| `defs_candle.h` | none | toggles on/off at every reboot, no level |
| `defs_220switch.h` | none | hacked 220V switch (Kruidvat/Tuya), esp32c3, no level |
| `defs_double.h` | 2× cycling button | two independent PWM channels |

**lissabonLedstrip/** — NeoPixel RGBW LED strip controller. `LedstripDimmer.cpp/h` and `iotsaPixelstrip.cpp/h` live here. Two variants: 5V vs 12V RGBW strips.

**lissabonRemote/** — battery-operated (LiPo) BLE client remote control. Touchpads. Normally WiFi-off; USB charging re-enables WiFi for configuration.

**lissabonController/** — BLE client controller with OLED display and rotary encoder. Incomplete.

### Boards used

| Board | Used by |
|---|---|
| esp32dev | most lissabonDimmer variants |
| lolin32 | lissabonDimmer (candle), lissabonController |
| lolin32-lite | lissabonRemote (has built-in LiPo charger) |
| pico32 | lissabonLedstrip |
| esp32-c3-devkitm-1 (SuperMini, RISC-V) | lissabon-220-switch |

### Python tooling

**python/** — `lissabonCalibrate` package. Calibrates RGBW ledstrips by driving the ledstrip and simultaneously measuring output with an `iotsaRGBWSensor` device over the network. Supports lux, CCT, and per-LED measurements; can produce matplotlib plots.

Note: does not follow the `extras/python/` convention used by other iotsa repos — should eventually be moved there.

Venv setup (from repo root):

```
python3 -m venv .venv
source .venv/bin/activate
pip install -e ../iotsa/extras/python/
pip install -e python/
```

### Other directories

- **sandbox/** — scripts and device config snapshots not ready for general use. `getconfig.sh` dumps live device config via HPS. Not to be reorganised — Jack deliberately keeps a `sandbox/` in repos for WIP worth committing.
- **doc/** — BLE power consumption analysis (`ble-power.md`).
- **images/** — photos of built hardware; ESP32 board pinout references (lolin32, esp32dev, pico32).

### Sibling directory (outside this repo)

**../lissabon-config/** — not a git repo. Saved JSON config files for two deployed ledstrip devices: `stripdeur` and `striplinks`. Lives alongside but not inside the lissabon repo.

## Hardware design files

`lissabonDimmer/extras/` and `lissabonLedstrip/extras/` each contain:

- Fritzing files (`.fzz`) and exported schematic/board PDFs
- Fusion 360 3D models (`.f3d`) for printable enclosures (hidden ceiling dimmer, pluggable dimmer, ledstrip controller case)

## Build system

PlatformIO. Each appliance is its own PlatformIO project with `src_dir = .`. All share these `lib_deps`:

```
https://github.com/cwi-dis/iotsa.git#develop
libLissabon
h2zero/NimBLE-Arduino
```

**Always verify `lib_deps` points to `#develop` branch of iotsa before building.**

Key build flags:

- `-DIOTSA_WITH_BLE` — enables BLE via NimBLE. Required for all lissabon appliances.
- `-DVARIANT="defs_xxx.h"` — selects hardware variant for lissabonDimmer.
- `-DDIMMER_WITHOUT_LEVEL` — on/off only, no brightness control (candle, 220-switch).
- `-DDIMMER_WITH_GAMMA`, `-DDIMMER_WITH_ANIMATION`, `-DDIMMER_WITH_TEMPERATURE` — optional features.
- `board_build.partitions = min_spiffs.csv` — required when BLE is enabled on 4MB flash boards (BLE stack takes ~200KB).

For esp32c3 (220-switch), USB CDC flags are also needed — see `platformio.ini` in lissabonDimmer.

## CI

GitHub Actions (`.github/workflows/`) builds a matrix of appliance × variant. **Not all variants are covered** — the following are missing and should be added:

- `lissabon-candle`
- `lissabon-220-switch`
- `lissabon-1button-dimmer`

## Deployed devices (Tuinpark Lissabon)

Full inventory and config backups in `../lissabon-config/`. Summary:

| Device | Type | Board | Pixels/notes |
|---|---|---|---|
| stripdeur | 5V GRBW ledstrip | pico32 | 48 px |
| stripkeuken | 5V GRBW ledstrip | pico32 | 71 px |
| striplinks | 5V GRBW ledstrip | pico32 | 48 px |
| striprechts | 5V GRBW ledstrip | pico32 | 45 px |
| stripbank | 12V RGBW ledstrip | pico32 | 60 px; reversed connector to prevent 5V/12V mix-up |
| keuken | buttons dimmer | esp32 | 20 kHz PWM |
| tafel | buttons dimmer | esp32 | |
| spot | plugin dimmer | esp32 | |
| bank | plugin dimmer | esp32 | |
| control | BLE controller | lolin32 | Controls the 4 strips; uses deep sleep |

`control` has bank/keuken/spot/tafel as `unassigned`; `stripkeuken` is not yet configured in it.

All devices: light sleep 1500 ms / 300 ms wake, WiFi disabled on boot. (Confirmed 2026-07-19
against `../lissabon-config/*/battery.json` — the previous "1200 s / 80 ms" here was stale/wrong.)

**Exception:** `control` uses deep sleep (not light sleep).

## Design notes

- BLE is the primary runtime protocol (NimBLE via `h2zero/NimBLE-Arduino`). WiFi is only enabled on explicit request (for configuration).
- Low power is a core goal: light-sleep wake/sleep cycle. Deep sleep is not used — wakeup is a reboot and costs ~100mA for ~1 second, making short deep-sleep cycles wasteful.
- All level changes are animated (gradual fade), both for aesthetics and to mask BLE latency.
- `DIMMER_WITHOUT_LEVEL` / `DIMMER_WITH_LEVEL` controls whether brightness is a concept at all — switches have no level.
- The `DimmerCallbacks` interface (`dimmerOnOffChanged`, `dimmerValueChanged`, `dimmerAvailableChanged`) is how the main module gets notified of dimmer state changes.
