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
  flash** the board. After the initial flash, updates can go over WiFi (OTA) with the
  4298 just powering it — and **OTA is exercised and working**: twice on 2026-08-09
  (27.48 s, 11.17 s) and again on 2026-08-18. Only the *first* flash needs USB.

## Why these parts (context for substitutions)

- **Classic dual-core ESP32, not S2/S3** — best-supported silicon for ESPHome's
  `remote_receiver` / `remote_transmitter` components. ESPHome board key:
  `adafruit_feather_esp32_v2` (variant esp32). Arduino-cli FQBN equivalent:
  `esp32:esp32:adafruit_feather_esp32_v2`.
- **5990 has an onboard N-channel FET driver**, so a 3.3V GPIO toggles the emitter
  with no external transistor. The LEDs pull from **VIN** — and on this board VIN is
  also the logic-level reference for the receiver output, so with a 3.3V ESP32 you
  power VIN from **3V** (see `wiring.md`), which caps emitter range but keeps the
  receiver line pin-safe.
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
> | ESPHome (`esphome run`) | Set `platformio_options: upload_speed: 115200` under `esphome:` in **every** config — including any new one you add. (Already set in the configs currently in this repo.) |
> | Arduino IDE | **Tools → Upload Speed → 115200** (set it *after* choosing the board — picking the board resets this menu to its 921600 default) |
> | `arduino-cli` | append `:UploadSpeed=115200` to the FQBN |

## The 5990 transceiver

- Combined **emitter (940 nm) + receiver (38 kHz demodulator)** on one board.
- Pins (per [Adafruit's pinout](https://learn.adafruit.com/adafruit-infrared-ir-remote-transceiver/pinouts)):
  **green = IRout** = drive to fire the LEDs (**transmit** → GPIO27);
  **white = IRin** = demodulated signal from the receiver (**receive** → GPIO14).
  The names are the reverse of what they sound like — don't swap them.
- Output is **buffered/demodulated**, but the ESPHome receiver still needs
  **`inverted: true`** — the IRin line idles HIGH and pulls LOW during a carrier
  burst, so `false` records idle as mark. Measured 2026-08-09 (R1): `false` gave
  0/24 matching entries and no decode; `true` gave 24/24 and decoded first press.
  This bullet previously said `false` (see [troubleshooting](troubleshooting.md)).
- Has **one vertical and one horizontal emitter LED** — reorient the board if range
  to a specific device is marginal, rather than adding a second blaster.
- **Single VIN powers both the receiver and the LEDs, and VIN is the output logic
  level.** So VIN must match the MCU: for the 3.3V ESP32, wire VIN to **3V**, not 5V.
  Powering VIN at 5V drives the receiver output (white) to ~5V into a 3.3V GPIO —
  over the ESP32's ~3.6V limit, at power-up before any firmware runs. 5V VIN is only
  for genuinely 5V-logic MCUs (e.g. a classic Uno). For more emitter range on a 3.3V
  MCU, use a separate 5V emitter rather than pushing this board's VIN to 5V — see
  [`wiring.md`](wiring.md).
- **The 3V range cost is small.** A SPICE check ([`../sim/`](../sim/), calibrated to
  Adafruit's ~100mA@3V / ~200mA@5V figures) puts the emitter at **~115 mA/LED at 3.3V
  vs ~200 mA at 5V** — about 57% of the current, so roughly **0.75× the range**
  (√ of intensity). Adafruit's "10+ m" at 5V becomes ~7–8 m at 3.3V: still plenty for
  a single-room blaster, and safely within the LED/FET ratings.
- **Not I2C** despite the 4045 cable's "I2C STEMMA" label — do not use the STEMMA QT
  port.
