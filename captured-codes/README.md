# Captured codes

Record IR codes here as you sniff them, so they survive re-flashes and can be
pasted into template buttons or the `send_raw_ir` service.

Suggested format — one entry per remote button:

## Captured (this build)

| Button | Protocol | Notes |
|--------|----------|-------|
| fan-power-switch | raw (UNKNOWN) | 28 identical captures → stable (not rolling). Verified toggling the fan on real hardware. In `ir-blaster-livingroom.yaml` as the "Fan Power" button. |

### fan-power-switch (raw, 38 kHz)

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
