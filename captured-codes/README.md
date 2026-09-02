# Captured codes

Record IR codes here as you sniff them, so they survive re-flashes and can be
pasted into template buttons or the `send_raw_ir` service.

Suggested format — one entry per remote button:

## Captured (this build)

| Button | Protocol | Notes |
|--------|----------|-------|
| fan-speed-up | **Symphony** `data=0xD82`, `nbits=12` | **Captured 2026-09-02** (`ir-sniffer-blaster-dud`), 5/5 presses identical -> stable, not rolling. **Direction CONFIRMED on the running fan 2026-09-02**, and it is the OPPOSITE of the remote button that produced it -- see the warning below. Shipped as **raw**, not decoded. |
| fan-speed-down | **Symphony** `data=0xD92`, `nbits=12` | **Captured 2026-09-02** (`ir-sniffer-blaster-dud`), 5/5 presses identical -> stable, not rolling. Direction confirmed the same way, same inversion. |
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

### The direction came from WATCHING THE FAN, and it contradicts the remote button

At capture time the owner pressed **increase first** (producing `0xD92`) and
**decrease second** (producing `0xD82`). Firing them back on the running fan did
the **opposite**: `0xD92` steps the fan **down**, `0xD82` steps it **up**.

The actuation is the evidence. Which button a human believes they pressed is
not. **Do not "correct" this mapping back to match the capture note** -- that is
the mapping that was measured wrong, and it is the natural-looking one, so it
will invite correction more than once.

This is the second direction in this project documented backwards from a
plausible-but-untested assumption. The first was RX polarity (`inverted:` was
asserted at bring-up and believed for months until someone ran a capture, and
fixing it turned "unknown protocol" into Symphony `0xD81`). Same shape, same
cure: run it and look.

The YAML anchors are therefore named by **code** (`&sym_d82_raw`), never by
direction. A direction-named anchor is a second place to record the direction
and so a second place for it to be wrong while looking right.

**Confirmed 2026-09-02**, fan running and watched: 3x down gave **2** observable
decreases (the 3rd hit the floor and did nothing), then 3x up gave **3**
increases ending at **maximum**. **Both ends clamp; neither code wraps** -- so
SPEED + at maximum does nothing rather than dumping the fan to minimum, which is
the behaviour the watch app wants. *Inferred, not measured:* ~4 speed levels,
starting at level 3. Nobody has read a speed indicator, so do not treat the
level count as established.

### A run into a powered-off fan looks EXACTLY like a dead emitter

Recorded because it nearly sent a diagnosis down the wrong path. An earlier
confirmation run on 2026-09-02 fired seven speed presses while the fan was off.
Every machine-side signal was perfect -- HTTP 200 on all seven, and the board's
own receiver decoding each code correctly -- and the fan did nothing. That is
indistinguishable from the degraded-emitter regression in
`ir-sniffer-blaster-54y`.

**The owner does not leave the fan running between tests.** Start every sitting
by pressing `Fan Power` and confirming with a human that the fan actually
started, before sending a single speed code. The board can report what it
*sent*; only a person can report what the fan *did*.

### Why these ship as raw, not as the 3-field decoded form

`transmit_symphony` has never been shown to actuate this fan: the "Fan Power
(Symphony probe)" button has sat unpressed since 2026-08-09 and
`docs/hardware-gate-runbook.md` still lists that probe as open. Raw is the only
TX form with a physical result behind it -- and as of 2026-09-02 these two
buttons are part of that result. Probes for both new codes ship alongside, so
one sitting can settle the question on any of three codes. Flip to
`transmit_symphony` the day someone watches the fan move on a probe press.

### How the arrays were produced

**Derived from the Pronto dump in the same capture, not hand-timed**, and the
conversion was validated before it was trusted: run against that session's Fan
Power capture it reproduced the stored `&fan_raw` array on **46/47 entries
within 80 us**, the only outlier being the inter-frame gap (-7205 vs -7120,
1.2 percent on a 7 ms pause). Each array also decodes by mark/space ratio back
to the exact Symphony value the receiver reported, so two independent decoders
agree on them. Two frames each, matching `&fan_raw` and the remote's own
`command_repeats: 2`.

**fan-speed-up** -- Symphony `0xD82`, anchor `&sym_d82_raw`:

```
[1288, -447, 1236, -447, 394, -1288, 1236, -447, 1236, -447, 394, -1288, 394, -1288, 394, -1288, 394, -1288, 394, -1288, 1236, -447, 394, -8047, 1236, -447, 1236, -447, 394, -1288, 1236, -447, 1236, -447, 394, -1288, 394, -1288, 394, -1288, 394, -1288, 394, -1288, 1236, -447, 394]
```

**fan-speed-down** -- Symphony `0xD92`, anchor `&sym_d92_raw`:

```
[1288, -447, 1236, -447, 394, -1288, 1236, -447, 1236, -447, 394, -1288, 394, -1288, 1236, -447, 394, -1288, 394, -1288, 1236, -447, 394, -8047, 1236, -447, 1236, -447, 394, -1288, 1236, -447, 1236, -447, 394, -1288, 394, -1288, 1236, -447, 394, -1288, 394, -1288, 1236, -447, 394]
```

### The remote has DISCRETE up and down, not one cycling speed button

Checked on the physical remote 2026-09-02, because the watch's two-cell layout
depends on it, and confirmed independently by the codes themselves: up and down
are different values, which a single cycling button could not produce.

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
