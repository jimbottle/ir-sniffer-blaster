/*
 * CONTINUITY TESTER — a go/no-go circuit checker.
 * Board: Adafruit Feather ESP32 V2.  No external components needed.
 *
 * ---------------------------------------------------------------------------
 * WHAT THIS DOES
 *   Touch the two probe leads across a wire, fuse, switch, or solder joint.
 *     - LED on  + "CONTINUITY"  -> the circuit is COMPLETE (current can flow)
 *     - LED off + "OPEN"        -> the circuit is BROKEN
 *
 * WIRING (two jumper wires, nothing else)
 *   Probe A: a jumper in pin  32   <- the "sense" pin
 *   Probe B: a jumper in pin  GND  <- the ground reference
 *   Touch the free ends of the two jumpers together to simulate a closed circuit.
 *
 * ---------------------------------------------------------------------------
 * SAFETY — READ THIS
 *
 * 1. ONLY test UNPOWERED circuits.
 *    This probes with the ESP32's own 3.3V logic. If you touch a probe to
 *    something that has its own voltage on it (a battery, a plugged-in board),
 *    you inject that voltage straight into the GPIO pin. ESP32 pins are NOT
 *    5V tolerant (~3.6V max) and you can destroy the pin or the chip.
 *    Disconnect power from whatever you're testing. Every time.
 *
 * 2. NEVER test continuity by driving one pin HIGH and another LOW and
 *    shorting them together. That connects the chip's internal power driver
 *    straight to its ground driver with nothing to limit the current, and can
 *    physically damage the output drivers. This sketch never does that: the
 *    sense pin is an INPUT, and the other lead goes to the real GND pin.
 *    (If you ever need a second GPIO as a ground reference, it must be
 *    OUTPUT and driven LOW — never HIGH.)
 * ---------------------------------------------------------------------------
 */

// ---- Pin choices -----------------------------------------------------------
//
// Why pin 32? Three reasons, all ESP32-specific:
//
//   * It has a usable internal pull-up. GPIO 34, 35, 36, 37, 38 and 39 are
//     INPUT-ONLY on the ESP32 and have NO internal pull-up at all — INPUT_PULLUP
//     silently does nothing there, the pin floats, and this sketch would read
//     garbage. (Go by GPIO NUMBER, not the silkscreen label: the "Ax" labels map
//     to different GPIOs on different Feather revisions.)
//
//   * It isn't a "strapping" pin (0, 2, 5, 12, 15). The chip reads those at boot
//     to decide how to start up, so holding one low with a probe could stop the
//     board booting.
//
//   * It isn't used by the 5990 IR board (that uses GPIO14 and GPIO27), so you can
//     leave that hardware plugged in while you use this tester.
const int SENSE_PIN = 32;

// LED_BUILTIN is defined by the board package — on this Feather it's GPIO13, the
// small RED LED (not the NeoPixel). Using the name instead of "13" means the sketch
// would still work if you moved it to a different board.
const int LED_PIN = LED_BUILTIN;

// Not used by the tester — but if the 5990 IR board is still wired to this Feather,
// GPIO27 is the wire that switches its infrared emitter on. An ESP32 pin that you
// never configure comes out of reset as a FLOATING input: undefined, drifting, and
// liable to read as "on". That would quietly run the IR LEDs the whole time this
// sketch is up.
//
// The lesson (a real embedded one): if other hardware is attached to your board, put
// its pins in a known, safe state — even the ones your program doesn't care about.
// "I'm not using that pin" is not the same as "that pin is off".
const int IR_EMITTER_PIN = 27;

// Remembers what we saw last time around, so we only print when something actually
// changes instead of flooding the Serial Monitor 20x a second.
//
// -1 is a deliberate sentinel: digitalRead() can only ever return 0 or 1, so the
// FIRST comparison in loop() is guaranteed to differ. That forces exactly one print
// on startup, so you see the initial state instead of a blank screen.
int lastState = -1;

void setup() {
  // setup() runs ONCE, right after the board powers up or resets.
  // It's where you configure things.

  Serial.begin(115200);   // open the USB serial link at 115200 bits/sec.
                          // The Serial Monitor MUST be set to this same number.
  delay(500);             // give the USB connection a moment to come up

  // pinMode() tells the chip how to use a pin.
  //
  // INPUT_PULLUP does two things at once:
  //   1. makes the pin an input (we read it, we don't drive it), and
  //   2. switches on a resistor INSIDE the chip that gently ties the pin up
  //      to 3.3V.
  //
  // That internal resistor is the whole trick. With nothing connected, the pin
  // has no other influence, so the resistor wins and the pin sits at 3.3V =
  // HIGH. When you complete a circuit from the pin to GND, you give the voltage
  // a much easier path to 0V than through that weak resistor, so the pin gets
  // dragged down to 0V = LOW.
  //
  // This is why you need NO external parts: the pull-up resistor is already
  // inside the microcontroller.
  pinMode(SENSE_PIN, INPUT_PULLUP);

  // OUTPUT means we drive this pin: we set its voltage, we don't read it.
  pinMode(LED_PIN, OUTPUT);

  // Hold the 5990's IR emitter off, in case that board is still wired up. See the
  // comment on IR_EMITTER_PIN above: an unconfigured pin floats, and a floating gate
  // here could run the IR LEDs for the whole session. Harmless if nothing is attached.
  pinMode(IR_EMITTER_PIN, OUTPUT);
  digitalWrite(IR_EMITTER_PIN, LOW);

  Serial.println();
  Serial.println("=== Continuity Tester ===");
  Serial.println("Probe A = pin 32   Probe B = GND");
  Serial.println("Touch the probes together (or across a wire) to test.");
  Serial.println("Only test circuits with the power OFF.");
  Serial.println();
}

void loop() {
  // loop() runs over and over, forever, as fast as it can, for as long as the
  // board has power. This is where the actual work happens.

  // digitalRead() looks at the pin's voltage and gives back one of two values:
  //   HIGH -> the pin is near 3.3V
  //   LOW  -> the pin is near 0V
  int state = digitalRead(SENSE_PIN);

  // Here's the logic that makes this a continuity tester.
  //
  // Remember the pull-up is holding the pin HIGH by default. So:
  //   LOW  means something connected our pin to GND  -> circuit COMPLETE
  //   HIGH means nothing did                         -> circuit OPEN
  //
  // Note it's "backwards" from what you'd first guess: a COMPLETED circuit
  // reads LOW. That inversion trips up most beginners. It reads LOW *because*
  // completing the circuit is what pulls the voltage DOWN to ground.
  bool continuity = (state == LOW);

  // digitalWrite() sets an output pin's voltage. HIGH lights this LED.
  digitalWrite(LED_PIN, continuity ? HIGH : LOW);

  // Only print when the reading CHANGES. Without this guard we'd print
  // hundreds of identical lines per second and you couldn't read anything.
  if (state != lastState) {
    if (continuity) {
      Serial.println("*** CONTINUITY  — circuit is COMPLETE  (pin 32 pulled LOW)  [LED ON]");
    } else {
      Serial.println("    open        — circuit is BROKEN    (pin 32 sits HIGH)  [LED off]");
    }
    lastState = state;
  }

  // A short pause. Without it we'd read the pin millions of times a second for
  // no benefit. 50ms is 20 checks per second — instant to a human, and it also
  // smooths over the tiny electrical "bounce" you get as metal probes touch.
  delay(50);
}
