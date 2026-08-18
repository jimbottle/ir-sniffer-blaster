# Project orientation (IR Sniffer / Blaster)

Context for anyone (human or AI) picking up this repo. For the full docs see
[`README.md`](README.md) and [`docs/`](docs/).

## What this is

A single-room IR sniffer/blaster: an **ESP32 Feather V2 (Adafruit 5900)** + **Adafruit
5990 IR transceiver**, running **ESPHome**. It captures IR remote codes and replays
them over the Home Assistant API, MQTT, REST, or a built-in web server.

## Current state

> **⚠️ REGRESSION, 2026-08-18 — READ FIRST.** The fan has not responded to the blaster
> since **2026-08-17 ~23:48 local**, and the emitter claims in this section are the ones
> in question. Under diagnosis as `ir-sniffer-blaster-54y`. Delivery is confirmed intact
> (42/42 presses transmitted and decoded device-side as Symphony `0xD81`); what is not
> excluded is a **degraded emitter drive path**. Do not prime on "emitter validated end
> to end" as current status.

**Built and working on real hardware.** Flashed to the ESP32-PICO-V3-02 Feather, on
2.4GHz WiFi via DHCP (holding `192.168.0.6` by lease reuse — **no router reservation
yet**, the ask is tracked in `home-assistant-pi-6sf`; see the "Known consequence"
section of `docs/naming.md`), reachable at `http://ir-blaster-livingroom.local/`. The **emitter** is validated end to end: the
`Fan Power` button, pressed over the network (REST/web server), fires a captured raw
code that toggles the fan — **but see the regression banner above: this has not held
since 2026-08-17 (`ir-sniffer-blaster-54y`).** **Receiver:** the RX hardware/wiring is proven via the
Arduino diagnostics, **and the ESPHome `remote_receiver` dump is confirmed too** —
PASS 2026-08-09 (R1): a 120-entry raw array plus a clean `Symphony data=0xD81,
nbits=12` decode, **which requires `inverted: true`** (see below). Also since
confirmed: **HA adoption** (the integration's own `core.config_entries` record) and
**OTA** (twice on 2026-08-09, again on 2026-08-18 to deliver the 54y probe
firmware). Still unexercised: only the `send_raw_ir` service. Compiling / flashing
/ testing the board is expected — it's a live build, not prep mode.

## The device is in the BEDROOM; the identifiers say living room

Moved 2026-08-10. Home Assistant was updated (area `bedroom`, display name "IR Blaster
(Bedroom)"). The ESPHome `name`, this repo's filename, the entity ids and the mDNS
hostname were **all deliberately kept as `livingroom`** — they have consumers outside
this repo, including the automation that the Bangle.js watch button fires.

**Do not rename them.** Full rationale and the consequence table: [`docs/naming.md`](docs/naming.md).

## Source of truth

- **`ir-blaster-livingroom.yaml`** is the authoritative firmware spec — the single
  source of truth for pins, protocols, and services.
- The brief's facts are distilled into `docs/` and `README.md`.
- **Diagnostics (not the shipped firmware):** `ir-wiring-test.yaml` (ESPHome,
  no-network receiver test) and `arduino-tests/` (Arduino IDE sketches: a library-free
  wiring test, an IRremote decode test, and an IRremote single-shot transmit test
  `ir_tx_test`). These **duplicate** pins/tuning from the authoritative config — the RX
  pin (all of them), the TX pin (`ir_tx_test`, `#define IR_SEND_PIN 27`), and a captured
  raw code (`ir_tx_test` carries a copy of one from `captured-codes/`). If you change the
  RX **or TX** pin or tuning in `ir-blaster-livingroom.yaml`, mirror it into all of them,
  or a diagnostic silently drives the wrong GPIO / tests a config the firmware no longer
  uses.
- **Intentional deviations from the original brief** (code-review driven, all
  reversible): the API block uses current `actions:`/`action:` syntax instead of
  the deprecated `services:`/`service:`; a fallback `ap:` + `captive_portal:` was
  added for WiFi recovery; WiFi uses **DHCP** (address held by lease reuse; a router
  reservation is still pending, `home-assistant-pi-6sf`) with the `manual_ip:` static
  block kept commented out as an alternative.
- **Real captured buttons, not placeholders.** The commented placeholder buttons
  were replaced by a live **`Fan Power`** `transmit_raw` button using a real captured
  code (`captured-codes/`), verified toggling the fan on hardware. Add more the same
  way (capture → convert to signed +mark/-space → add a button). Don't re-comment
  `Fan Power` — it's verified, not a placeholder.

## Non-negotiable facts (don't "fix" these)

- ESPHome board key is **`adafruit_feather_esp32_v2`** (variant esp32). Confirmed
  silicon: ESP32-PICO-V3-02, 8MB flash / 2MB PSRAM — the brief said "ESP32-WROOM",
  which is wrong, but the variant/board key are unaffected.
  Not S2/S3. (The brief's `featheresp32-v2` is outdated and fails validation on
  current ESPHome.) Arduino-cli FQBN equivalent: `esp32:esp32:adafruit_feather_esp32_v2`.
- **RX = GPIO14**, **TX = GPIO27**. These must match `docs/wiring.md`.
- **`remote_receiver.inverted: true`** is correct on this hardware, and it is what
  the shipped `ir-blaster-livingroom.yaml` carries. **Do not set it to `false`.**
  This entry previously asserted the opposite; that was asserted at bring-up and
  **disproved by measurement on 2026-08-09 (R1)** — `false` returned every timing
  sign-flipped, 0/24 matching the known-good array with no decode and no actuation,
  while `true` matched 24/24 and decoded on the first press
  (`docs/validation.md`, `docs/troubleshooting.md`). The 5990's IRin idles HIGH and
  pulls LOW during a carrier burst, so `false` records idle as mark. Reverting this
  breaks the receiver — which is the instrument every `ir-sniffer-blaster-54y`
  measurement depends on.
- The included PSU (4298) is **power-only**; the first flash needs a separate USB-C
  **data** cable. Subsequent flashes go **OTA — exercised and working**: twice on
  2026-08-09 (27.48 s, 11.17 s) and again on 2026-08-18 for the 54y probe firmware.
  Bootloader rollback is supported, so a failed OTA leaves the running image intact.
- The 5990 is **not I2C** — ignore the cable's "STEMMA" label; wire to GPIOs.
- **`secrets.yaml` is git-ignored and must never be committed.** Template:
  `secrets.yaml.example`; generate the API key with `openssl rand -base64 32`.

Bring-up ran against [`docs/validation.md`](docs/validation.md): the **transmit path is
proven** (captured code → `Fan Power` button → fan toggles, over the network) **as of
bring-up — but see the regression banner under "Current state": the fan has not responded
since 2026-08-17 (`ir-sniffer-blaster-54y`)**, while the
the **ESPHome receiver dump, HA adoption and OTA have all since been confirmed**
(R1, 2026-08-09; OTA again 2026-08-18), leaving **`send_raw_ir` as the only
genuinely unconfirmed item**.
Captured codes live in [`captured-codes/`](captured-codes/) — add new ones there as you
capture them.

---

# Agent Instructions

This project uses **bd** (beads) for issue tracking. Run `bd prime` for full workflow context.

> **Architecture in one line:** Issues live in a local Dolt database
> (`.beads/dolt/`); cross-machine sync uses `bd dolt push/pull` (a
> git-compatible protocol), stored under `refs/dolt/data` on your git
> remote — separate from `refs/heads/*` where your code lives.
> `.beads/issues.jsonl` is a passive export, not the wire protocol.
>
> See [SYNC_CONCEPTS.md](https://github.com/gastownhall/beads/blob/main/docs/SYNC_CONCEPTS.md)
> for the one-screen overview and anti-patterns (don't treat JSONL as the
> source of truth; don't `bd import` during normal operation; don't
> reach for third-party Dolt hosting before trying the default).

## Quick Reference

```bash
bd ready              # Find available work
bd show <id>          # View issue details
bd update <id> --claim  # Claim work atomically
bd close <id>         # Complete work
bd dolt push          # Push beads data to remote
```

## Non-Interactive Shell Commands

**ALWAYS use non-interactive flags** with file operations to avoid hanging on confirmation prompts.

Shell commands like `cp`, `mv`, and `rm` may be aliased to include `-i` (interactive) mode on some systems, causing the agent to hang indefinitely waiting for y/n input.

**Use these forms instead:**
```bash
# Force overwrite without prompting
cp -f source dest           # NOT: cp source dest
mv -f source dest           # NOT: mv source dest
rm -f file                  # NOT: rm file

# For recursive operations
rm -rf directory            # NOT: rm -r directory
cp -rf source dest          # NOT: cp -r source dest
```

**Other commands that may prompt:**
- `scp` - use `-o BatchMode=yes` for non-interactive
- `ssh` - use `-o BatchMode=yes` to fail instead of prompting
- `apt-get` - use `-y` flag
- `brew` - use `HOMEBREW_NO_AUTO_UPDATE=1` env var

<!-- BEGIN BEADS INTEGRATION v:1 profile:minimal hash:7510c1e2 -->
## Beads Issue Tracker

This project uses **bd (beads)** for issue tracking. Run `bd prime` to see full workflow context and commands.

### Quick Reference

```bash
bd ready              # Find available work
bd show <id>          # View issue details
bd update <id> --claim  # Claim work
bd close <id>         # Complete work
```

### Rules

- Use `bd` for ALL task tracking — do NOT use TodoWrite, TaskCreate, or markdown TODO lists
- Run `bd prime` for detailed command reference and session close protocol
- Use `bd remember` for persistent knowledge — do NOT use MEMORY.md files

**Architecture in one line:** issues live in a local Dolt DB; sync uses `refs/dolt/data` on your git remote; `.beads/issues.jsonl` is a passive export. See https://github.com/gastownhall/beads/blob/main/docs/SYNC_CONCEPTS.md for details and anti-patterns.

## Session Completion

**When ending a work session**, you MUST complete ALL steps below. Work is NOT complete until `git push` succeeds.

**MANDATORY WORKFLOW:**

1. **File issues for remaining work** - Create issues for anything that needs follow-up
2. **Run quality gates** (if code changed) - Tests, linters, builds
3. **Update issue status** - Close finished work, update in-progress items
4. **PUSH TO REMOTE** - This is MANDATORY:
   ```bash
   git pull --rebase
   git push
   git status  # MUST show "up to date with origin"
   ```
5. **Clean up** - Clear stashes, prune remote branches
6. **Verify** - All changes committed AND pushed
7. **Hand off** - Provide context for next session

**CRITICAL RULES:**
- Work is NOT complete until `git push` succeeds
- NEVER stop before pushing - that leaves work stranded locally
- NEVER say "ready to push when you are" - YOU must push
- If push fails, resolve and retry until it succeeds
<!-- END BEADS INTEGRATION -->
