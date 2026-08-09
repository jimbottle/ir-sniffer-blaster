# Captured codes

Record IR codes here as you sniff them, so they survive re-flashes and can be
pasted into template buttons or the `send_raw_ir` service.

Suggested format — one entry per remote button:

## Captured (this build)

| Button | Protocol | Notes |
|--------|----------|-------|
| fan-power-switch | **Symphony** `data=0xD81`, `nbits=12` | **Re-identified 2026-08-09 (R1).** Was recorded as "raw (UNKNOWN)" — that was an artefact of inverted RX polarity, not the remote. With `inverted: true` ESPHome decodes it cleanly. 28 identical captures → stable (not rolling). Verified toggling the fan on real hardware. |

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
