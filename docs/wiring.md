# Wiring (no solder)

The Feather (5900) has male headers; it plugs into the breadboard straddling the
center channel, pins down. The 5990 connects via the 4045 cable, whose four tinned
wire ends push into breadboard holes. Each breadboard column of 5 holes is
internally tied, so a wire only needs to **share a column** with its target Feather
pin.

## Connection table

| 5990 lead (4045 color) | Function                          | Feather pin  |
|------------------------|-----------------------------------|--------------|
| **red** (V+)           | 5V power to emitter LEDs           | USB pin (5V) |
| **black** (GND)        | ground                            | any GND      |
| **white**              | IR data IN (MCU → emitter / TX)   | **GPIO27**   |
| **green**              | IR data OUT (receiver → MCU / RX) | **GPIO14**   |

These pins match the config: `remote_transmitter` → GPIO27, `remote_receiver` →
GPIO14.

## Notes

- **Power the emitter from the USB pin (5V), not 3V3**, for full range. Logic on
  the signal lines stays 3.3V from the GPIOs; the 5990 accepts 3–5V logic.
- If a tinned wire is loose in a breadboard hole, **pinch/fold the tip** to fatten
  it. The breadboard's spring clips help.
- **Wire to GPIOs, not the STEMMA QT port.** The 4045 cable is labeled "I2C
  STEMMA," but this build does not use I2C.

## Quick ASCII reference

```
  5990 transceiver                         ESP32 Feather V2
  ----------------                         ----------------
  red  (V+)  ───────────────────────────►  USB  (5V)
  black(GND) ───────────────────────────►  GND
  white(TX)  ───────────────────────────►  GPIO27   (remote_transmitter)
  green(RX)  ───────────────────────────►  GPIO14   (remote_receiver)
```

## Durability note

The breadboard is the least rugged link for a permanent fixture. If a connection
gets flaky over time, the upgrade is to **solder the four 4045 leads directly to
the Feather**. Nothing in the firmware changes if you do.
