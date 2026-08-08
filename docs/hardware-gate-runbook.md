# Hardware gate runbook — R1, R15, R11

One sitting at the board. **Everything in `docs/bangle-integration.md` is speculative
until R1 passes**, so this runs before any watch work (the H6 decision, 2026-08-07).

Budget ~20 minutes. You need: the fan remote, line of sight to the fan, and a phone or
laptop that can reach Home Assistant. A USB-C **data** cable only if OTA fails.

---

## Before you start

Agent-side, already done — nothing for you here, listed so you know the state:

- `ir-roundtrip-test.yaml` written and compiled clean
- `buffer_size` raised 2kb → 8kb in all three configs (R13)
- `docs/validation.md` corrected (R12)

---

## R15 — does Home Assistant actually reach the board? *(do this first, ~1 min)*

Cheapest test in the project, and it has been asserted without evidence three times.
Doing it first means that if it fails you find out before spending time on a flash.

1. Open Home Assistant → **Developer Tools → States**, or just find the device page.
2. Press **`button.ir_blaster_living_room_fan_power`** from the HA UI.
3. **Watch the fan.**

| result | what it means | record it as |
|---|---|---|
| Fan toggles | HA → ESP32 delivery works. The hop the whole architecture rests on | R15 **PASS** |
| Nothing happens | HA queues the command but it does not arrive, or the emitter is not firing | R15 **FAIL** — stop and tell me; the HA-based design needs rethinking |

> **Do not** record a pass because a timestamp moved in HA. That is exactly the mistake
> this test exists to correct — `button.*` state is HA's own `last_pressed`, written
> whether or not the device ever hears anything. **Only the fan moving counts.**

---

## R1 — the round-trip *(the real gate, ~10 min)*

Proves the receiver fires in ESPHome, that `on_raw` yields a usable array, and that the
array replays and actuates the fan. **Runs entirely on-device — no Home Assistant.**

### 1. Flash

```bash
.venv/bin/esphome run ir-roundtrip-test.yaml
```

OTA is attempted first. **OTA has never been exercised on this board** — both prior
flashes were USB — so this doubles as the OTA test.

- **It works** → OTA is proven. Tell me; it comes off the open list.
- **It fails** → plug in a **USB-C data cable** and re-run. The bundled Adafruit 4298
  PSU is power-only and **cannot flash**. Record OTA as failed, not as untested.

### 2. Watch the log

```bash
.venv/bin/esphome logs ir-roundtrip-test.yaml
```

### 3. Press the remote

Point the **fan remote** at the 5990 and press **POWER once**. Then keep your eyes on
the fan.

### 4. Read the result

| what you see | meaning | record as |
|---|---|---|
| `CAPTURED n entries`, then the fan toggles ~3 s later | **The whole premise holds.** Receiver fires, `on_raw` delivers, the array round-trips | R1 **PASS** |
| `CAPTURED n entries`, but the fan never moves | Capture works, replay does not. Copy the logged array to me — it is a shape or carrier problem, and it is diagnosable | R1 **PARTIAL** |
| Nothing at all in the log | The ESPHome receiver never fired. This is the RX path, not `on_raw` | R1 **FAIL** |
| Garbage lines with no button press | Floating or miswired data line — see `docs/wiring.md` | R1 **FAIL** |

**The device transmits by itself, once, 3 seconds after the first code it hears.** That
is the test. It will not loop — a latch stops it capturing its own replay.

### 5. Copy me the log

Whatever happens, paste the `CAPTURED` block. I need it for two reasons beyond
pass/fail:

- **Does the array match `captured-codes/README.md`?** The committed fan code is 47
  entries ending on a mark (`1230`) with no trailing space. A live capture may differ at
  the edges — that is expected and worth knowing precisely, because it tells me whether
  captures need normalising before storage.
- **R11 — how long do real arrays get?** The fan is my only sample.

---

## R11 — sample the other remotes *(optional, ~5 min, while you are there)*

If R1 passed and you have other remotes to hand, this is nearly free and it sizes the
whole library design.

The test image only captures **once per boot** — a deliberate latch. To capture another
remote, power-cycle the board (or re-flash) and press a different remote.

Most useful samples, in order:

1. **Any HVAC / mini-split / window AC remote.** These send full state in one frame and
   commonly run 100–450 entries versus the fan's 47. If one exists in this house it is
   the single most informative capture — it is what `buffer_size: 8kb` was raised for.
2. TV or soundbar power.
3. Anything that decodes as a named protocol (NEC, Sony, Samsung) rather than raw —
   `dump: all` will print the protocol name, which tells me whether the library needs to
   store decoded codes as well as raw arrays.

---

## When you are done

```bash
.venv/bin/esphome run ir-blaster-livingroom.yaml   # restore production firmware
```

**Do not skip this.** The test image transmits on its own and has no code library, no
web server and no HA buttons. Leaving it flashed means the fan button in HA is dead.

Then tell me: R15 result, R1 result, whether OTA worked, and the `CAPTURED` block.

---

## What each outcome unblocks

| outcome | what happens next |
|---|---|
| R1 + R15 pass | Phase 2 starts — the HA code library, the three endpoints, the transmit counter. The watch sitting gets scheduled |
| R1 passes, R15 fails | Premise holds but the HA path does not. Design shifts toward the ESP32's own web server; a real rethink, and I would want to talk it through |
| R1 partial (capture, no replay) | Diagnosable from the logged array. Likely carrier frequency or an edge artefact. Not fatal |
| R1 fails silent | The ESPHome receiver path is the problem, and it is a genuine fork: either debug `remote_receiver` tuning, or fall back to a custom component. The Arduino sketches already prove the hardware works, so this would be software |
