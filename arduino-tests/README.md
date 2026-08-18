# Arduino IDE — IR receiver & transmitter tests

Diagnostic sketches to prove the Adafruit 5990 is wired correctly, using the
**Arduino IDE**. These are throwaway test images — the shipped firmware for this
project is ESPHome (`ir-blaster-livingroom.yaml`), not these sketches.

Run them in order: **Stage 1 proves the wiring** (no libraries, isolates a bad
connection fast). **Stage 2 decodes** actual remote codes. **Stage 3 transmits** a
captured code out the emitter and confirms the target device responds.

---

## One-time setup

### 1. Add ESP32 board support

**Arduino IDE → Settings** (macOS: `Arduino IDE → Settings…`, `⌘,`) → in
**"Additional boards manager URLs"** paste:

```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Click OK.

### 2. Install the ESP32 core

**Tools → Board → Boards Manager…** → search **`esp32`** → install
**"esp32" by Espressif Systems**.

(This is a few hundred MB and takes a while the first time.)

### 3. Select the board and port

- **Tools → Board → esp32 → "Adafruit Feather ESP32 V2"**
- **Tools → Port → `/dev/cu.usbserial-5B1E0635301`**
- **Tools → Upload Speed → `115200`** ← **required**

> **The upload speed matters.** This board's USB-serial bridge is a CH34x
> (VID `0x1A86`), which is unreliable at the default **921600**. Uploads connect,
> identify the chip, then die with *"A fatal error occurred: The chip stopped
> responding."* Dropping to 115200 fixes it. (Verified on this board — the failure
> is the baud rate, not your wiring.)
>
> Set it **after** choosing the board — selecting the board resets this menu to 921600.

> **Click Upload (→), not Debug (🐞).** The bug icon starts a JTAG debug session, which
> needs an external probe this board has no interface for; it fails with `unable to open
> ftdi device`. On this board `Serial.println()` is your debugger. See
> [`../learn/README.md`](../learn/README.md#dont-press-the-debug-button).

> That port is the board as currently detected. If it changes, it'll be the
> `/dev/cu.usbserial-*` entry — not `/dev/cu.Bluetooth-*` or the others.

---

## Stage 1 — Wiring test (no libraries)

Open **`arduino-tests/ir_rx_wiring_test/ir_rx_wiring_test.ino`**.

1. Click **Upload** (→ arrow).
2. Open **Tools → Serial Monitor** and set the baud rate to **115200** (dropdown,
   bottom-right).
3. Point any remote at the 5990 and press buttons.

### Reading the result

The sketch first decides whether anything is **actually driving** GPIO14, by toggling
the ESP32's internal pull-down/pull-up and seeing if the pin follows. A floating
(disconnected) pin obeys the internal resistor; a pin driven by the 5990 overpowers
it. This is the part that tells "sensor connected" apart from "nothing on the wire" —
reading the pin once cannot, because a floating pin reads HIGH just as readily as a
connected one.

| Serial Monitor shows | Verdict |
|---|---|
| `DRIVEN HIGH — the 5990 is connected and idle`, then `IR DETECTED! edges=…` on each press | ✅ **Receiver wiring is good.** Move to Stage 2. |
| `FLOATING — nothing is driving GPIO14` (`follows pulldown/pullup: 0/1`) | ❌ **5990 not connected.** White wire (IRin) not seated, or the sensor has no power. Check white→GPIO14, red→3V, black→GND. |
| `DRIVEN LOW — something is holding GPIO14 low` | ❌ Short to GND, or a miswired lead. |
| `DRIVEN HIGH` but no edges when you press a remote | ❌ White wire (IRin) on the wrong pin, or the remote is dead / not 38 kHz / not aimed at the sensor. |
| Edges reported continuously with no remote press | ❌ Noise on the line. (The sketch holds GPIO27 LOW so the 5990's own emitter can't be the cause.) |

Stage 1 exercises the entire receive chain, so a pass here means the wiring is
sound — no multimeter needed.

---

## Stage 2 — Decode test

Requires the **IRremote** library (v4.x) by *Armin Joachimsmeyer*:
**Sketch → Include Library → Manage Libraries…** → search **`IRremote`** → Install.

Open **`arduino-tests/ir_rx_decode_test/ir_rx_decode_test.ino`**, Upload, and open
the Serial Monitor at **115200**.

Each button press prints:
- the **decoded protocol / address / command** (e.g. `NEC address=0x20DF command=0x10EF`),
- IRremote's own C++ send call,
- the **raw timing array**.

### Getting these into ESPHome — the values transfer, the format does not

IRremote prints **C++**, not ESPHome YAML, so you have to transcribe. Don't paste
its output verbatim — ESPHome will reject it.

| From IRremote | Into ESPHome |
|---|---|
| `IrSender.sendNEC(0x20DF, 0x10EF, 0);` | Take just the hex values → `transmit_nec:` with `address: 0x20DF`, `command: 0x10EF` |
| The raw dump — multi-line, whitespace-padded, and starting with a big negative **gap** (e.g. `-1044750`) | **Drop the leading gap**, strip newlines/padding, and join the remaining `+mark`/`-space` µs values into one flat comma-separated array for `transmit_raw.code` or the `send_raw_ir` `raw` variable |

If a frame is long (AC and soundbar remotes especially), watch for the sketch's
`!! OVERFLOW` warning — a truncated raw array looks valid but will not replay.
Raise `RAW_BUFFER_LENGTH` and re-capture.

See [`../docs/capture-replay.md`](../docs/capture-replay.md), and record what you
capture in [`../captured-codes/`](../captured-codes/).

---

## Stage 3 — transmit test (`ir_tx_test`)

Requires the **IRremote** library (v4.x), same as Stage 2. Open
**`arduino-tests/ir_tx_test/ir_tx_test.ino`**.

It replays a raw code captured with Stage 2 (baked in as the example
`fan-power-switch`; swap the array for your own) out the emitter on **GPIO27**
(green / IRout). It fires **once per boot**, then idles — so the target toggles a
single time. Tap the board's **RESET** to send again.

1. Paste your captured code into the `code[]` array (IRremote raw format).
2. Upload, point the 5990 at the target device.
3. On boot it fires once → the device should respond. (Phone-camera check: the LEDs
   flash faint purple as it sends.)

⚠️ **It fires on every boot — and this board resets whenever a serial port is
opened** (DTR/RTS → EN). So opening the Serial Monitor, or any tool that toggles
DTR/RTS, re-boots the board and sends the code again. Expect an extra toggle each
time you connect. Use `python3 ../learn/monitor.py` (holds DTR/RTS low) if you want
to watch serial without re-triggering — though it still resets once on open.

---

## Gotchas

- **Serial Monitor must be 115200 baud.** At the wrong rate you get garbage or nothing.
- **Board rebooting / boot noise (`SPI_FAST_FLASH_BOOT`, `configsip:`) shredding your
  output?** The Arduino Serial Monitor drives DTR/RTS, which are wired to the ESP32's
  EN (reset) and GPIO0 (boot) pins, and can reset the chip mid-print. Use
  `python3 learn/monitor.py` instead — it holds those lines low. Measured on this board:
  the IDE monitor rebooted constantly where `monitor.py` had zero reboots. See
  [`../learn/README.md`](../learn/README.md).
- **Upload fails / port busy?** Close the Serial Monitor, then Upload. Only one thing
  can own the port at a time — this also applies to `esphome logs`.
- **Don't set `inverted`/pull-ups on the RX pin.** The 5990's output is already
  buffered and demodulated. **Note this differs from the ESPHome config, which needs
  `inverted: true`** — measured 2026-08-09 (R1); an earlier revision of this line
  cited `inverted: false` and was wrong.
- **A phone camera** sees the 940 nm LED flash faint purple when the emitter fires
  (Stage 3) — a quick smoke test.

## When the test passes

Flash the real ESPHome firmware over the top — it overwrites the test sketch
entirely, nothing to undo:

```bash
esphome run ir-blaster-livingroom.yaml
```
