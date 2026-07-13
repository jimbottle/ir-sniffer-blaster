/*
 * CONTINUITY TESTER - a go/no-go circuit checker.
 * Board: Adafruit Feather ESP32 V2.  No external components needed.
 *
 * ===========================================================================
 * WHAT IT DOES
 *   Touch the two probe leads across a wire, fuse, switch, or solder joint.
 *     LED on  + "CONTINUITY" -> circuit is COMPLETE (current can flow)
 *     LED off + "open"       -> circuit is BROKEN
 *
 * WIRING (two jumper wires, nothing else)
 *   Probe A: a jumper in pin  32   <- the "sense" pin
 *   Probe B: a jumper in pin  GND  <- the ground reference
 *   Touch the free ends together to test.
 *
 *   Nothing else should be attached. If the 5990 IR board is wired to this
 *   Feather, unplug it: this sketch doesn't configure GPIO27, and an ESP32 pin
 *   you never configure comes out of reset FLOATING, which can gate the 5990's
 *   IR emitter on.
 *
 * SERIAL MONITOR:  Tools > Serial Monitor, and set the baud dropdown to 115200.
 *   It MUST match Serial.begin(115200) below. At the wrong rate every byte is
 *   sliced at the wrong bit boundary and you get garbage like "?? ? ??".
 *   (The IDE likes to reset that dropdown to 9600. If you see garbage, check it
 *   first - it has probably reverted.)
 *
 * ===========================================================================
 * THE ONE IDEA THAT MAKES THIS WORK
 *
 * A pin connected to nothing does NOT read as 0 or 1. It "floats" - it picks up
 * stray electrical noise and reads randomly. Useless. So how do you detect
 * "connected vs not connected"?
 *
 * Give the pin a DEFAULT, then see whether the circuit overrides it.
 *
 * INPUT_PULLUP switches on a resistor INSIDE the chip that gently ties the pin up
 * toward 3.3V. It's weak on purpose - a suggestion, not a command.
 *
 *   Nothing connected -> nothing else influences the pin, the weak resistor wins,
 *                        pin sits at 3.3V -> reads HIGH -> "open"
 *
 *   Probes bridged    -> you have handed the pin an easy path down to GND (0V).
 *                        That path beats the weak resistor easily, so the pin is
 *                        dragged to 0V -> reads LOW -> "CONTINUITY"
 *
 *      3.3V
 *        |
 *       [R]   <- internal pull-up, switched on by INPUT_PULLUP
 *        |
 *        +---- pin 32 ---- probe A ----+
 *        |                              |  circuit under test
 *       GND ------------- probe B ------+
 *
 * THE COUNTERINTUITIVE BIT: a COMPLETED circuit reads LOW, not HIGH. It reads LOW
 * *because* completing the circuit is what drags the voltage DOWN to ground. Sit
 * with that until it clicks - the same inverted logic is how nearly every button
 * on every Arduino project works.
 *
 * And that internal resistor is why this needs NO external parts: the component
 * you would normally have to add is already inside the microcontroller.
 *
 * ===========================================================================
 * REBUILD IT FROM SCRATCH  (the exercise - try this in a new, empty sketch)
 *
 *   1. Serial.begin(115200)          - open a text channel back to the computer,
 *                                      so the program can tell you what it sees.
 *   2. pinMode(pin, INPUT_PULLUP)    - make it an input AND switch on the internal
 *                                      pull-up. This is the whole trick.
 *   3. pinMode(LED_BUILTIN, OUTPUT)  - we drive the LED, so it's an output.
 *   4. In loop(): digitalRead(pin)   - ask "is this pin near 3.3V (HIGH) or 0V (LOW)?"
 *   5. LOW == continuity             - remember the inversion above.
 *   6. digitalWrite the LED          - show the answer without needing the monitor.
 *   7. Only print when it CHANGES    - compare against the previous reading. Without
 *                                      this, loop() spams identical lines forever.
 *   8. Seed lastState = -1           - digitalRead only ever returns 0 or 1, so the
 *                                      first comparison always differs. That forces
 *                                      exactly one line at startup, so you see the
 *                                      initial state instead of a blank screen.
 *   9. delay(20)                     - don't read pointlessly fast; also rides over
 *                                      "contact bounce" (two metal probes actually
 *                                      make and break several times in a few ms).
 *
 *   Then extend it: invert the LED; count the contacts; blink instead of solid
 *   (meet millis()); time how long contact was held.
 *
 * ===========================================================================
 * SAFETY - READ THIS
 *
 * 1. ONLY test UNPOWERED circuits. This probes with the ESP32's own 3.3V logic.
 *    Touch a probe to something carrying its own voltage (a battery, a live board)
 *    and you inject that voltage straight into the GPIO. ESP32 pins are NOT 5V
 *    tolerant (~3.6V max) - you can destroy the pin or the chip.
 *
 * 2. NEVER test continuity by driving one pin HIGH and another LOW and shorting
 *    them together. That ties the chip's internal power driver straight to its
 *    ground driver with nothing to limit the current, and can damage the output
 *    drivers. This sketch never does that: the sense pin is only ever an INPUT,
 *    and the other lead goes to a real GND pin (which is built to sink current).
 * ===========================================================================
 */

#include "esp_system.h"   // only for esp_reset_reason() - a diagnostic, not core

// ---- Pin choices ----------------------------------------------------------
//
// Why pin 32? Two reasons, both ESP32-specific and both worth knowing:
//
//   * It has a usable internal pull-up. GPIO 34, 35, 36, 37, 38 and 39 are
//     INPUT-ONLY and have NO internal pull-up at all - INPUT_PULLUP silently does
//     nothing there, the pin floats, and this sketch would read garbage. Go by the
//     GPIO NUMBER, not the "Ax" silkscreen label: those labels map to different
//     GPIOs on different Feather revisions. (No generic Arduino tutorial warns you
//     about this - on a real Uno, every pin has a pull-up.)
//
//   * It isn't a "strapping" pin (0, 2, 5, 12, 15). The chip samples those at boot
//     to decide how to start up, so holding one low with a probe could stop it
//     booting at all.
const int SENSE_PIN = 32;

// LED_BUILTIN is defined by the board package. On this Feather it is GPIO13: the
// small RED led, not the NeoPixel. Using the name rather than "13" means the sketch
// still works if you move it to a different board.
const int LED_PIN = LED_BUILTIN;

// ---- State that has to survive between passes through loop() ---------------
//
// loop() runs over and over. Anything you want to REMEMBER from one pass to the
// next must live out here, not inside loop() - a variable declared inside loop()
// is created fresh and destroyed on every single pass.
//
// -1 is a deliberate sentinel: digitalRead() can only ever return 0 (LOW) or 1
// (HIGH), so the very first comparison in loop() is guaranteed to differ. That
// forces exactly one line to print at startup.
int lastState = -1;

unsigned long contacts = 0;   // how many times contact has been made this session


// Diagnostic only: ask the chip why it last restarted. Handy if the board reboots
// unexpectedly - this names the cause instead of leaving you to guess. Not part of
// the core idea; delete it freely when you rebuild this.
const char* resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "POWERON (power applied, or reset pressed)";
    case ESP_RST_EXT:       return "EXTERNAL reset pin";
    case ESP_RST_SW:        return "SOFTWARE restart";
    case ESP_RST_PANIC:     return "PANIC - the sketch crashed";
    case ESP_RST_BROWNOUT:  return "BROWNOUT - supply voltage sagged";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:       return "WATCHDOG";
    default:                return "unknown";
  }
}


void setup() {
  // setup() runs ONCE, right after power-on or reset. It is where you configure
  // things. It is not a loop, and it never runs again.

  Serial.begin(115200);   // open the USB text channel at 115200 bits/sec.
  delay(500);             // give USB serial a moment to come up before printing

  // pinMode() tells the chip how to use a pin.
  //
  // INPUT_PULLUP does two things at once:
  //   1. makes the pin an INPUT (we read it; we never drive it), and
  //   2. switches on the internal pull-up resistor described at the top.
  pinMode(SENSE_PIN, INPUT_PULLUP);

  // OUTPUT means the opposite: we set this pin's voltage, we don't read it.
  pinMode(LED_PIN, OUTPUT);

  Serial.println();
  Serial.println("=========================================");
  Serial.println(" Continuity Tester");
  Serial.println(" Probe A = pin 32     Probe B = GND");
  Serial.print(  " Last reset: ");
  Serial.println(resetReason());
  Serial.println(" Only test circuits with the power OFF.");
  Serial.println("=========================================");

  // Nothing is printed here about the current state on purpose - the first pass
  // through loop() does that, because lastState is seeded to -1.
}


void loop() {
  // loop() runs over and over, forever, as fast as it can, for as long as the board
  // has power. There is no main() and no exit. This is where the work happens.

  // digitalRead() looks at the pin's voltage and answers a yes/no question:
  //   HIGH -> the pin is near 3.3V
  //   LOW  -> the pin is near 0V
  int state = digitalRead(SENSE_PIN);

  // The pull-up holds the pin HIGH by default, so LOW means something connected it
  // to ground -> the circuit is complete. (Re-read "THE COUNTERINTUITIVE BIT" above
  // if this still feels backwards. It should, at first.)
  bool continuity = (state == LOW);

  // digitalWrite() sets an output pin's voltage. HIGH lights this LED.
  // This happens every pass, so the LED always tracks reality - no waiting for a
  // print, and it works even with no Serial Monitor open at all.
  digitalWrite(LED_PIN, continuity ? HIGH : LOW);

  // ---- Print ONLY on a change -------------------------------------------------
  //
  // This is the "edge detection" idea, and it matters. loop() runs ~50 times a
  // second. Printing every pass would flood the monitor with identical lines and
  // you could not read anything. Comparing against the PREVIOUS reading means we
  // only speak when something actually happened:
  //
  //   - one line at startup   (because lastState starts at -1, which never matches)
  //   - one line each time contact is MADE
  //   - one line each time contact is BROKEN
  //
  // Between events: silence. Silence here means "nothing has changed", which is the
  // correct and useful thing for a tool like this to say.
  if (state != lastState) {
    if (continuity) {
      contacts++;                       // count each new contact
      Serial.print("CONTINUITY  circuit COMPLETE   pin32=LOW    LED=ON    contacts=");
      Serial.println(contacts);
    } else {
      Serial.println("open        circuit BROKEN     pin32=HIGH   LED=off");
    }
    lastState = state;                  // remember it, so we don't print it again
  }

  // A short pause. Without it we would read the pin millions of times a second for
  // no benefit. 20ms = 50 checks/second: instant to a human, and it smooths over the
  // tiny electrical "bounce" you get as two metal probes actually touch.
  delay(20);
}
