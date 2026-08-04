// TRANSMIT test — blast a captured raw IR code out the 5990 emitter.
// Requires the "IRremote" library (v4.x).
//
// Board : Adafruit Feather ESP32 V2      Sensor: Adafruit 5990 IR transceiver
// Green (IRout, emitter drive) -> GPIO27      (this is what we drive here)
// Red (VIN) -> 3V     Black (GND) -> GND
//
// This is a bring-up test: it sends a code captured with ir_rx_decode_test
// ONCE on boot, then idles. Point the 5990 at the target device and watch for a
// single response. To send again, tap the board's RESET button (or re-flash).
// (Phone-camera tip: many phone cameras see the 940nm LEDs flicker faint purple
// when it fires — a quick "is the emitter lighting up?" check.)
//
// The array below is "fan-power-switch" (hash 0xDA0AE878) captured this session,
// in IRremote raw format: mark, space, mark, space, ... in microseconds.

#include <IRremote.hpp>

#define IR_SEND_PIN 27      // green / IRout — drives the 5990's emitter FET

// Captured code (unknown protocol -> raw replay). 47 entries.
uint16_t code[] = {
  1280,420, 1230,420, 430,1270, 1230,420, 1230,420, 380,1270, 380,1270,
  430,1220, 430,1270, 380,1270, 380,1270, 1230,7120, 1230,420, 1230,420,
  430,1270, 1230,420, 1230,420, 380,1270, 380,1270, 430,1220, 430,1270,
  380,1270, 380,1270, 1230
};
const unsigned int codeLen = sizeof(code) / sizeof(code[0]);

void setup() {
  Serial.begin(115200);
  delay(500);
  IrSender.begin(IR_SEND_PIN);   // 38 kHz carrier on GPIO27
  Serial.println();
  Serial.println("=== IR TX test — single shot on GPIO27 (5990 emitter) ===");
  Serial.print("raw entries: ");
  Serial.println(codeLen);

  delay(300);                    // let the emitter settle after boot
  Serial.println(">>> sending ONCE (fan-power-switch)...");
  IrSender.sendRaw(code, codeLen, 38);   // send exactly once
  Serial.println(">>> done. Tap RESET to send again.");
}

void loop() {
  // idle — one shot only, so the fan toggles a single time
}
