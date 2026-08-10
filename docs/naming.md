# Naming: the blaster is in the bedroom, the identifiers still say living room

**This is deliberate. Do not "fix" it.**

The device was physically moved to the **bedroom** on **2026-08-10**. Home Assistant was
updated the same evening. The identifiers were not, and were not meant to be.

## What changed and what did not

| what | value | rename? |
|---|---|---|
| Physical location | bedroom | — |
| HA device area | `bedroom` | already done |
| HA display name (`name_by_user`) | `IR Blaster (Bedroom)` | already done |
| ESPHome `friendly_name` | `IR Blaster (Living Room)` | **NO** — `name_by_user` overrides it everywhere in the UI |
| ESPHome `name` / `device_name` | `ir-blaster-livingroom` | **NO** — firmware-baked |
| This repo's config filename | `ir-blaster-livingroom.yaml` | **NO** |
| HA entity ids | `button.ir_blaster_living_room_fan_power` | **NO** |
| Generated HA service id | `esphome.ir_blaster_livingroom_send_raw_ir` | **NO** — derived from `name` |
| mDNS hostname | `ir-blaster-livingroom.local` | **NO** — derived from `name` |

## Why renaming is not a cosmetic change

Each of these has a consumer outside this repo:

- **The entity id is fired by an automation in `jimbottle/home-assistant-pi`**
  (`automations/bangle_irblast.yaml`), which is what the Bangle.js watch button triggers.
  Renaming the entity silently breaks the watch. A comment at that automation's
  `entity_id` records the same decision from the other side
- **Changing `name:` forces re-adoption in Home Assistant** and changes the generated
  service id, which `docs/bangle-integration.md` builds on
- **`name:` also determines the mDNS hostname.** mDNS resolution is broken *from the
  HA Pi specifically* (`home-assistant-pi-6sf`; elsewhere on the LAN it works —
  measured resolving to `192.168.0.6` on 2026-08-09, `docs/hardware-gate-runbook.md`),
  so the ESPHome integration pins a bare IP — `host: 192.168.0.6`. A rename would not
  fix that and would add churn to it

**Location belongs in Home Assistant's registry, not in firmware.** That is where it was
changed, and that is the right place for it.

## If the location changes again

Update the HA device registry — area and `name_by_user`. Nothing in this repo needs to
change, including this table's left-hand column. Add a dated line below instead.

- **2026-08-10** — moved living room → bedroom. HA registry updated; all identifiers kept.
  Recorded in `home-assistant-pi` `CHANGELOG.md` and `CLAUDE.md`, and in
  `home-assistant-pi-6sf`

## Known consequence of the move

Pings to the blaster from the LAN run **~90–116 ms**, against under ~10 ms for a healthy
ESPHome node — the bedroom position looks marginal for Wi-Fi. **If the device flaps,
reposition it before debugging protocol.** It also dropped off the network entirely during
the move and re-took `192.168.0.6` by lease reuse rather than by reservation, so the
reservation ask in `home-assistant-pi-6sf` still stands.
