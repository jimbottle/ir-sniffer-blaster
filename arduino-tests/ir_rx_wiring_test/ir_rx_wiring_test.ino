// STAGE 1 — IR receiver WIRING test.  No libraries required.
//
// Board : Adafruit Feather ESP32 V2      Sensor: Adafruit 5990 IR transceiver
// Green (IR data OUT, receiver -> MCU) -> GPIO14
// Red   (V+)  -> USB pin (5V)     Black (GND) -> GND
//
// This does NOT decode IR. It answers one question: is the receiver wired and
// alive? It reports the idle line level, then counts signal edges when you press
// a remote.
//
// Serial Monitor @ 115200 baud.
//
// EXPECTED:
//   "Idle level: 1 (HIGH)"  -> receiver is powered and driving the line. Good.
//   Press a remote -> "IR DETECTED! edges=..."  -> data path works. Wiring is GOOD.
//
// TROUBLESHOOTING:
//   Idle level: 0 (LOW), never changes   -> green wire likely not on GPIO14, or the
//                                           5990 has no power (check red->5V, black->GND).
//   Idle flickers / edges with no remote -> floating pin: green wire not seated.
//   Idle HIGH but no edges on press      -> green wire wrong pin, or remote is dead /
//                                           not 38 kHz / not aimed at the sensor.

const int IR_RX_PIN = 14;   // green — IR data OUT (receiver -> MCU)
const int IR_TX_PIN = 27;   // white — IR data IN  (MCU -> emitter)

volatile unsigned long edgeCount = 0;

void IRAM_ATTR onIrEdge() {
  edgeCount++;
}

// Read the pin with a given input mode, after letting the line settle.
int probe(int mode) {
  pinMode(IR_RX_PIN, mode);
  delay(5);
  return digitalRead(IR_RX_PIN);
}

// Is anything actually DRIVING the pin, or is it floating (disconnected)?
//
// A floating pin obeys whichever internal resistor we enable: pull-down -> LOW,
// pull-up -> HIGH. The 5990's output is a buffered push-pull driver, so when it IS
// connected it overpowers the ESP32's weak (~45k) internal resistors and holds its
// level regardless. That difference is what tells "sensor connected" apart from
// "nothing on the wire" — which reading the pin once CANNOT do.
void reportConnection() {
  int withPulldown = probe(INPUT_PULLDOWN);
  int withPullup   = probe(INPUT_PULLUP);
  pinMode(IR_RX_PIN, INPUT);          // restore plain input for the real test

  Serial.print("Pin follows pulldown/pullup: ");
  Serial.print(withPulldown);
  Serial.print("/");
  Serial.println(withPullup);

  if (withPulldown == LOW && withPullup == HIGH) {
    Serial.println(">>> FLOATING — nothing is driving GPIO14.");
    Serial.println(">>> The 5990 is NOT CONNECTED (or has no power, or the green");
    Serial.println(">>> wire isn't seated). Any 'IR detected' below would be noise.");
  } else if (withPulldown == HIGH && withPullup == HIGH) {
    Serial.println(">>> DRIVEN HIGH — the 5990 is connected and idle. This is correct.");
    Serial.println(">>> Press a remote; you should see edges.");
  } else if (withPulldown == LOW && withPullup == LOW) {
    Serial.println(">>> DRIVEN LOW — something is holding GPIO14 low.");
    Serial.println(">>> Suspect a short to GND, or a miswired lead.");
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
  // look exactly like the "floating RX pin" fault this sketch is meant to detect.
  // Driving it LOW holds the emitter off for the duration of the test.
  pinMode(IR_TX_PIN, OUTPUT);
  digitalWrite(IR_TX_PIN, LOW);

  Serial.println();
  Serial.println("=== IR RX wiring test — GPIO14 (Adafruit 5990) ===");
  Serial.println("(GPIO27 held LOW: emitter off, so it cannot spoof the receiver.)");

  reportConnection();

  attachInterrupt(digitalPinToInterrupt(IR_RX_PIN), onIrEdge, CHANGE);

  Serial.println("Now point a remote at the 5990 and press buttons...");
  Serial.println();
}

void loop() {
  noInterrupts();
  unsigned long count = edgeCount;
  edgeCount = 0;
  interrupts();

  if (count > 0) {
    // Re-sample the level too: with no remote pressed the line should return to
    // idle HIGH between bursts. Edges that keep coming while the level sits at a
    // constant value (or with no remote in use) mean noise, not IR.
    Serial.print("IR DETECTED!  edges=");
    Serial.print(count);
    Serial.print("  level=");
    Serial.print(digitalRead(IR_RX_PIN));
    Serial.println("   -> receive path is working (if you pressed a remote).");
  }

  delay(200);
}
