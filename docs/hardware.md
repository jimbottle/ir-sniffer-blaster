# Hardware

All hardware is **already purchased**. This page is reference for assembly and for
anyone considering substitutions.

## Bill of materials (as purchased, all Adafruit)

| Part #  | Item                                  | Role                                                              |
|---------|---------------------------------------|------------------------------------------------------------------|
| 5990    | IR Remote Transceiver                 | 940 nm emitter + 38 kHz receiver on one board (the sniffer/blaster)|
| 5900    | ESP32 Feather V2 (with headers)       | The MCU — 8MB flash / 2MB PSRAM, ESP32-PICO-MINI-02               |
| 4298    | Raspberry Pi 5.1V 3A USB-C PSU        | **Power only** (captive cable) — cannot flash the board          |
| 4045    | JST PH 2mm 4-pin socket → color cable | Mates the 5990; tinned wire ends push into the breadboard         |
| 4539    | Half-size breadboard (400 tie points) | Solderless mounting + interconnect                               |

### Not in the parts list — required for first flash

- **A USB-C DATA cable** (PC → Feather). The 4298 PSU is power-only and **cannot
  flash** the board. After the initial flash, all updates go over WiFi (OTA) and
  the 4298 just powers it.

## Why these parts (context for substitutions)

- **Classic dual-core ESP32, not S2/S3** — best-supported silicon for ESPHome's
  `remote_receiver` / `remote_transmitter` components. ESPHome board key:
  `adafruit_feather_esp32_v2` (variant esp32). Arduino-cli FQBN equivalent:
  `esp32:esp32:adafruit_feather_esp32_v2`.
- **5990 has an onboard N-channel FET driver**, so a 3.3V GPIO toggles the emitter
  while the LEDs themselves pull from 5V for 10+ m range. No external transistor
  needed.
- **PCB-antenna board is fine here** — an IR blaster sits in the open with line of
  sight to the gear, so an external antenna earns nothing in this placement.

## Confirmed silicon (read off the actual board)

`esptool` reports the real hardware, which differs from the original brief (which
said "ESP32-WROOM" — that was wrong, though the practical implication is the same:
classic dual-core ESP32, variant `esp32`):

| Property   | Value                                  |
|------------|----------------------------------------|
| Chip       | **ESP32-PICO-V3-02** (rev v3.1)        |
| Module     | ESP32-PICO-MINI-02                     |
| Flash      | 8 MB (embedded)                        |
| PSRAM      | 2 MB (embedded)                        |
| USB serial | CH34x-class bridge (VID `0x1A86`); macOS drives it natively — no driver install needed |

> **The CH34x bridge is unreliable at 921600 baud.** Flashing connects, identifies the
> chip, then dies with *"A fatal error occurred: The chip stopped responding."* It looks
> like dead hardware; it's the baud rate. Use **115200**.
>
> The 921600 default comes from the **ESP32 Arduino/PlatformIO board definition**, not
> from esptool (esptool's own default is 115200). So the fix differs per toolchain:
>
> | Toolchain | Fix |
> |---|---|
> | ESPHome (`esphome run`) | Already applied — both configs set `platformio_options: upload_speed: 115200` |
> | Arduino IDE | **Tools → Upload Speed → 115200** |
> | `arduino-cli` | append `:UploadSpeed=115200` to the FQBN |

## The 5990 transceiver

- Combined **emitter (940 nm) + receiver (38 kHz demodulator)** on one board.
- Output is **buffered/demodulated** → the ESPHome receiver uses `inverted: false`
  (see [troubleshooting](troubleshooting.md)).
- Has **one vertical and one horizontal emitter LED** — reorient the board if range
  to a specific device is marginal, rather than adding a second blaster.
- Accepts **3–5V logic**: drive the signal lines from 3.3V GPIOs, power the emitter
  LEDs from 5V for full range.
- **Not I2C** despite the 4045 cable's "I2C STEMMA" label — do not use the STEMMA QT
  port.
