# Hardware gate runbook — ✅ COMPLETE

> **The hardware gate closed on 2026-08-09. R15 ✅ R13 ✅ R1 ✅ OTA ✅.**
> Phase 2 is unblocked and the watch sitting is schedulable.
>
> **Nothing in the "YOUR PART" steps below needs doing again.** They are kept as the
> record of how R1 was run and what it produced. What R1 found — RX polarity documented
> backwards, and the fan remote decoding as Symphony rather than "unknown" — is in
> [`docs/validation.md`](validation.md) and [`captured-codes/`](../captured-codes/).
>
> **Still open, and each is cheap:**
> - **R11 sampling** — see [below](#r11-sampling--no-test-firmware-needed). No firmware
>   needed; the correction to that section is the whole reason to still open this file.
> - **Symphony TX probe** — flash production and press "Fan Power (Symphony probe)" in
>   HA. Decides whether the code library can be protocol-first.
> - **`send_raw_ir`** — the last unticked item in `validation.md`. HA → Developer Tools →
>   Actions. No flash.

R1 was **the** gate: everything in `docs/bangle-integration.md` was speculative until it
passed, which is why it ran before any watch work (the H6 decision, 2026-08-07).

**Time it took:** ~10 minutes plus one extra flash, since run 1 exposed the polarity bug.

---

# YOUR PART — four steps *(HISTORICAL — R1 passed, do not re-run)*

The procedure as it was run on 2026-08-09. Kept for the record and in case R1 ever needs
repeating on new hardware.

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

# What run 1 and run 2 actually returned *(historical)*

- **Run 1 (`inverted: false`) — the capture was sign-flipped**, 0/24 matching the
  known-good array. No decode, no actuation. This is what exposed the polarity bug.
- **Run 2 (`inverted: true`) — 24/24 matching** within 19.2 µs mean / 29 µs max, decoded
  on the first press as `Symphony data=0xD81, nbits=12`, and the replay **actuated the
  fan**.
- **OTA worked**, twice: 27.48 s then 11.17 s.
- **A capture holds ~2.5 repeats, not one frame** — Symphony's `command_repeats` defaults
  to 2. Single-frame extraction is a real library requirement.

---

# R11 sampling — no test firmware needed

**Corrected 2026-08-09.** This section used to say: *"The test image captures once per
boot — a deliberate latch. To capture another remote, power-cycle the board, then press
a different remote."* **That procedure is unnecessary — do not follow it.**

The **production** firmware runs `dump: all`, so it already prints protocol + raw
timings for **every** remote press, with no latch and no reboot between them. Sampling
is just:

1. Run production (`.venv/bin/esphome run ir-blaster-livingroom.yaml`) — it streams logs
   after flashing, or reattach with `.venv/bin/esphome logs ir-blaster-livingroom.yaml`.
2. Point each remote at the box and press. One press per button is enough.
3. Copy the log.

The once-per-boot latch is a property of `ir-roundtrip-test.yaml` only, and it exists
there to stop the R1 test capturing its own replay. It has nothing to do with sampling.

Nearly free, and it sizes the whole library design. Most useful, in order:

1. **Any HVAC / mini-split / window AC remote.** These send full state in one frame and
   commonly run 100–450 entries against the fan's 47. If one exists in this house it is
   the single most informative capture — it is what `receive_symbols: 256` was set for,
   and the one case where the stock 192 would have silently truncated.
2. TV or soundbar power.
3. Anything that prints a **protocol name** rather than only raw timings. This is now the
   highest-value thing sampling can find: the fan remote turned out to be **Symphony
   `0xD81`/12**, not the "unknown" it was documented as, and a decoded record is ~3 fields
   against a 120-entry array. Every remote that decodes is one that avoids raw entirely.

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

## What the outcome unblocked

**R1 PASSED**, so: Phase 2 starts — the HA code library, the three endpoints, and the
transmit counter — and the watch sitting (H5 sitting 2) is schedulable.

*The other branches, kept in case R1 is ever re-run on new hardware:* a PARTIAL (capture
but no replay) is diagnosable from the logged array, likely carrier frequency or an edge
artefact, and not fatal. A silent FAIL is a genuine fork — debug `remote_receiver`
tuning, or fall back to a custom component; the Arduino sketches already prove the
hardware works, so it would be software rather than wiring.
