/*
 * CONTINUITY TESTER - a go/no-go circuit checker.
 * Board: Adafruit Feather ESP32 V2.  No external components needed.
 *
 * ---------------------------------------------------------------------------
 * WHAT THIS DOES
 *   Touch the two probe leads across a wire, fuse, switch, or solder joint.
 *     - LED on  + "CONTINUITY" -> the circuit is COMPLETE (current can flow)
 *     - LED off + "open"       -> the circuit is BROKEN
 *
 * WIRING (two jumper wires, nothing else)
 *   Probe A: a jumper in pin  32   <- the "sense" pin
 *   Probe B: a jumper in pin  GND  <- the ground reference
 *   Touch the free ends of the two jumpers together to test.
 *
 *   NOTHING ELSE SHOULD BE ATTACHED. If the 5990 IR board is wired to this
 *   Feather, unplug it: this sketch does not configure GPIO27, and an ESP32 pin
 *   you never configure comes out of reset FLOATING, which can gate the 5990's
 *   IR emitter on for the whole session.
 *
 * ---------------------------------------------------------------------------
 * READING THE SERIAL MONITOR   (Tools > Serial Monitor, set baud to 115200)
 *
 * This sketch prints a HEARTBEAT once a second, always, even when nothing is
 * happening. So:
 *
 *   - Lines ticking by  => the board is alive and the monitor is working.
 *   - Total silence     => something IS wrong (wrong baud, no upload, dead board).
 *
 * That's deliberate. The old version printed only when the state changed, so a
 * working board looked identical to a dead one.
 *
 * At boot it also prints WHY it last reset. If you see resets you didn't ask for,
 * that line names the culprit instead of leaving you guessing.
 *
 * Output is plain ASCII on purpose - fancy dashes turn into mojibake at the wrong
 * baud rate or across a dropped byte.
 *
 * ---------------------------------------------------------------------------
 * SAFETY - READ THIS
 *
 * 1. ONLY test UNPOWERED circuits.
 *    This probes with the ESP32's own 3.3V logic. If you touch a probe to
 *    something that has its own voltage on it (a battery, a plugged-in board),
 *    you inject that voltage straight into the GPIO pin. ESP32 pins are NOT
 *    5V tolerant (~3.6V max) and you can destroy the pin or the chip.
 *
 * 2. NEVER test continuity by driving one pin HIGH and another LOW and shorting
 *    them together. That ties the chip's internal power driver straight to its
 *    ground driver with nothing to limit the current. This sketch never does
 *    that: the sense pin is an INPUT, and the other lead goes to a real GND pin.
 * ---------------------------------------------------------------------------
 */

#include "esp_system.h"

// Why pin 32? Two reasons, both ESP32-specific:
//
//   * It has a usable internal pull-up. GPIO 34, 35, 36, 37, 38 and 39 are
//     INPUT-ONLY and have NO internal pull-up at all - INPUT_PULLUP silently does
//     nothing there, the pin floats, and this sketch would read garbage. Go by
//     GPIO NUMBER, not the "Ax" silkscreen label (those move between revisions).
//
//   * It isn't a "strapping" pin (0, 2, 5, 12, 15). The chip reads those at boot
//     to decide how to start, so holding one low with a probe could stop it booting.
const int SENSE_PIN = 32;

// LED_BUILTIN on this Feather is GPIO13: the small RED led (not the NeoPixel).
const int LED_PIN = LED_BUILTIN;

const unsigned long HEARTBEAT_MS = 1000;

int lastState = -1;              // -1 = nothing read yet, so the first pass always prints
unsigned long contacts = 0;      // how many times contact has been made
unsigned long lastBeat = 0;

// Ask the chip why it last restarted. If the board reboots while you're probing,
// this tells you what actually happened instead of leaving you to guess.
const char* resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "POWERON (power applied, or EN/reset pulled low)";
    case ESP_RST_EXT:       return "EXTERNAL reset pin";
    case ESP_RST_SW:        return "SOFTWARE restart";
    case ESP_RST_PANIC:     return "PANIC - the sketch crashed";
    case ESP_RST_BROWNOUT:  return "BROWNOUT - supply voltage sagged (weak USB/cable)";
    case ESP_RST_INT_WDT:   return "WATCHDOG (interrupt)";
    case ESP_RST_TASK_WDT:  return "WATCHDOG (task)";
    case ESP_RST_WDT:       return "WATCHDOG";
    case ESP_RST_DEEPSLEEP: return "wake from deep sleep";
    default:                return "UNKNOWN";
  }
}

void report(int state) {
  // digitalRead gives HIGH (~3.3V) or LOW (~0V).
  //
  // The internal pull-up holds the pin HIGH by default, so:
  //   LOW  = something connected the pin to GND -> circuit COMPLETE
  //   HIGH = nothing did                        -> circuit OPEN
  //
  // Note the inversion: a COMPLETED circuit reads LOW. It reads LOW *because*
  // completing it is what drags the voltage DOWN to ground.
  if (state == LOW) {
    Serial.print("[CONTINUITY] circuit COMPLETE  pin32=LOW   LED=ON   contacts=");
    Serial.println(contacts);
  } else {
    Serial.print("[   open   ] circuit BROKEN    pin32=HIGH  LED=off  contacts=");
    Serial.println(contacts);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);                       // let USB serial come up

  // INPUT_PULLUP = make it an input AND switch on a resistor inside the chip that
  // gently ties the pin to 3.3V. That internal resistor is the whole trick, and it
  // is why this needs no external parts.
  pinMode(SENSE_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  Serial.println();
  Serial.println("========================================");
  Serial.println(" Continuity Tester");
  Serial.println(" Probe A = pin 32    Probe B = GND");
  Serial.print(  " Last reset: ");
  Serial.println(resetReason());
  Serial.println(" Heartbeat prints every 1s: if lines are");
  Serial.println(" ticking, the board and monitor are OK.");
  Serial.println(" Only test circuits with the power OFF.");
  Serial.println("========================================");
}

void loop() {
  int state = digitalRead(SENSE_PIN);

  digitalWrite(LED_PIN, (state == LOW) ? HIGH : LOW);

  // Print immediately on a change - this is the event you care about.
  if (state != lastState) {
    if (state == LOW) contacts++;   // count each new contact
    Serial.print(">>> CHANGE  ");
    report(state);
    lastState = state;
    lastBeat = millis();            // don't also beat right after a change
  }

  // ...and print a heartbeat regardless, so silence always means something is wrong.
  if (millis() - lastBeat >= HEARTBEAT_MS) {
    Serial.print("    beat    ");
    report(state);
    lastBeat = millis();
  }

  delay(20);   // 50 reads/sec: instant to a human, and it rides over contact bounce
}
