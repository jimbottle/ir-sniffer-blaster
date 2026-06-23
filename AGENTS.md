# Project orientation (IR Sniffer / Blaster)

Context for anyone (human or AI) picking up this repo. For the full docs see
[`README.md`](README.md) and [`docs/`](docs/).

## What this is

A single-room IR sniffer/blaster: an **ESP32 Feather V2 (Adafruit 5900)** + **Adafruit
5990 IR transceiver**, running **ESPHome**. It captures IR remote codes and replays
them over the Home Assistant API, MQTT, REST, or a built-in web server.

## Current state

**Scaffolding only — nothing has been flashed.** Hardware is purchased. The repo is
documented and ready for the build. The next real step is the first USB flash
([`docs/flashing.md`](docs/flashing.md)). Do **not** compile, flash, or run the board
unless explicitly asked — this repo is in documentation/prep mode.

## Source of truth

- **`ir-blaster-livingroom.yaml`** is the authoritative firmware spec — the single
  source of truth for pins, protocols, and services.
- The brief's facts are distilled into `docs/` and `README.md`.
- **Intentional deviations from the original brief** (code-review driven, all
  reversible): the API block uses current `actions:`/`action:` syntax instead of
  the deprecated `services:`/`service:`; a fallback `ap:` + `captive_portal:` was
  added for WiFi recovery; and the example `transmit_*` buttons ship **commented
  out** so the first flash exposes no pressable buttons firing placeholder codes.
  Uncomment the buttons after capturing real codes.

## Non-negotiable facts (don't "fix" these)

- ESPHome board key is **`adafruit_feather_esp32_v2`** (ESP32-WROOM, variant esp32).
  Not S2/S3. (The brief's `featheresp32-v2` is outdated and fails validation on
  current ESPHome.) Arduino-cli FQBN equivalent: `esp32:esp32:adafruit_feather_esp32_v2`.
- **RX = GPIO14**, **TX = GPIO27**. These must match `docs/wiring.md`.
- **`remote_receiver.inverted: false`** is correct for the 5990's buffered output.
  Setting `true` is only for a bare TSOP38238 and will break decoding.
- The included PSU (4298) is **power-only**; the first flash needs a separate USB-C
  **data** cable. Subsequent flashes are OTA.
- The 5990 is **not I2C** — ignore the cable's "STEMMA" label; wire to GPIOs.
- **`secrets.yaml` is git-ignored and must never be committed.** Template:
  `secrets.yaml.example`; generate the API key with `openssl rand -base64 32`.

When the build begins, drive it against [`docs/validation.md`](docs/validation.md) and
record captured codes in [`captured-codes/`](captured-codes/).

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
