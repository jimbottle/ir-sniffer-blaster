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

const int IR_RX_PIN = 14;

volatile unsigned long edgeCount = 0;

void IRAM_ATTR onIrEdge() {
  edgeCount++;
}

void setup() {
  Serial.begin(115200);
  delay(800);                      // let USB serial settle

  pinMode(IR_RX_PIN, INPUT);

  Serial.println();
  Serial.println("=== IR RX wiring test — GPIO14 (Adafruit 5990) ===");

  int idle = digitalRead(IR_RX_PIN);
  Serial.print("Idle level: ");
  Serial.print(idle);
  Serial.println(idle == HIGH ? " (HIGH)  <- expected. Receiver powered."
                              : " (LOW)   <- UNEXPECTED. Check power/GND and green->GPIO14.");

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
    Serial.print("IR DETECTED!  edges=");
    Serial.print(count);
    Serial.println("   -> receive path is working.");
  }

  delay(200);
}
