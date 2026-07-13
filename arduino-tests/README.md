# Arduino IDE — IR receiver test

Diagnostic sketches to prove the Adafruit 5990 is wired correctly, using the
**Arduino IDE**. These are throwaway test images — the shipped firmware for this
project is ESPHome (`ir-blaster-livingroom.yaml`), not these sketches.

Run them in order: **Stage 1 proves the wiring** (no libraries, isolates a bad
connection fast). **Stage 2 decodes** actual remote codes.

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
| `FLOATING — nothing is driving GPIO14` (`follows pulldown/pullup: 0/1`) | ❌ **5990 not connected.** Green wire not seated, or the sensor has no power. Check green→GPIO14, red→USB(5V), black→GND. |
| `DRIVEN LOW — something is holding GPIO14 low` | ❌ Short to GND, or a miswired lead. |
| `DRIVEN HIGH` but no edges when you press a remote | ❌ Green wire on the wrong pin, or the remote is dead / not 38 kHz / not aimed at the sensor. |
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

## Gotchas

- **Serial Monitor must be 115200 baud.** At the wrong rate you get garbage or nothing.
- **Upload fails / port busy?** Close the Serial Monitor, then Upload. Only one thing
  can own the port at a time — this also applies to `esphome logs`.
- **Don't set `inverted`/pull-ups on the RX pin.** The 5990's output is already
  buffered and demodulated (same reason the ESPHome config uses `inverted: false`).
- These sketches **only test the receiver.** The emitter can't be verified without a
  target device — though many phone cameras see the 940 nm LED as a faint purple
  flash, which is a reasonable smoke test.

## When the test passes

Flash the real ESPHome firmware over the top — it overwrites the test sketch
entirely, nothing to undo:

```bash
esphome run ir-blaster-livingroom.yaml
```
