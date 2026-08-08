# Validation checklist

Status as of hardware bring-up. Ticked items were confirmed on the real board;
unticked items genuinely haven't been exercised yet.

## Build & connectivity

- [x] `esphome run` compiles with no errors.
- [x] Device joins WiFi (2.4GHz); web server reachable at
      `http://ir-blaster-livingroom.local/` (DHCP + router reservation).
- [x] Adopted in Home Assistant. *(Corrected 2026-08-07 — this was stale, not pending.
      Evidence is the **existence** of `button.ir_blaster_living_room_fan_power`: HA only
      creates that entity from a list-entities response over the ESPHome integration, and
      `home-assistant-pi/automations/bangle_irblast.yaml` presses it by `entity_id`.)*
- [x] **HA can DELIVER an action to the device over the encrypted API.** *(R15 — CLOSED
      2026-08-07 by owner observation: pressing `button.ir_blaster_living_room_fan_power`
      from HA toggled the fan, and it has been in that state for some time. The owner also
      confirmed nothing had been changed or flashed beforehand, so this is the standing
      production firmware.
      **This was an evidence gap, not a fault.** Adoption proves HA enumerated the device;
      it does not prove a command arrives, and **no HA-side artefact can settle it** —
      ESPHome buttons are stateless, so `button.*` state is HA's own locally-generated
      `last_pressed`, and `automation.last_triggered` only says the automation started.
      Three revisions of `docs/bangle-integration.md` cited one of those and were wrong.
      What closed it is the one thing none of them had: a human watching the fan.)*
- [ ] OTA update succeeds (a second `esphome run` over the network). *(Not yet done —
      both flashes were over USB; OTA is configured but unexercised.)*

## Receive (sniff)

- [ ] Receiver: a remote press produces a **decoded and/or raw** line in the ESPHome
      log. *(Not yet observed. The RX **hardware/wiring** is proven via the Arduino
      diagnostics — 22 edge bursts + 28 clean captures — but the ESPHome
      `remote_receiver` path (`inverted: false`, `tolerance`, `idle`, `buffer_size`) is
      a different code path; run `esphome logs` and press a remote to confirm.)*

## Transmit (blast)

- [x] Transmitter: a captured code, fired from a button, actuates the real device.
      (The `Fan Power` button toggled the fan, over the network via REST.)
      **⚠️ WHICH REST is unrecorded, and it matters (R12).** This device exposes two:
      ESPHome's own `web_server` on port 80, and Home Assistant's API. If it was the
      former — the likelier reading, since `web_server` is configured and needs no
      credentials — then this line proves the emitter works but says **nothing** about
      the HA path, and it is not evidence for R15. Whoever ran it should say which, or
      it should be re-run deliberately through HA.
- [ ] `send_raw_ir` service transmits a known raw array successfully. *(Not yet fired —
      transmit was proven via the `transmit_raw` button, not the HA service.)*

## Acceptance criteria (from the project brief)

- [ ] Board boots, joins WiFi, and is reachable (HA + web server + OTA). *(Boots, joins
      WiFi, web server reachable — confirmed. HA adoption and OTA not yet exercised.)*
- [ ] Receiver: pressing any 38 kHz remote at the device prints decoded and raw IR to
      the ESPHome log. *(RX hardware proven via Arduino diagnostics; the ESPHome
      `remote_receiver` dump not yet observed — see Receive above.)*
- [x] Transmitter: a captured code can be replayed and the target device responds.
- [x] A generic service exists to transmit arbitrary raw IR timing arrays programmatically.
      (`send_raw_ir` is defined and the config validates; not yet fired end-to-end — see above.)
