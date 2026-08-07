# IR Sniffer / Blaster

A programmable, networked **infrared blaster for a single room**. It captures
("sniffs") IR remote signals, stores them, and retransmits ("spoofs") them on
demand. Built on an **ESP32 Feather V2 running ESPHome**, controllable over the
Home Assistant API, MQTT, REST, or a built-in web server.

> **Status: built and working.** Flashed on real hardware (ESP32-PICO-V3-02), on
> 2.4GHz WiFi via DHCP, reachable at `http://ir-blaster-livingroom.local/`. The
> **emitter** is validated end to end — the **Fan Power** button fires a captured code
> that toggles the fan over the network. RX hardware/wiring is proven via the Arduino
> diagnostics; the ESPHome receiver dump, HA adoption, and OTA aren't exercised yet
> (see acceptance criteria). Pinned to ESPHome 2026.6.2 (`requirements.txt`).

## What it does

1. **Sniff** — point any 38 kHz remote at the device; decoded protocol + raw IR
   timings print to the ESPHome log.
2. **Store** — record the captured codes (see [`captured-codes/`](captured-codes/)).
3. **Blast** — replay codes from template buttons, the web server, Home Assistant,
   or the generic `send_raw_ir` service that transmits an arbitrary raw timing array.

## Acceptance criteria

- [ ] Board boots, joins WiFi, and is reachable (HA + web server + OTA). *(Boots, WiFi,
      web server — confirmed. HA adoption + OTA not yet exercised.)*
- [ ] Receiver: pressing any 38 kHz remote prints decoded and raw IR to the log. *(RX
      hardware proven via Arduino diagnostics; ESPHome `remote_receiver` dump not yet
      observed.)*
- [x] Transmitter: a captured code can be replayed and the target device responds.
- [x] A generic service exists to transmit arbitrary raw IR timing arrays programmatically.

Transmit path fully proven on hardware. Still to confirm: the ESPHome receiver dump
(`esphome logs`), HA adoption, OTA, and the `send_raw_ir` service. Full checklist:
[`docs/validation.md`](docs/validation.md).

## Quick start

```bash
# 1. Set up secrets
cp secrets.yaml.example secrets.yaml
#    then edit secrets.yaml — generate the API key with: openssl rand -base64 32

# 2. (Standalone CLI route) install the toolchain
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt

# 3. Wire the hardware  -> see docs/wiring.md  (DO THIS BEFORE FLASHING)

# 4. First flash over a USB-C DATA cable (the included PSU cannot flash)
esphome run ir-blaster-livingroom.yaml

# 5. Watch IR captures stream in
esphome logs ir-blaster-livingroom.yaml
```

Home Assistant ESPHome add-on users: see [`docs/flashing.md`](docs/flashing.md)
for the GUI route.

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
| `captured-codes/`               | Record captured IR codes here                                    |
| `arduino-tests/`                | Arduino IDE diagnostic sketches to verify the IR receiver wiring |
| `ir-wiring-test.yaml`           | ESPHome no-network receiver test (alternative to `arduino-tests/`)|
| `sim/`                          | ngspice emitter-drive check (quantifies the 3V-vs-5V range tradeoff)|
| `AGENTS.md`                     | Orientation for AI agents / contributors                         |

## Key facts (read before changing the config)

- **Board key:** `adafruit_feather_esp32_v2` (classic dual-core ESP32 — best ESPHome
  IR support). Validated against ESPHome 2026.6.2. Confirmed silicon on the actual
  board: **ESP32-PICO-V3-02**, 8 MB flash / 2 MB PSRAM (the brief's "ESP32-WROOM"
  was inaccurate; variant is still `esp32`).
- **Pins:** RX (receive) on **GPIO14** ← 5990 **white** (IRin); TX (transmit) on
  **GPIO27** ← 5990 **green** (IRout). The green/white names are the reverse of what
  they sound like — see [`docs/wiring.md`](docs/wiring.md). Don't swap them.
- **⚠️ Power the 5990's VIN (red) from `3V`, not 5V.** VIN is the receiver's output
  logic level; at 5V it drives ~5V into the 3.3V GPIO14 and can damage the ESP32.
- **`inverted: false` is correct** for the Adafruit 5990's buffered output. Do
  **not** set it to `true` — that's only for a bare TSOP38238 sensor.
- The included PSU (Adafruit 4298) is **power-only** and cannot flash the board.
  The first flash needs a separate **USB-C data cable**; updates after that can go OTA
  over WiFi (OTA is configured but not yet exercised — both flashes so far were USB).
- The 5990 is **not I2C** despite the "STEMMA" cable label — wire the four leads to
  GPIOs, do not use the STEMMA QT port.

See [`docs/troubleshooting.md`](docs/troubleshooting.md) for the full list.
