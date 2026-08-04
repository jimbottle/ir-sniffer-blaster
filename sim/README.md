# Emitter-drive simulation

A small SPICE check that puts a real number on one design decision: the wiring
docs require powering the 5990's **VIN from 3V** (not 5V) on the 3.3V ESP32, to keep
the receiver output line pin-safe — see [`../docs/wiring.md`](../docs/wiring.md). The
tradeoff is reduced IR emitter range. This quantifies "reduced" so we can confirm 3V
is still fine for a single-room blaster.

## Run it

```bash
ngspice -b sim/emitter.cir      # ngspice v46+; brew install ngspice
```

## Result

| VIN | I per LED (ngspice) | Adafruit's published figure |
|-----|---------------------|-----------------------------|
| 3.0 V | 100 mA | ~100 mA ✓ |
| **3.3 V** (ESP32 rail) | **115 mA** | *(not published)* |
| 5.0 V | 202 mA | ~200 mA ✓ |

The fit lands on both anchors, and its 3V:5V ratio is **0.50** — reproducing Adafruit's
stated 3V-vs-5V halving. At 3.3V the emitter runs at **~57% of the 5V drive current**.
Since IR intensity scales with current and detection range with √intensity
(inverse-square), range at 3.3V is roughly **√0.57 ≈ 0.75×** the 5V range — so Adafruit's
"10+ m" at 5V becomes **~7–8 m** at 3.3V. Ample for a living-room blaster reaching the
TV, and the lower current sits further inside the LED/FET ratings than 5V does.

**Conclusion:** the `VIN = 3V` recommendation costs little real range and keeps the
receiver line pin-safe. Both docs are consistent with this.

## Scope / honesty

The exact 5990 BOM (series-R value, LED part, FET) isn't published, so `R1` and the
LED model in `emitter.cir` are **fitted to Adafruit's two stated operating points**
(~100 mA @ 3V, ~200 mA @ 5V). This model therefore doesn't independently *measure* the
board — its job is to **interpolate to 3.3V** (which Adafruit doesn't publish) and
confirm the physics is self-consistent with Adafruit's numbers. The fit hits ~100 mA @
3V and ~200 mA @ 5V (its 3V:5V ratio is ~0.50, matching Adafruit's halving), and reads
**~115 mA at 3.3V**. The robust takeaway is the **~0.57× current vs 5V** at 3.3V;
the exact milliamp figure moves a few mA with the model.

Sources: [Adafruit forum (5V/3V current & range)](https://forums.adafruit.com/viewtopic.php?t=223942),
[5990 product page](https://www.adafruit.com/product/5990),
[5990 pinouts](https://learn.adafruit.com/adafruit-infrared-ir-remote-transceiver/pinouts).
