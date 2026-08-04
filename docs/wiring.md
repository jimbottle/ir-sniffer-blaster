# Wiring (no solder)

The Feather (5900) has male headers; it plugs into the breadboard straddling the
center channel, pins down. The 5990 connects via the 4045 cable, whose four tinned
wire ends push into breadboard holes. Each breadboard column of 5 holes is
internally tied, so a wire only needs to **share a column** with its target Feather
pin.

## Connection table

| 5990 lead (4045 color) | 5990 pin | Function                              | Feather pin |
|------------------------|----------|---------------------------------------|-------------|
| **red**                | VIN      | power **and logic-level reference**   | **3V**      |
| **black**              | GND      | ground                                | any GND     |
| **green**              | IRout    | emitter drive (MCU → LEDs) = **TX**   | **GPIO27**  |
| **white**              | IRin     | receiver output (LEDs → MCU) = **RX** | **GPIO14**  |

These pins match the config: `remote_transmitter` → GPIO27 (the emitter, green),
`remote_receiver` → GPIO14 (the receiver, white).

> **⚠️ VIN must be 3V, not 5V — and green/white are easy to swap.**
> The 5990 powers its receiver *and* LEDs from one VIN pin, and **its output logic
> level equals VIN** (Adafruit: "red wire is 3V~5V power *and logic level*"). On a
> 3.3V ESP32, powering VIN at 5V makes the receiver output (white) swing to ~5V into
> GPIO14 — over the ESP32's ~3.6V max, which can damage the pin. Adafruit's own
> Feather example says: *"wire the board's 3V pin to the transceiver VIN."*
>
> Also note the wire directions are the opposite of what the names suggest:
> **green (IRout)** is what you *drive* to fire the LEDs (transmit); **white (IRin)**
> is the demodulated signal coming *from* the receiver (receive). See
> [Adafruit pinouts](https://learn.adafruit.com/adafruit-infrared-ir-remote-transceiver/pinouts).

## Notes

- **Power VIN from 3V, not 5V.** VIN is the logic-level reference for the receiver
  output, so it must match the ESP32's 3.3V logic. This trades some emitter range
  for a pin-safe receiver line.
- **Do not run this board's VIN at 5V with the 3.3V ESP32.** Because VIN also powers
  the receiver, 5V puts ~5V on the white/IRin line the instant power is applied —
  before any firmware runs — so a level shifter added "later" can't save the pin, and
  even one wired at power-up must have its HV side on IRin and LV side on 3V/GPIO14
  exactly right. If you genuinely need more range than 3V gives, drive a **separate
  5V emitter** (e.g. Adafruit 5639) from its own 5V supply and keep the 5990 at 3V
  for receive — don't push this board's VIN to 5V.
- If a tinned wire is loose in a breadboard hole, **pinch/fold the tip** to fatten
  it. The breadboard's spring clips help.
- **Wire to GPIOs, not the STEMMA QT port.** The 4045 cable is labeled "I2C
  STEMMA," but this build does not use I2C.

## Quick ASCII reference

```
  5990 transceiver                         ESP32 Feather V2
  ----------------                         ----------------
  red  (VIN) ───────────────────────────►  3V   (NOT 5V — VIN sets the logic level)
  black(GND) ───────────────────────────►  GND
  green(IRout, TX) ─────────────────────►  GPIO27   (remote_transmitter → emitter)
  white(IRin,  RX) ─────────────────────►  GPIO14   (remote_receiver  ← receiver)
```

## Durability note

The breadboard is the least rugged link for a permanent fixture. If a connection
gets flaky over time, the upgrade is to **solder the four 4045 leads directly to
the Feather**. Nothing in the firmware changes if you do.
