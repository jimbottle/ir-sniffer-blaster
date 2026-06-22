# Constraints, gotchas & troubleshooting

## Hard constraints (not fixable in firmware)

- **Rolling codes won't replay.** Some remotes (noted for certain LG TVs; common on
  RF garage/car fobs) emit a different code each press to defeat replay. Static
  capture-and-replay cannot reproduce these. This is inherent to IR replay, not an
  ESP32 limitation.
- **Carrier frequency.** Most consumer IR is 38 kHz, which the 5990 receives and the
  config transmits. For odd-carrier remotes, use the **raw capture** (preserves
  timing) and set `carrier_frequency` to match on transmit.

## Wiring / hardware gotchas

- **`inverted: false` is correct** for the 5990's buffered output. Do **NOT** switch
  it to `inverted: true` — that setting is only for a bare TSOP38238 sensor, which
  this build does not use. (Symptom of getting it wrong: garbage or no decodes.)
- **The 5990 is NOT I2C**, despite the 4045 cable being labeled "I2C STEMMA." Do not
  plug it into the Feather's STEMMA QT port. Wire the four leads to GPIOs per
  [`docs/wiring.md`](wiring.md).
- **Power supply is flash-incapable.** The 4298 has a captive cable and supplies
  power only; the first flash needs a separate USB-C **data** cable.
- **Power the emitter from 5V (USB pin), not 3V3** — 3V3 works but cuts range.
- **Breadboard is the least rugged link** for a permanent fixture. If a connection
  gets flaky over time, solder the four 4045 leads directly to the Feather. Nothing
  in the firmware changes.

## Network gotchas

- **Static IP.** The config pins `192.168.0.150`. Change it to fit the LAN, or
  delete the `manual_ip:` block to use DHCP.
- If the device never appears in HA but the web server responds, check the
  `api_encryption_key` matches what HA has stored.

## ESPHome version note

This config uses the `api:` → `services:` block. **Recent ESPHome releases renamed
this to `api:` → `actions:`** (the old `services:` may warn or error depending on
version). If a build fails on that block, rename `services:` to `actions:` and
`service:` to `action:` — the structure is otherwise identical. Verify against the
[ESPHome native API docs](https://esphome.io/components/api.html) for your installed
version.

## First-boot checklist if nothing works

1. Compiles cleanly? (`esphome run` finishes with no errors.)
2. On WiFi? (appears on the router / pingable at the static IP.)
3. Web server up at `http://192.168.0.150/`?
4. Receiver: any decoded/raw line on a remote press in `esphome logs`?
5. Transmitter: does a known button actuate the real device?

Walk these in order — each depends on the previous. Full checklist in
[`docs/validation.md`](validation.md).
