# Validation checklist

Work these in order — each step depends on the previous one passing.

## Build & connectivity

- [ ] `esphome run` compiles with no errors.
- [ ] Device joins WiFi (2.4GHz); reachable at `http://ir-blaster-livingroom.local/`
      (DHCP + router reservation) and in Home Assistant.
- [ ] OTA update succeeds (a second `esphome run` over the network).

## Receive (sniff)

- [ ] Receiver: a remote press produces a **decoded and/or raw** line in the log.

## Transmit (blast)

- [ ] Transmitter: a captured code, fired from a button, actuates the real device.
- [ ] `send_raw_ir` service transmits a known raw array successfully.

## Acceptance criteria (from the project brief)

- [ ] Board boots, joins WiFi, and is reachable (HA + web server + OTA).
- [ ] Receiver: pressing any 38 kHz remote at the device prints decoded and raw IR to the log.
- [ ] Transmitter: a captured code can be replayed and the target device responds.
- [ ] A generic service exists to transmit arbitrary raw IR timing arrays programmatically.
