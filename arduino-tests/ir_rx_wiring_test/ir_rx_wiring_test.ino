// STAGE 1 — IR receiver WIRING test.  No libraries required.
//
// Board : Adafruit Feather ESP32 V2      Sensor: Adafruit 5990 IR transceiver
// Green (IR data OUT, receiver -> MCU) -> GPIO14
// White (IR data IN,  MCU -> emitter)  -> GPIO27   (held LOW here; emitter stays off)
// Red   (V+)  -> USB pin (5V)     Black (GND) -> GND
//
// This does NOT decode IR. It answers one question: is the receiver wired and alive?
//
// Serial Monitor @ 115200 baud.
//
// ---------------------------------------------------------------------------
// HOW TO READ IT
//
// The sketch prints a static probe, then waits for edges. THE EDGES ARE THE VERDICT.
//
//   "DRIVEN HIGH"  -> sensor is connected and actively holding the line high. Good.
//   "DRIVEN LOW"   -> something is holding GPIO14 at ground. Suspect a short or a
//                     miswired lead.
//   "UNDETERMINED" -> the static probe CANNOT tell these two apart:
//                       (a) nothing connected (pin floating), or
//                       (b) a connected sensor whose output idles high through its
//                           own weak internal pull-up (open-drain, typical of
//                           TSOP-class demodulators).
//                     Both look identical to a static read. To settle it:
//                       PRESS A REMOTE.  Edges => connected.  No edges => not connected.
//
// So: press a remote at the sensor. That is the real test.
//   Edges appear    -> receive path works. Wiring is GOOD.
//   No edges at all -> sensor not connected / unpowered / green wire not on GPIO14,
//                      or the remote is dead / not 38 kHz / not aimed at the sensor.
//   Edges with NO remote pressed -> noise on the line (the emitter can't be the
//                      cause; we hold GPIO27 low).
// ---------------------------------------------------------------------------

const int IR_RX_PIN = 14;   // green — IR data OUT (receiver -> MCU)
const int IR_TX_PIN = 27;   // white — IR data IN  (MCU -> emitter)

volatile unsigned long edgeCount = 0;

void IRAM_ATTR onIrEdge() {
  edgeCount++;
}

// Read the pin under a given input mode, several times, and only report a level if
// every sample agrees. Returns HIGH, LOW, or -1 for "unstable" (the line was moving
// while we looked — e.g. a remote is being pressed, or a fluorescent/PWM lamp is
// hitting the sensor).
int probeStable(int mode) {
  pinMode(IR_RX_PIN, mode);
  delay(5);                       // let the line settle
  int first = digitalRead(IR_RX_PIN);
  for (int i = 0; i < 8; i++) {
    delay(2);
    if (digitalRead(IR_RX_PIN) != first) return -1;   // it moved: not stable
  }
  return first;
}

// Try to characterize what (if anything) is on GPIO14.
//
// The idea: toggle the ESP32's internal pull-down / pull-up and see whether the pin
// FOLLOWS them. A floating pin obeys whatever weak resistor we enable.
//
// IMPORTANT LIMIT — why this cannot be the final word:
// It is tempting to conclude "follows the resistors => nothing is connected." That is
// only true if the sensor drives the line hard (push-pull). Many IR demodulators use
// an OPEN-DRAIN output with a ~30k internal pull-up: they only pull DOWN actively and
// let a weak resistor supply the idle high. Against the ESP32's ~45k pull-down, that
// forms a divider around 2.0V — under the ~2.48V logic-high threshold — so a perfectly
// good sensor reads LOW too, and looks exactly like a floating pin.
//
// We have not confirmed the 5990's output stage, so we DO NOT claim "not connected"
// from this probe alone. We report it as undetermined and let the edge count decide.
void reportConnection() {
  int withPulldown = -1;
  int withPullup   = -1;

  // Retry a few times before giving up. An open-drain sensor sits near the logic
  // threshold against our pull-down, so a marginal line can legitimately flap; one
  // bad sample shouldn't force the user to hit the reset button.
  for (int attempt = 1; attempt <= 3; attempt++) {
    withPulldown = probeStable(INPUT_PULLDOWN);
    withPullup   = probeStable(INPUT_PULLUP);
    if (withPulldown >= 0 && withPullup >= 0) break;
    Serial.print("    (unstable reading, retrying ");
    Serial.print(attempt);
    Serial.println("/3...)");
    delay(250);
  }
  pinMode(IR_RX_PIN, INPUT);          // restore plain input for the real test

  Serial.print("Static probe — pulldown/pullup reads: ");
  Serial.print(withPulldown < 0 ? "?" : String(withPulldown));
  Serial.print("/");
  Serial.println(withPullup < 0 ? "?" : String(withPullup));

  if (withPulldown < 0 || withPullup < 0) {
    Serial.println(">>> UNSTABLE — the line kept moving while we probed.");
    Serial.println(">>> Make sure no remote is being pressed and no flickering lamp is");
    Serial.println(">>> on the sensor, then press the board's RESET button to retry.");
    Serial.println(">>> (You can ignore this and just press a remote — edges still count.)");
  } else if (withPulldown == HIGH && withPullup == HIGH) {
    Serial.println(">>> DRIVEN HIGH — something is actively holding GPIO14 high.");
    Serial.println(">>> The 5990 is connected and idle. This is correct.");
  } else if (withPulldown == LOW && withPullup == LOW) {
    Serial.println(">>> DRIVEN LOW — something is holding GPIO14 at ground.");
    Serial.println(">>> Suspect a short to GND, or a miswired lead.");
  } else if (withPulldown == LOW && withPullup == HIGH) {
    // The pin followed whichever internal resistor we enabled.
    Serial.println(">>> UNDETERMINED — the pin followed our internal resistors.");
    Serial.println(">>> That means EITHER (a) nothing is connected (floating), OR");
    Serial.println(">>> (b) the sensor IS connected but idles high through its own");
    Serial.println(">>> weak pull-up (open-drain output). A static read cannot tell");
    Serial.println(">>> these apart.");
    Serial.println(">>> ---> PRESS A REMOTE. Edges = connected. No edges = not connected.");
  } else {
    // pulldown=HIGH, pullup=LOW — the pin read the OPPOSITE of the resistor we
    // applied, both times. That is not physically sensible for a passive line.
    Serial.println(">>> CONTRADICTORY — the pin read the inverse of each resistor we");
    Serial.println(">>> applied (high when pulled down, low when pulled up). That is not");
    Serial.println(">>> a normal passive line. Check for a miswired/inverted lead, then");
    Serial.println(">>> press RESET to retry. Press a remote — the edge count still rules.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(800);                      // let USB serial settle

  // Silence the emitter FIRST, before we measure anything.
  //
  // GPIO27 gates the 5990's emitter, and the emitter sits on the SAME board as the
  // receiver. ESP32 pins come out of reset as floating inputs, so if we leave GPIO27
  // undriven it can float high/noisy, fire the 940nm LEDs, and flood our own
  // demodulator — producing endless edges with no remote in the room. That would
  // look exactly like a receiver fault. Driving it LOW holds the emitter off.
  pinMode(IR_TX_PIN, OUTPUT);
  digitalWrite(IR_TX_PIN, LOW);

  Serial.println();
  Serial.println("=== IR RX wiring test — GPIO14 (Adafruit 5990) ===");
  Serial.println("(GPIO27 held LOW: emitter off, so it cannot spoof the receiver.)");

  reportConnection();

  attachInterrupt(digitalPinToInterrupt(IR_RX_PIN), onIrEdge, CHANGE);

  Serial.println();
  Serial.println("NOW PRESS A REMOTE at the 5990 — this is the real test.");
  Serial.println();
}

void loop() {
  noInterrupts();
  unsigned long count = edgeCount;
  edgeCount = 0;
  interrupts();

  if (count > 0) {
    // Re-sample the level too: with no remote pressed the line should sit at idle
    // between bursts. Edges that keep coming when you AREN'T pressing a remote mean
    // noise, not IR.
    Serial.print("IR EDGES: ");
    Serial.print(count);
    Serial.print("  level=");
    Serial.print(digitalRead(IR_RX_PIN));
    Serial.println("   -> if you pressed a remote, the RECEIVE PATH WORKS.");
  }

  delay(200);
}
