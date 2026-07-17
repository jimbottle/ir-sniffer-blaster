# Project Instructions for AI Agents

This file provides instructions and context for AI coding agents working on this project.

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


## Build & Test

```bash
# --- Toolchain (once) ---
uv venv && uv pip install -r requirements.txt   # esphome + pyserial, pinned

# --- ESPHome firmware (the IR blaster) ---
.venv/bin/esphome config ir-blaster-livingroom.yaml   # validate config (needs secrets.yaml)
.venv/bin/esphome config ir-wiring-test.yaml          # no-network diagnostic config

# --- Arduino sketches (compile-check against the real board definition) ---
CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
FQBN="esp32:esp32:adafruit_feather_esp32_v2:UploadSpeed=115200"
"$CLI" compile --fqbn "$FQBN" learn/continuity_tester
"$CLI" compile --fqbn "$FQBN" arduino-tests/ir_rx_wiring_test
"$CLI" compile --fqbn "$FQBN" arduino-tests/ir_rx_decode_test

# --- Unit tests (no board, no deps, stock python3) ---
python3 learn/test_verify.py    # verify.py's serial-line parser
```

**Run `python3 learn/test_verify.py` whenever you change what a sketch prints.**
`verify.py`'s parser greps for specific output strings and has been silently broken
by format changes several times; the test is the guard.

## Architecture Overview

Two independent things share this repo and the same ESP32 Feather V2 board:

1. **The IR sniffer/blaster** (the real project) — an ESPHome firmware
   (`ir-blaster-livingroom.yaml`) that captures and replays IR. `ir-wiring-test.yaml`
   is a network-free receiver diagnostic; `arduino-tests/` holds Arduino IDE
   receiver-test sketches. See `README.md` and `docs/`.
2. **`learn/`** — a standalone Arduino teaching project (a pull-up continuity tester)
   plus host-side serial tools (`monitor.py`, `verify.py`) and their tests. Unrelated
   to the firmware; it just borrows the board.

## Conventions & Patterns

- **Board:** ESPHome key `adafruit_feather_esp32_v2`; Arduino FQBN
  `esp32:esp32:adafruit_feather_esp32_v2`. **Upload at 115200** — the CH34x USB bridge
  fails at the default 921600.
- **Serial is 115200**, and the ESP32 resets when the port opens (DTR/RTS → EN/GPIO0).
- **Secrets:** `secrets.yaml` is git-ignored; only `secrets.yaml.example` is committed.
- Host-side Python targets **stock macOS `python3` (3.9)** — use
  `from __future__ import annotations`, not bare PEP 604 unions in runtime positions.

<!-- BEGIN WYK CONVENTIONS v:1 -->
## wyk — planning & handoff over bd

This repo uses **wyk**, a view + handoff layer over **bd (beads)**. "Plan
it in wyk" = **file the plan as bd issues** (deps via `bd dep add`), not
markdown/TodoWrite. File with **`wyk create`** (same flags as `bd create`,
forwarded verbatim) — it also stamps the Claude session so the TUI's
Session column traces work back to a conversation. A PreToolUse hook
blocks raw `bd create` and tells you to switch; that's expected — just
re-run as `wyk create`.

**Owner column** — whose move it is, label-driven (NOT bd's owner/assignee):
- `human` → **HUMAN** (a human must act).
- `agent-handoff` → **AGENT-HANDOFF**: another agent owns it; don't touch,
  a human coordinates. Excluded from `wyk inbox`.
- agent task blocked by a `human`-flagged dep → **HUMAN-BLOCK** (skip it).
- else → **AGENT** (the default; a null owner is never blank — so a task
  that needs a human MUST be handed off, or the human never sees it).

**Hand off to a human**: `wyk handoff <id>` (or `wyk handoff -create "<title>"`)
sets `human` + writes the runbook. Never hand-roll labels; `-a`/`--claim`
are bd's status, not the badge.

**Pick up work**: `wyk inbox` FIRST (items bounced back to you — WORK them),
then `wyk` / `bd ready`. `wyk conventions` prints the full contract.

**Something wrong? Act — don't shrug.** If a wyk/bd command errors, a
convention looks broken, or the workflow rubs wrong, file a bd issue (with
an owner) and fix or hand it off — don't route around it silently.
Friction with wyk is product data; surfacing it is the job.
<!-- END WYK CONVENTIONS -->
