# Capture → replay workflow

The core loop: **sniff** a remote, **store** the code, **blast** it back.

## 1. Capture (sniff)

1. With `logger.level: DEBUG` and `remote_receiver.dump: all` active (both already
   set in the config), stream the device log:
   ```bash
   esphome logs ir-blaster-livingroom.yaml
   ```
   *(or the HA add-on log view.)*
2. Point a remote at the 5990 and press a button. The log prints either a **decoded
   line** (e.g. `Received NEC: address=0x20DF, command=0x10EF`) and/or a **raw
   timing array**.
3. Record what you captured in [`captured-codes/`](../captured-codes/) so it isn't
   lost.

## 2. Replay (blast)

### Decoded protocols (NEC / Sony / Samsung / …)

Copy the values into a `transmit_<protocol>` template button, re-flash (OTA or USB),
and trigger from HA or the web server:

```yaml
button:
  - platform: template
    name: "TV Power"
    on_press:
      - remote_transmitter.transmit_nec:
          address: 0x20DF
          command: 0x10EF
```

### Anything that doesn't decode cleanly

Use the raw array (which preserves exact timing). Two options:

**a) `transmit_raw` button** (requires a re-flash to add):

```yaml
button:
  - platform: template
    name: "TV Volume Up"
    on_press:
      - remote_transmitter.transmit_raw:
          carrier_frequency: 38kHz
          code: [9000, -4500, 560, -560, ...]
```

**b) `send_raw_ir` HA service** (fully programmatic, **no re-flash**):

- Service id: `esphome.ir_blaster_livingroom_send_raw_ir`
- Variable: `raw` = the int array (microseconds, `+mark` / `-space`)
- Carrier is fixed at 38 kHz in the service definition.

This is the generic path for replaying anything captured without rebuilding the
firmware.

## Notes on carrier frequency

Most consumer IR is **38 kHz**, which the 5990 receives and the config transmits.
For odd-carrier remotes, prefer the **raw capture** (it preserves timing) and set
`carrier_frequency` to match on transmit.
