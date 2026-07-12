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

| Serial Monitor shows | Verdict |
|---|---|
| `Idle level: 1 (HIGH)` then `IR DETECTED! edges=…` on each press | ✅ **Receiver wiring is good.** Power, ground, and the green→GPIO14 data line all work. Move to Stage 2. |
| `Idle level: 0 (LOW)`, never changes | ❌ 5990 likely unpowered or green wire not on GPIO14. Check red→USB(5V), black→GND. |
| Idle flickers / edges with no remote press | ❌ Floating pin — green wire isn't seated in the breadboard. |
| `Idle level: 1 (HIGH)` but no edges on press | ❌ Green wire on the wrong pin, or the remote is dead / not 38 kHz / not aimed at the sensor. |

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
- a ready-made send call,
- the **raw timing array**.

Both forms are directly reusable in the ESPHome build — paste them into a
`transmit_nec` / `transmit_raw` button or the `send_raw_ir` service. See
[`../docs/capture-replay.md`](../docs/capture-replay.md), and record what you
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
