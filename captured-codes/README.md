# Captured codes

Record IR codes here as you sniff them, so they survive re-flashes and can be
pasted into template buttons or the `send_raw_ir` service.

Suggested format — one entry per remote button:

## Example: Living Room TV (LG)

| Button     | Protocol | Address  | Command  | Notes                    |
|------------|----------|----------|----------|--------------------------|
| Power      | NEC      | 0x20DF   | 0x10EF   | decoded cleanly          |
| Volume Up  | raw      | —        | —        | see raw array below      |

### Volume Up (raw, 38 kHz)

```
[9000, -4500, 560, -560, 560, -1690, 560, -560, ...]
```

---

Tips:
- Capture with `esphome logs ir-blaster-livingroom.yaml` (DEBUG + `dump: all` are
  already on). See [`docs/capture-replay.md`](../docs/capture-replay.md).
- Prefer the **decoded** form when the log decodes it; fall back to the **raw**
  array otherwise.
- Note any button whose code **changes every press** — that's a rolling code and
  cannot be replayed (see [troubleshooting](../docs/troubleshooting.md)).
