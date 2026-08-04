# Validation checklist

Status as of hardware bring-up. Ticked items were confirmed on the real board;
unticked items genuinely haven't been exercised yet.

## Build & connectivity

- [x] `esphome run` compiles with no errors.
- [x] Device joins WiFi (2.4GHz); reachable at `http://ir-blaster-livingroom.local/`
      (DHCP + router reservation) and in Home Assistant.
- [ ] OTA update succeeds (a second `esphome run` over the network). *(Not yet done —
      both flashes were over USB; OTA is configured but unexercised.)*

## Receive (sniff)

- [x] Receiver: a remote press produces a **decoded and/or raw** line in the log.
      (Validated with the Arduino diagnostics: 22 edge bursts + 28 clean captures.)

## Transmit (blast)

- [x] Transmitter: a captured code, fired from a button, actuates the real device.
      (The `Fan Power` button toggled the fan, over the network via REST.)
- [ ] `send_raw_ir` service transmits a known raw array successfully. *(Not yet fired —
      transmit was proven via the `transmit_raw` button, not the HA service.)*

## Acceptance criteria (from the project brief)

- [x] Board boots, joins WiFi, and is reachable (HA + web server + OTA-capable).
- [x] Receiver: pressing any 38 kHz remote at the device prints decoded and raw IR to the log.
- [x] Transmitter: a captured code can be replayed and the target device responds.
- [x] A generic service exists to transmit arbitrary raw IR timing arrays programmatically.
      (`send_raw_ir` is defined and the config validates; not yet fired end-to-end — see above.)
