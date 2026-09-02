# Captured codes

Record IR codes here as you sniff them, so they survive re-flashes and can be
pasted into template buttons or the `send_raw_ir` service.

Suggested format — one entry per remote button:

## Captured (this build)

| Button | Protocol | Notes |
|--------|----------|-------|
| fan-speed-up | **Symphony** `data=0xD92`, `nbits=12` | **Captured 2026-09-02** (`ir-sniffer-blaster-dud`). 5/5 presses identical -> stable, not rolling. Same session re-captured fan power as `0xD81`, so the session is calibrated against a known code. Shipped as **raw** (see below), not decoded -- `transmit_symphony` still has no physical result behind it. **NOT yet confirmed to move the fan**; the board decoding its own transmission proves code content, not actuation. |
| fan-speed-down | **Symphony** `data=0xD82`, `nbits=12` | **Captured 2026-09-02** (`ir-sniffer-blaster-dud`). 5/5 presses identical -> stable, not rolling. Same caveats as the row above. |
| fan-power-switch | **Symphony** `data=0xD81`, `nbits=12` | **Re-identified 2026-08-09 (R1).** Was recorded as "raw (UNKNOWN)" — that was an artefact of inverted RX polarity, not the remote. With `inverted: true` ESPHome decodes it cleanly. 28 identical captures → stable (not rolling). Verified toggling the fan on real hardware **2026-08-09; the fan has not responded since 2026-08-17 ~23:48 (`ir-sniffer-blaster-54y`). Code content is ELIMINATED by measurement — the emitted signal is bit-identical to the real remote — so do NOT re-capture on that hypothesis.** |

### ⚠️ It is NOT an unknown protocol — that was a polarity artefact

The whole project was built around "this remote decodes as UNKNOWN, so we use raw."
**That is false.** It was never tested with correct receiver polarity, and the ESPHome
receiver dump had never once been observed. R1 run 2 (2026-08-09, `inverted: true`)
decoded it on the first press:

```
[I][remote.symphony:117]: Received Symphony: data=0xD81, nbits=12
[I][remote.rc_switch:260]: Received RCSwitch Raw: protocol=1 data='11011000000'
```

**This is a large simplification for the code library.** ESPHome has a matching
`transmit_symphony` action (`remote_base/__init__.py:1173`) taking `data`, `nbits` and
`command_repeats` (default 2):

```yaml
- remote_transmitter.transmit_symphony:
    data: 0xD81
    nbits: 12
```

Three small fields instead of a 120-entry timing array — roughly a 50× payload
reduction, which matters directly for the watch's response-size limit (R2). Prefer the
decoded form; keep raw as the fallback for remotes that genuinely do not decode.

`command_repeats: 2` also explains the capture length: the remote repeats the frame, so
a single press yields ~2.5 frames, not one.

### fan-speed-up / fan-speed-down (raw, 38 kHz) -- PRODUCTION FORM

**These ship as raw even though they decode cleanly, and that is deliberate.**
`transmit_symphony` has never been shown to actuate this fan: the "Fan Power
(Symphony probe)" button has sat unpressed since 2026-08-09 and
`docs/hardware-gate-runbook.md` still lists that probe as open. Raw is the only
TX form with a physical result behind it, so the working buttons use it and the
decoded form stays a probe. Flip them to `transmit_symphony` the day someone
watches the fan move on a probe press -- not before.

**The arrays were DERIVED from the Pronto dump in the same capture, and the
conversion was validated before it was trusted.** Run against that session's
Fan Power capture it reproduced the stored `&fan_raw` array on **46/47 entries
within 80 us**, the only outlier being the inter-frame gap (-7205 vs -7120,
1.2% on a 7 ms pause). Each array also decodes by mark/space ratio back to the
exact Symphony value the receiver reported, so two independent decoders agree.
Two frames each, matching `&fan_raw` and the remote's own `command_repeats: 2`.

Verified device-side 2026-09-02, immediately after the OTA: pressing each new
button made the board's own receiver decode `0xD92` and `0xD82` respectively --
bit-identical to what the real remote sent. **That proves code content and that
something left the emitter. It does NOT prove the fan responds** -- this
receiver sits inches from the emitter and cannot discriminate carrier (it
decoded a 20 kHz carrier 2/3 of the time, 2026-08-18), so only a human watching
the fan settles actuation.

fan-speed-up, `data=0xD92`:

```
[1288, -447, 1236, -447, 394, -1288, 1236, -447, 1236, -447, 394, -1288, 394, -1288, 1236, -447, 394, -1288, 394, -1288, 1236, -447, 394, -8047, 1236, -447, 1236, -447, 394, -1288, 1236, -447, 1236, -447, 394, -1288, 394, -1288, 1236, -447, 394, -1288, 394, -1288, 1236, -447, 394]
```

fan-speed-down, `data=0xD82`:

```
[1288, -447, 1236, -447, 394, -1288, 1236, -447, 1236, -447, 394, -1288, 394, -1288, 394, -1288, 394, -1288, 394, -1288, 1236, -447, 394, -8047, 1236, -447, 1236, -447, 394, -1288, 1236, -447, 1236, -447, 394, -1288, 394, -1288, 394, -1288, 394, -1288, 394, -1288, 1236, -447, 394]
```

### The remote has DISCRETE up and down, not one cycling speed button

Checked on the physical remote 2026-09-02, because the watch's two-cell layout
depends on it. Confirmed by the codes themselves: up and down are different
values (`0xD92` vs `0xD82`), which a single cycling button could not produce.

---

### fan-power-switch (raw fallback, 38 kHz)

*Prefer the Symphony form above. This raw array is kept as the fallback and as the
reference other captures are checked against.*

**Verified against a live ESPHome capture 2026-08-09 (R1 run 2):** 24/24 leading signs
match and magnitudes agree to 19.2 µs mean / 29 µs max. That comparison is what proved
the polarity fix — run 1, with `inverted: false`, matched 0/24.

IRremote hash `0xDA0AE878`, 47 entries. ESPHome `transmit_raw` format (+mark / -space, µs):

```
[1280, -420, 1230, -420, 430, -1270, 1230, -420, 1230, -420, 380, -1270, 380, -1270, 430, -1220, 430, -1270, 380, -1270, 380, -1270, 1230, -7120, 1230, -420, 1230, -420, 430, -1270, 1230, -420, 1230, -420, 380, -1270, 380, -1270, 430, -1220, 430, -1270, 380, -1270, 380, -1270, 1230]
```

Note the ~7120 µs mid-frame gap — this remote sends the code as two halves with a
gap between them. The raw replay reproduces it exactly.

---

## Example format (for reference)

| Button     | Protocol | Address  | Command  | Notes                    |
|------------|----------|----------|----------|--------------------------|
| Power      | NEC      | 0x20DF   | 0x10EF   | decoded cleanly          |

---

Tips:
- Capture with `esphome logs ir-blaster-livingroom.yaml` (DEBUG + `dump: all` are
  already on). See [`docs/capture-replay.md`](../docs/capture-replay.md).
- Prefer the **decoded** form when the log decodes it; fall back to the **raw**
  array otherwise.
- Note any button whose code **changes every press** — that's a rolling code and
  cannot be replayed (see [troubleshooting](../docs/troubleshooting.md)).
