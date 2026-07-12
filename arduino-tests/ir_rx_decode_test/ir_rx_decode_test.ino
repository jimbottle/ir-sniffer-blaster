// STAGE 2 — IR receiver DECODE test.  Requires the "IRremote" library (v4.x)
// by Armin Joachimsmeyer:  Sketch > Include Library > Manage Libraries > "IRremote"
//
// Board : Adafruit Feather ESP32 V2      Sensor: Adafruit 5990 IR transceiver
// Green (IR data OUT) -> GPIO14
//
// Prints the decoded protocol/address/command AND the raw timing array for every
// button press. Serial Monitor @ 115200 baud.
//
// The raw array it prints is directly reusable in the ESPHome build: paste it into
// a `remote_transmitter.transmit_raw` button, or pass it to the `send_raw_ir`
// service. See docs/capture-replay.md.
//
// NOTE: this is a DIAGNOSTIC sketch. The shipped firmware for this project is
// ESPHome (ir-blaster-livingroom.yaml), not this sketch.

#include <IRremote.hpp>

#define IR_RECEIVE_PIN 14

void setup() {
  Serial.begin(115200);
  delay(800);

  // DISABLE_LED_FEEDBACK: don't blink the onboard LED, avoids any pin surprises.
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);

  Serial.println();
  Serial.println("=== IR decode test — GPIO14 (Adafruit 5990) ===");
  Serial.print("Listening on GPIO");
  Serial.println(IR_RECEIVE_PIN);
  Serial.println("Point a remote at the 5990 and press a button...");
  Serial.println();
}

void loop() {
  if (IrReceiver.decode()) {
    Serial.println("---------------------------------------------");

    // Decoded protocol + address/command (e.g. NEC, address 0x20DF, command 0x10EF)
    IrReceiver.printIRResultShort(&Serial);

    // The ready-made send call for this code
    IrReceiver.printIRSendUsage(&Serial);

    // Raw timings — this is what you copy into ESPHome for anything that
    // doesn't decode to a known protocol.
    Serial.println("Raw timings:");
    IrReceiver.printIRResultRawFormatted(&Serial, true);

    Serial.println();

    IrReceiver.resume();   // ready for the next press
  }
}
