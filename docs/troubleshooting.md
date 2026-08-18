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

- **`inverted: true` is correct** for the 5990 on this build. **This reverses what this
  file said until 2026-08-09**, which was: *"`inverted: false` is correct … do NOT switch
  it to `inverted: true` — that setting is only for a bare TSOP38238."* That was asserted
  from bring-up and **never tested** — the ESPHome receiver dump had not once been
  observed. R1 observed it and refuted it.
  - **Run 1 (`false`):** capture came back as the known-good fan code with **every sign
    flipped** — 24/24 leading entries inverted, magnitudes matching to 15.5 µs mean.
    Replaying it did not actuate the fan.
  - **Run 2 (`true`):** the captured array replayed through `transmit_raw` **actuated the
    fan**.
  - **Why:** the 5990's IRin idles HIGH and pulls LOW during a carrier burst, so `false`
    records idle as mark and inverts everything.
  - **The old symptom description was also wrong.** Getting it wrong does *not* give
    "garbage or no decodes" — it decodes fine and dumps a plausible-looking array. It is
    silently inverted, which is far harder to spot. Compare a capture against a known-good
    array rather than trusting that it looks reasonable.
- **The 5990 is NOT I2C**, despite the 4045 cable being labeled "I2C STEMMA." Do not
  plug it into the Feather's STEMMA QT port. Wire the four leads to GPIOs per
  [`docs/wiring.md`](wiring.md).
- **Power supply is flash-incapable.** The 4298 has a captive cable and supplies
  power only; the first flash needs a separate USB-C **data** cable.
- **Power VIN (red) from 3V, not 5V.** On the 5990, VIN is the receiver output's
  logic level; 5V into the 3.3V GPIO14 can damage the ESP32. 3V trades emitter range
  for safety. Don't run this board's VIN at 5V with a 3.3V MCU — see the range note
  in [`wiring.md`](wiring.md) (use a separate 5V emitter instead).
- **Breadboard is the least rugged link** for a permanent fixture. If a connection
  gets flaky over time, solder the four 4045 leads directly to the Feather. Nothing
  in the firmware changes.

## Network gotchas

- **Addressing.** The config uses **DHCP** — reserve the device's address on your
  router (find it in the boot log or the router's client list; MAC is on the ESP32).
  Reachable by name at `http://ir-blaster-livingroom.local/` regardless of IP. To pin
  a static IP in firmware instead, uncomment the `manual_ip:` block in the YAML.
- **Locked out? Use the fallback AP.** If the WiFi join fails (wrong creds, wrong
  band, out of range), the board hosts its own access point named
  **`IR-Blaster Fallback`** (password = `ap_password` from `secrets.yaml`). Connect to
  it and browse to the captive portal to fix the network config — no USB cable needed.
- If the device never appears in HA but the web server responds, check the
  `api_encryption_key` matches what HA has stored.

## ESPHome version note

`requirements.txt` pins **ESPHome 2026.6.2**, the version this config was validated
against (`esphome config` passes). It ships the **current** `api:` → `actions:` /
`action:` syntax. **Older ESPHome installs (pre-rename) used `api:` → `services:` /
`service:`.** If you unpin and a build fails on that block complaining about
`actions`, rename `actions:` back to `services:` and
`action:` to `service:` — the structure is otherwise identical, and the generated
HA service id (`esphome.ir_blaster_livingroom_send_raw_ir`) is the same either way.
Verify against the [ESPHome native API docs](https://esphome.io/components/api.html)
for your installed version.

## First-boot checklist if nothing works

1. Compiles cleanly? (`esphome run` finishes with no errors.)
2. On WiFi? (appears in the router's client list; the boot log prints the IP.) Note
   the ESP32 is **2.4GHz-only** — a 5GHz SSID shows as "no networks found".
3. Web server up at `http://ir-blaster-livingroom.local/`?
4. Receiver: any decoded/raw line on a remote press in `esphome logs`?
5. Transmitter: does a known button actuate the real device?
   **⚠️ If this is your symptom:** the blaster has been transmitting a correct,
   carrier-modulated `0xD81` that the fan ignores since 2026-08-17 ~23:48. Do not
   re-diagnose from scratch — `ir-sniffer-blaster-54y` records what is already
   eliminated (code content, amplitude, aim, delivery, repeat count, reboot).

Walk these in order — each depends on the previous. Full checklist in
[`docs/validation.md`](validation.md).
