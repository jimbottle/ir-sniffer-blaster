# IR Sniffer / Blaster

A programmable, networked **infrared blaster for a single room**. It captures
("sniffs") IR remote signals, stores them, and retransmits ("spoofs") them on
demand. Built on an **ESP32 Feather V2 running ESPHome**, controllable over the
Home Assistant API, MQTT, REST, or a built-in web server.

> **Status: config validated, not yet flashed.** `esphome config` passes against
> ESPHome 2026.6.2 (pinned in `requirements.txt`); no board has been flashed yet —
> that waits on the hardware. Start with [`docs/flashing.md`](docs/flashing.md).

## What it does

1. **Sniff** — point any 38 kHz remote at the device; decoded protocol + raw IR
   timings print to the ESPHome log.
2. **Store** — record the captured codes (see [`captured-codes/`](captured-codes/)).
3. **Blast** — replay codes from template buttons, the web server, Home Assistant,
   or the generic `send_raw_ir` service that transmits an arbitrary raw timing array.

## Acceptance criteria

- [ ] Board boots, joins WiFi, and is reachable (HA + web server + OTA).
- [ ] Receiver: pressing any 38 kHz remote at the device prints decoded and raw IR to the log.
- [ ] Transmitter: a captured code can be replayed and the target device responds.
- [ ] A generic service exists to transmit arbitrary raw IR timing arrays programmatically.

Full step-by-step checklist: [`docs/validation.md`](docs/validation.md).

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
| `captured-codes/`               | Record captured IR codes here                                    |
| `AGENTS.md`                     | Orientation for AI agents / contributors                         |

## Key facts (read before changing the config)

- **Board key:** `adafruit_feather_esp32_v2` (ESP32-WROOM, classic dual-core — best
  ESPHome IR support). Validated against ESPHome 2026.6.2.
- **Pins:** RX (receive) on **GPIO14**, TX (transmit) on **GPIO27**.
- **`inverted: false` is correct** for the Adafruit 5990's buffered output. Do
  **not** set it to `true` — that's only for a bare TSOP38238 sensor.
- The included PSU (Adafruit 4298) is **power-only** and cannot flash the board.
  The first flash needs a separate **USB-C data cable**; everything after is OTA.
- The 5990 is **not I2C** despite the "STEMMA" cable label — wire the four leads to
  GPIOs, do not use the STEMMA QT port.

See [`docs/troubleshooting.md`](docs/troubleshooting.md) for the full list.
