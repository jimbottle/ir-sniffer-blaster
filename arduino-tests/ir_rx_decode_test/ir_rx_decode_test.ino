// STAGE 2 — IR receiver DECODE test.  Requires the "IRremote" library (v4.x)
// by Armin Joachimsmeyer:  Sketch > Include Library > Manage Libraries > "IRremote"
//
// Board : Adafruit Feather ESP32 V2      Sensor: Adafruit 5990 IR transceiver
// Green (IR data OUT) -> GPIO14      White (IR data IN) -> GPIO27
//
// Prints the decoded protocol/address/command AND the raw timing array for every
// button press. Serial Monitor @ 115200 baud.
//
// NOTE: this is a DIAGNOSTIC sketch. The shipped firmware for this project is
// ESPHome (ir-blaster-livingroom.yaml), not this sketch.
//
// ---------------------------------------------------------------------------
// USING THESE VALUES IN ESPHOME — the VALUES transfer, the FORMAT does not.
// IRremote prints C++ (`IrSender.sendNEC(0x20DF, 0x10EF, 0);`), not ESPHome YAML,
// so you must transcribe:
//
//   Decoded  ->  copy the address/command hex into a transmit_nec: block:
//                  - remote_transmitter.transmit_nec:
//                      address: 0x20DF
//                      command: 0x10EF
//
//   Raw      ->  the raw dump is multi-line, whitespace-padded, and begins with a
//                large negative "gap" value (e.g. -1044750). ESPHome's
//                transmit_raw.code / the send_raw_ir `raw` variable both want a
//                FLAT comma-separated signed int array. So: DROP the leading gap,
//                strip the newlines/padding, and join the remaining +mark/-space
//                microsecond values into one array.
//
// See docs/capture-replay.md.
// ---------------------------------------------------------------------------

// Must be defined BEFORE the include. IRremote's default is 200 entries, which
// silently truncates long frames (AC and soundbar remotes routinely exceed it) —
// exactly the remotes you reach for raw capture to handle. The ESP32 has the RAM.
#define RAW_BUFFER_LENGTH 750

#include <IRremote.hpp>

#define IR_RECEIVE_PIN 14
#define IR_EMITTER_PIN 27

void setup() {
  Serial.begin(115200);
  delay(800);

  // Hold the emitter off. GPIO27 gates the 5990's 940nm LEDs, which sit on the same
  // board as the receiver; left floating it can self-trigger and pollute the capture.
  pinMode(IR_EMITTER_PIN, OUTPUT);
  digitalWrite(IR_EMITTER_PIN, LOW);

  // DISABLE_LED_FEEDBACK: don't blink the onboard LED, avoids any pin surprises.
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);

  Serial.println();
  Serial.println("=== IR decode test — GPIO14 (Adafruit 5990) ===");
  Serial.print("Listening on GPIO");
  Serial.print(IR_RECEIVE_PIN);
  Serial.print(", raw buffer = ");
  Serial.println(RAW_BUFFER_LENGTH);
  Serial.println("Point a remote at the 5990 and press a button...");
  Serial.println();
}

void loop() {
  if (IrReceiver.decode()) {
    Serial.println("---------------------------------------------");

    // A truncated raw array is worse than none — it looks valid but won't replay.
    if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_WAS_OVERFLOW) {
      Serial.println("!! OVERFLOW — frame exceeded RAW_BUFFER_LENGTH. The raw array");
      Serial.println("!! below is TRUNCATED and will not replay correctly.");
      Serial.println("!! Raise RAW_BUFFER_LENGTH and re-capture.");
    }

    // Decoded protocol + address/command (e.g. NEC, address 0x20DF, command 0x10EF)
    IrReceiver.printIRResultShort(&Serial);

    // IRremote's own C++ send call. Handy reference, but NOT ESPHome syntax —
    // take the hex values from it, not the line itself (see header).
    IrReceiver.printIRSendUsage(&Serial);

    // Raw timings. Transcribe into ESPHome per the header: drop the leading gap,
    // flatten to a single comma-separated array.
    Serial.println("Raw timings (drop leading gap; flatten before pasting into ESPHome):");
    IrReceiver.printIRResultRawFormatted(&Serial, true);

    Serial.println();

    IrReceiver.resume();   // ready for the next press
  }
}
