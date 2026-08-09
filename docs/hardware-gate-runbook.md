# Hardware gate runbook — R1

**One thing left to do: R1.** R15 and R13 are closed (see [Already done](#already-done)).

R1 is **the** gate. Everything in `docs/bangle-integration.md` is speculative until it
passes, which is why it runs before any watch work (the H6 decision, 2026-08-07).

**Time:** ~10 minutes. **You need:** the fan remote, line of sight to the fan, and a
USB-C **data** cable within reach in case OTA fails.

---

# YOUR PART — four steps

Everything below this section is reference. These four steps are the whole job.

### Step 1 — Flash it

```bash
source .venv/bin/activate
esphome run ir-roundtrip-test.yaml
```

> **The venv is `.venv`, with a leading dot.** There is no system-wide `esphome`, so a
> bare `esphome ...` gives `command not found`. Without activating, the long form is
> `.venv/bin/esphome run ir-roundtrip-test.yaml`.

**`run` flashes and then streams the log by itself.** Leave it running — you do not need
a second command.

**It will try OTA first.** This is also the OTA test, which has never been run on this
board.

- Flashes over the air → **OTA works.** Tell me; it comes off the open list.
- Falls back to asking for a serial port → **OTA failed.** Plug in a USB-C **data** cable
  and re-run. The bundled Adafruit 4298 PSU is power-only and **cannot flash**. Tell me
  it failed — that is a real result, not a non-event.

### Step 2 — Press the remote, once

Once log lines are scrolling: point the **fan remote** at the 5990 and press **POWER
one time**.

Then **look at the fan** and keep looking for about 5 seconds.

### Step 3 — Read what happened

The board captures the first real code it hears, waits 3 seconds, and replays it by
itself. You press the remote once; the *second* fan movement is the board.

| what you see | verdict | what to do |
|---|---|---|
| `CAPTURED n entries`, then the fan toggles ~3 s later **on its own** | **R1 PASS** — the whole premise holds | Send me the log. Phase 2 starts |
| `CAPTURED n entries`, fan never moves again | **R1 PARTIAL** — capture works, replay does not | Send me the `CAPTURED` block; it is diagnosable from the array |
| `ignoring N-entry noise burst` repeatedly, no `CAPTURED` | Electrical noise on the RX line | See `docs/wiring.md`. Send me the log |
| Nothing at all in the log when you press | **R1 FAIL** — the ESPHome receiver never fired | Send me the log. This is a real fork; see [outcomes](#what-each-outcome-unblocks) |

### Step 4 — Put the real firmware back

```bash
esphome run ir-blaster-livingroom.yaml
```

**Do not skip this.** The test image transmits on its own, has no code library, no web
server and no HA buttons — leaving it flashed means the fan button in HA stops working.

---

# Send me back

1. **The `CAPTURED` block**, whatever happened. I need it for two things beyond
   pass/fail:
   - **Does it match `captured-codes/README.md`?** The committed fan code is 47 entries
     ending on a mark (`1230`) with no trailing space. A live capture may differ at the
     edges — that tells me whether captures need normalising before storage.
   - **R11 — how long do real arrays get?** The fan is my only sample.
2. **Did OTA work?**
3. Anything that looked wrong.

---

# Optional, while you are there — R11 sampling

Nearly free, and it sizes the whole library design. Skip it if R1 failed.

The test image captures **once per boot** — a deliberate latch. To capture another
remote, power-cycle the board, then press a different remote.

Most useful, in order:

1. **Any HVAC / mini-split / window AC remote.** These send full state in one frame and
   commonly run 100–450 entries against the fan's 47. If one exists in this house it is
   the single most informative capture — it is what `receive_symbols: 256` was set for,
   and the one case where the stock 192 would have silently truncated.
2. TV or soundbar power.
3. Anything that prints a **protocol name** (NEC, Sony, Samsung) rather than only raw
   timings — that tells me whether the library must store decoded codes as well as raw
   arrays.

---

# Reference

## Already done

Closed, listed so you know why they are not in your steps:

- **R15 — HA → ESP32 delivery. PASS, 2026-08-07.** You pressed
  `button.ir_blaster_living_room_fan_power` in HA and the fan toggled, on the standing
  production firmware with nothing flashed beforehand.
  *Kept as the recorded rationale:* **only the fan moving counts.** Three earlier
  revisions tried to close this on HA-side timestamps and were wrong each time —
  `button.*` state is HA's own `last_pressed`, written whether or not the device hears
  anything. Full record in `docs/validation.md`.
- **R13 — capture length.** Reopened and fixed properly: the knob is `receive_symbols`
  (now `256`), **not** `buffer_size`. See below.
- `ir-roundtrip-test.yaml` written, compiled clean, noise-guarded.

## Why `receive_symbols`, not `buffer_size`

Recorded because the first attempt at R13 got this wrong in three config files at once.

- `buffer_size` only floors a **queue of completed events** awaiting the main loop
  (`remote_receiver_rmt.cpp:107`). It is queue depth, not capture length. Its esp32
  default is already `10000b`, so the original "raise 2kb → 8kb" was still **below
  stock**.
- **`receive_symbols` is the per-frame ceiling**, passed as the `rmt_receive` length
  (`remote_receiver_rmt.cpp:109`). Default `192`, and each RMT symbol carries 2 pulses
  (`decode_rmt_` reserves `item_count * 2`) — so stock tops out near **384 entries**,
  under what an HVAC frame can reach, and it truncates **silently**.
- Now `256` in all three configs → ~512 entries.
**If the log ever says `out of RMT symbol memory` — LOWER `rmt_symbols`, do not raise it.**

That string comes only from `rmt_new_rx_channel` returning `ESP_ERR_NOT_FOUND`
(`remote_receiver_rmt.cpp:49-51`), and the one field driving that allocation is
`channel.mem_block_symbols = rmt_symbols_` (`:42`). So the fix is either:

- **lower `rmt_symbols`** on the receiver (default 192 = 3 of the ESP32's 8 blocks), or
- **lower `remote_transmitter`'s `rmt_symbols`** (esp32 default 64) to free blocks.

**Raising `rmt_symbols` makes it permanent, and changing `receive_symbols` does nothing**
— that one is a heap buffer and is never passed to `rmt_new_rx_channel`. An earlier
revision of this runbook advised both wrong moves; it is corrected here rather than
quietly, because it is the one troubleshooting line you would be reading at the board.

## Known risk — the API link has been seen dropping

Not a reason to delay, but if something behaves oddly this is the first thing to suspect.

`home-assistant-pi/CHANGELOG.md:1427-1441` records **five** consecutive
`Referenced entities button.ir_blaster_living_room_fan_power are missing or not currently
available` failures at 21:38:44–21:39:48Z on 2026-08-04 — minutes *after* the successful
press — plus a `ResolveTimeoutAPIError` and an `EncryptionHelloAPIError`. The device
dropped **after** the press, and the recovery was explicitly not long enough to call it
settled.

The board is pinned at a **bare IP, `192.168.0.6`, with no DHCP reservation**
(`CHANGELOG.md:1446`, `:1468`). R1 does not depend on the HA link at all — it runs
entirely on-device — but the flash and the log stream do go over the network, so an
unexplained OTA failure or a log stream that dies mid-test is more likely this than a
fault in the test.

**Measured 2026-08-09, immediately before the R1 sitting.** The board answers — mDNS
resolves to `192.168.0.6`, OTA port 3232 is open, the web server returns 200 in 76 ms —
but the link is genuinely poor: **6.7 % packet loss over 15 pings, latency 32–242 ms,
stddev 50 ms.** One first-connection attempt to port 80 timed out and succeeded on retry.

Two things follow, and the second one matters for how you record the result:

- **A failed OTA is safe.** The partition table is dual-slot (`app0`/`app1` + `otadata`),
  so a flash writes to the *inactive* partition and only switches on success. A failure
  mid-write leaves the current firmware running. There is no brick risk here.
- **A failed OTA is AMBIGUOUS on this link.** It would not cleanly mean "OTA does not
  work on this board" — it could equally be the WiFi dropping mid-transfer. If OTA fails,
  say so *and* note that the link was measured lossy, rather than recording OTA as broken.
  Retrying once before falling back to USB is worth it.

## What each outcome unblocks

| outcome | next |
|---|---|
| **R1 PASS** | Phase 2 starts — HA code library, the three endpoints, the transmit counter. The watch sitting gets scheduled |
| **R1 PARTIAL** (capture, no replay) | Diagnosable from the logged array. Likely carrier frequency or an edge artefact. Not fatal |
| **R1 FAIL**, silent | A genuine fork: debug `remote_receiver` tuning, or fall back to a custom component. The Arduino sketches already prove the hardware works, so this would be software, not wiring |
