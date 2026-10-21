# IR Sniffer / Blaster

A networked **infrared sniffer and blaster for a single room**, built on an
**Adafruit ESP32 Feather V2 running [ESPHome](https://esphome.io/)**. It captures
("sniffs") the signals from ordinary IR remotes, and replays them on demand from
Home Assistant, the device's built-in web server, or a raw-timing API action. The
result is that any IR-controlled device (fan, TV, AC, soundbar) becomes something
Home Assistant can automate.

> **Status: the full sniff → replay loop is proven on hardware.** A live capture from a
> fan remote replays through the emitter and actuates the fan; Home Assistant adoption,
> HA→device control and OTA updates are all exercised. Pinned to ESPHome 2026.6.2
> (`requirements.txt`). The one open item is an end-to-end call of the `send_raw_ir`
> action. Full checklist: [`docs/validation.md`](docs/validation.md).

## What it does

1. **Sniff** — point any 38 kHz remote at the device; the decoded protocol and the raw
   IR timings print to the ESPHome log.
2. **Store** — record the captured codes (see [`captured-codes/`](captured-codes/)).
3. **Blast** — replay codes from template buttons, the web server, Home Assistant,
   or the generic `send_raw_ir` action that transmits an arbitrary raw timing array.

## Hardware

All parts are from Adafruit; details, rationale and substitutions are in
[`docs/hardware.md`](docs/hardware.md), wiring in [`docs/wiring.md`](docs/wiring.md).

| Part # | Item | Role |
|--------|------|------|
| 5900 | ESP32 Feather V2 | The microcontroller (classic dual-core ESP32) |
| 5990 | IR Remote Transceiver | 940 nm emitter + 38 kHz receiver on one board |
| 4045 | JST PH 4-pin cable | Connects the 5990 to the breadboard |
| 4539 | Half-size breadboard | Solderless assembly |
| 4298 | 5.1V 3A USB-C power supply | Power only (any USB-C supply works) |

You also need a **USB-C data cable** for the first flash; power-only cables and the
4298 PSU cannot flash the board. No soldering is required.

## Quick start

```bash
# 1. Set up secrets (Wi-Fi, HA API key, OTA and fallback-AP passwords)
cp secrets.yaml.example secrets.yaml
#    then edit secrets.yaml — generate the API key with: openssl rand -base64 32

# 2. Install the ESPHome CLI
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt

# 3. Wire the hardware  -> see docs/wiring.md  (DO THIS BEFORE FLASHING)

# 4. Check the config, then do the first flash over a USB-C DATA cable
esphome config ir-blaster-livingroom.yaml
esphome run ir-blaster-livingroom.yaml

# 5. Watch IR captures stream in while you press remote buttons
esphome logs ir-blaster-livingroom.yaml
```

After the first flash, `esphome run` can update the board over Wi-Fi (OTA). If the
board can't join Wi-Fi it hosts a fallback access point, `IR-Blaster Fallback`, with
a captive portal for reconfiguring it. Using the Home Assistant ESPHome add-on instead
of the CLI? See [`docs/flashing.md`](docs/flashing.md) for the GUI route.

The device name `ir-blaster-livingroom` is historical; rename it in the YAML for a
fresh build (the reasons this build keeps it are in [`docs/naming.md`](docs/naming.md)).

## Home Assistant

The firmware exposes the ESPHome native API with encryption, so Home Assistant
discovers the board on the LAN (Settings → Devices & services → ESPHome) and asks for
the `api_encryption_key` from your `secrets.yaml`. Once adopted you get:

- a **button entity per captured code** (e.g. `Fan Power`), usable from dashboards,
  scripts and automations;
- the action **`esphome.ir_blaster_livingroom_send_raw_ir`**, which takes `raw` (a list
  of microsecond timings, `+mark / -space`) and transmits it, so automations can fire
  codes that aren't baked into the firmware.

The board also serves a small web UI at `http://ir-blaster-livingroom.local/` with the
same buttons. The capture → replay workflow is in
[`docs/capture-replay.md`](docs/capture-replay.md).

## Repo layout

| Path                            | What it is                                                        |
|---------------------------------|------------------------------------------------------------------|
| `ir-blaster-livingroom.yaml`    | **Authoritative** ESPHome device config (the actual firmware spec)|
| `secrets.yaml.example`          | Template for WiFi / API / OTA secrets — copy to `secrets.yaml`    |
| `requirements.txt`              | Standalone ESPHome toolchain                                      |
| `docs/hardware.md`              | Bill of materials, part rationale, substitution notes            |
| `docs/wiring.md`                | Solderless wiring table + notes                                  |
| `docs/flashing.md`              | First flash (USB) and later OTA updates                          |
| `docs/capture-replay.md`        | The sniff → store → blast workflow                               |
| `docs/troubleshooting.md`       | Constraints, gotchas, and known limitations                      |
| `docs/validation.md`            | Acceptance checklist                                              |
| `docs/extensions.md`            | Ideas once the basics work                                       |
| `docs/bangle-integration.md`    | Plan for browsing and firing captured codes from a Bangle.js watch |
| `docs/naming.md`                | **Why the identifiers say "living room" while the device is in the bedroom — do not rename** |
| `captured-codes/`               | Record captured IR codes here                                    |
| `arduino-tests/`                | Arduino IDE diagnostic sketches to verify the IR receiver wiring |
| `ir-wiring-test.yaml`           | ESPHome no-network receiver test (alternative to `arduino-tests/`)|
| `sim/`                          | ngspice emitter-drive check (quantifies the 3V-vs-5V range tradeoff)|
| `AGENTS.md`                     | Orientation for AI agents / contributors                         |

## Key facts (read before changing the config)

- **Board key:** `adafruit_feather_esp32_v2` (classic dual-core ESP32 — best ESPHome
  IR support). Validated against ESPHome 2026.6.2. Confirmed silicon on the actual
  board: **ESP32-PICO-V3-02**, 8 MB flash / 2 MB PSRAM (variant `esp32`).
- **Pins:** RX (receive) on **GPIO14** ← 5990 **white** (IRin); TX (transmit) on
  **GPIO27** ← 5990 **green** (IRout). The green/white names are the reverse of what
  they sound like — see [`docs/wiring.md`](docs/wiring.md). Don't swap them.
- **⚠️ Power the 5990's VIN (red) from `3V`, not 5V.** VIN is the receiver's output
  logic level; at 5V it drives ~5V into the 3.3V GPIO14 and can damage the ESP32.
- **`inverted: true` is correct** for the Adafruit 5990 on this build — **reversed
  2026-08-09**, see [`docs/troubleshooting.md`](docs/troubleshooting.md). This file
  previously said the opposite in bold, on a claim never tested against a real capture.
  With `false`, captures come back with every sign flipped and **look perfectly
  plausible**; they simply do not replay.
- The included PSU (Adafruit 4298) is **power-only** and cannot flash the board.
  The first flash needs a separate **USB-C data cable**; updates after that can go OTA
  over WiFi. **OTA is proven** as of 2026-08-09 — exercised twice (27.48 s, then 11.17 s),
  both reporting `OTA successful`, with `Bootloader rollback: supported` in the boot
  banner. The dual-partition layout means a failed OTA writes the inactive slot and
  leaves the running firmware intact, so there is no brick risk. See
  [`docs/validation.md`](docs/validation.md).
- The 5990 is **not I2C** despite the "STEMMA" cable label — wire the four leads to
  GPIOs, do not use the STEMMA QT port.

See [`docs/troubleshooting.md`](docs/troubleshooting.md) for the full list.

## License

[MIT](LICENSE) © 2026 Evan J Ray
