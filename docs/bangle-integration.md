# Bangle.js integration — project plan

Goal: **review captured IR signals and fire any of them from the Bangle.js 2 watch.**

Today the watch has a single full-screen button that fires one hard-coded webhook
(`my-bangle` repo, `apps/irblast`). This plan covers turning that into a browsable
library, and covers what would be needed for the watch to participate in capture.

Two repos and two agents are involved. The boundary is stated at the end, and the
task split is under [Before we begin](#before-we-begin).

> **Status: decisions taken 2026-08-07, build starting.** The measurement session has
> not been run, so sizing decisions remain open. Everything else below is settled.

## Decisions taken — 2026-08-07

Owner decisions, recorded so neither agent re-litigates them. Each supersedes the
open question of the same name.

| # | decision | consequence |
|---|---|---|
| **H2** | **Replay-only v1.** Capture is not armed from the wrist | The firmware `on_raw` path and the HA write path are **still in scope** — arming moves to an HA dashboard toggle. Without them the library cannot grow |
| **H1** | **Everything remote.** The project must work wherever the board is online and HA is reachable | Supersedes "split the ids" — see the revised exposure model below. No second webhook |
| **H3** | **Free-form names with enforced constraints** | Naming happens in HA with a real keyboard, since H2 means the watch never names anything. `short` ≤14 enforced on the write path; `device` is a grouping string, not a controlled enum. **H3 as a vocabulary question dissolves** |
| **H4** | **Build the transmit counter** | ESP32-side, proves IR left the LED. Two round trips per fire, pending R7 |
| **H6** | **Hardware first, then measure** | R1/R15/R13 before the watch session. If R1 fails the premise is dead and the measurement session would have been wasted |
| — | **Board access: on WiFi, USB available** | OTA is attempted first; a USB-C **data** cable is the fallback. The bundled PSU is power-only and cannot flash |

### Revised exposure model (supersedes "Split the ids")

"Everything remote" rules out the LAN-only read plan. It also rules out plain
`/local/` paths: those URLs are guessable, so an Access bypass on them would publish
the code library — an inventory of the room's devices — to the internet with **no
credential at all**.

```
READS   /local/ir-<32 hex>/codes.json     Access bypass on the SECRET PREFIX
        /local/ir-<32 hex>/status.json    (one destination covers both)

FIRE    /api/webhook/<blast-id>?code=x    existing webhook, unchanged

WRITE   HA dashboard  ->  Access LOGIN    authenticated, not a bearer URL
ARM     HA dashboard  ->  Access LOGIN
```

- **Reads use a capability URL** — the same trust model as the existing webhook id, a
  128-bit secret in the path that is never published. The secret is a *directory*, so
  one Access destination covers both files.
- **Writes go through the authenticated dashboard**, not a bearer URL. This is
  *stronger* than the superseded second-webhook plan: the one class of endpoint that
  mutates state gets a real login instead of a shared secret.
- **Two Access destinations total** — the existing webhook, plus the secret prefix.
- The secret directory is subject to the same rules as the webhook id: generated on
  the Pi, never in git, never in a session. Rotation is a two-place change (the
  filesystem path and the Access destination), exactly as
  `secrets.example.yaml` documents for the webhook.

## Architecture

```
Bangle.js  ──BLE──▶  Gadgetbridge (+ Internet Helper)  ──HTTP──▶  Home Assistant
                                                                       │
                                                            ESPHome API (encrypted)
                                                                       ▼
                                                          ESP32 / ir-blaster-livingroom
```

**The watch never talks to the ESP32.** `send_raw_ir` is an ESPHome API action behind
`api.encryption.key` — the watch cannot call it and should not try. Home Assistant owns
the code library and is the only thing that talks to the board.

Three HA endpoints are needed:

| endpoint | direction | purpose |
|---|---|---|
| list | HA → watch | returns the code library as JSON so the watch can render a menu |
| fire | watch → HA | takes a code identifier, calls `send_raw_ir` with the stored array |
| status | HA → watch | a transmit counter the watch can poll to confirm IR actually left the board |

The status endpoint is new to this revision and exists to answer H4. See
[Proposed library format](#proposed-library-format-and-endpoints).

## What is proven, and what is assumed

This section exists because the plan below is only as good as this table. Mirrors the
discipline of [`validation.md`](validation.md).

| claim | status |
|---|---|
| ESPHome exposes a **raw receive trigger** | **VERIFIED** by reading this repo's own vendored ESPHome: `remote_base/__init__.py:1220` has `@register_trigger("raw", RawTrigger, cg.std_vector.template(cg.int32))`. Payload is `std::vector<int32_t>` — the same shape `send_raw_ir` consumes |
| ESPHome can push **arbitrary-length data to HA's event bus** | **VERIFIED** by source read: `api/__init__.py:646` registers the `homeassistant.event` action. Event name must be `esphome.*` (`validate_homeassistant_event`, line 624). This is the route around the 255-char state cap |
| That trigger **fires on this hardware** | **PROVEN 2026-08-09 (R1).** 120-entry array delivered on the first press. Requires `inverted: true` — see below |
| A capture can round-trip `on_raw` → `transmit_raw` → device responds | **PROVEN 2026-08-09 (R1). THE PREMISE HOLDS.** On-device, no HA: the fan toggled ~3 s after capture with no second press. The board also heard its own replay and decoded it identically, so the retransmission is faithful. **⚠️ REGRESSED 2026-08-17 ~23:48 local — the fan has not responded to the blaster since, and is under diagnosis as `ir-sniffer-blaster-54y`.** The pass above remains true for its date; it is NOT current status. |
| RX polarity | **`inverted: true` — the opposite of what this project documented.** Run 1 (`false`) returned every sign flipped: 0/24 matching the known-good array, no decode, no actuation. Run 2 (`true`): 24/24 matching within 19.2 µs mean, decoded on first press, fan actuated. The old claim was asserted at bring-up and never tested against a capture |
| The fan remote's protocol | **Symphony `data=0xD81`, `nbits=12` — NOT "unknown".** The "decodes as UNKNOWN so we must use raw" premise was a polarity artefact. See the schema consequence below |
| The board is **adopted in Home Assistant** | **PROVEN.** Evidence is the ESPHome integration's own **config entry** — `core.config_entries` pins "IR Blaster (Living Room)" to `host: 192.168.0.6` (`home-assistant-pi/CHANGELOG.md:1446`, `:1468`). Entity existence is corroborating only: HA also creates `button.*` entities from template helpers and MQTT discovery, so an earlier "HA *only* creates that entity via ESPHome" was false |
| **HA → ESP32 over the encrypted API** delivers an action | **PROVEN 2026-08-07 (R15 closed).** The owner pressed `button.ir_blaster_living_room_fan_power` in HA and **watched the fan toggle**, on the standing production firmware with nothing flashed or changed beforehand. That is the physical observation the standing rule below demands — and note it is the *only* citation of the four attempted that qualifies. **⚠️ 2026-08-18: the fan actuation cited here NO LONGER REPRODUCES** (regressed 2026-08-17 ~23:48, `ir-sniffer-blaster-54y`). **This hop itself still holds** and is now on stronger evidence than R15 had: a `button.press` was confirmed device-side, the board logging its own 38 kHz transmission decoding as `Symphony 0xD81` on 42/42 presses. What broke is downstream of this row, past the emitter |
| 🚨 **STANDING RULE: no HA-side artefact proves this hop.** | *Kept after R15 closed, because the rule is what got it closed correctly.* **Three consecutive revisions asserted this hop and were wrong, each on a different artefact; the fourth attempt was a human watching the fan, and that is the one that held.** The hop was working the whole time — this was an evidence gap, never a fault — but "it probably works" is not what this table records. Do not assert this class of claim without device-side or physical evidence. What was tried and does **not** work: (1) `automation.…last_triggered` — proves the automation started; (2) `button.….state` — **ESPHome buttons are stateless.** No `ButtonStateResponse` exists anywhere in the vendored protocol (zero hits across `site-packages`) and `ButtonCommandRequest` is fire-and-forget with no reply (`aioesphomeapi/client.py:1661`), so the state is a **locally-generated `last_pressed`** written by HA's own `ButtonEntity`; (3) `my-bangle/apps/irblast/NOTES.md:43` *"A tap on the watch fired the fan"* — **not an independent observation.** The same sentence says *"Measured from Home Assistant rather than taken on report"*, and that note's own chain table ends at `webhook → automation → button`, with no `button → ESP32 → fan` leg. It is a summary of artefacts (1) and (2) |
| `homeassistant.event` reaches HA **from** the device | **UNVERIFIED**, but low risk. Grounded on **device-originated frames**: the device must emit `HelloResponse`, `ConnectResponse`, `ListEntitiesDoneResponse` and ongoing `PingResponse` keepalives for R15's press to have been routed at all. Those are device→HA application traffic on the connection an event would use. **R15 itself evidences only the HA→device leg** — `ButtonCommandRequest` is fire-and-forget and no `ButtonStateResponse` exists, so the press produced zero device-originated traffic and is the wrong direction for this row. *(Two earlier revisions got this wrong in opposite ways: one called enumeration "device→HA" when it is a round trip; the next deleted enumeration entirely and grounded on R15, which is one-directional. Enumeration was valid evidence all along — it was only ever awkward because it also proved HA→device while the row above still said UNVERIFIED. That row now reads PROVEN, so the awkwardness is gone.)* |
| **OTA** works on this board | **PROVEN 2026-08-09 (twice, 27.48 s / 11.17 s, both `OTA successful`), and again 2026-08-18** delivering the `ir-sniffer-blaster-54y` probe firmware — three network flashes, zero failures (`docs/validation.md`). The boot banner reports `Bootloader rollback: supported`, so a failed OTA writes the inactive slot and leaves the running image intact. An earlier revision of this row read UNVERIFIED and told the reader to budget for USB-only; that was true before the 2026-08-09 gate and is not now. Consistent with `:404` and `:501` in this file |
| `send_raw_ir` transmits | **UNVERIFIED.** `validation.md` — transmit was proven via the `transmit_raw` button, not the HA action |
| Watch can reach HA over **plain HTTP on the LAN** | **PROVEN** 2026-08-04 — a watch tap moved HA's `automation.bangle_ir_blaster_fan_power` `last_triggered`. **This is the whole of what that measurement proves.** Neither timestamp in it says anything about delivery to the board; see the standing rule above |
| `Bangle.http` resolves with the **response body** | **SOURCE READ ONLY.** `apps/android/lib.js` resolves `{t:"http", resp:"<body>", id}`. **No non-empty body has ever been observed on this setup** — every request so far hit a webhook that answers 200 empty |
| `Bangle.http` exposes an **HTTP status code** | **NO — it does not exist in the contract.** A 404, a captive-portal splash and a success are indistinguishable |
| Query strings survive the relay | **UNVERIFIED.** Everything today is path-only. See R4 — this one has no clean workaround |
| HTTPS / mDNS reachable from the watch | **UNVERIFIED** |
| `textinput` available on the watch | **NO — not installed.** Device app list read 2026-08-06: 23 apps, no `textinput`. It exists in the App Loader and could be added |
| Watch storage headroom | **MEASURED** 2026-08-06: 7,071,108 bytes free. Not a constraint |
| Watch RAM headroom | **UNMEASURED.** This is the real constraint on Bangle.js 2, not storage |
| The webhook id is **burned and unrotated** | **NO — CORRECTED.** It was rotated 2026-08-05. See the constraint below |

## Constraints that shape the design

**The watch cannot confirm anything fired.** No status code exists on this transport.
The existing app says `RELAYED`, not `SENT`, for exactly this reason — an earlier build
printed `SENT` for requests Home Assistant never received. A multi-code UI makes this
worse: with one button you at least know *which* thing did not happen. This plan now
proposes a pollable transmit counter to close the gap — see H4 and the status endpoint.

**HA caps entity state at 255 characters.** The one captured code is 47 entries, ~250
characters serialised — already at the edge. Raw arrays must live in event payloads or
attributes, never in an entity state.

**Raw arrays get much longer than 47.** Simple remotes run 30–70 entries. **HVAC and AC
remotes transmit full state — mode, temperature, fan, timer — in one frame and commonly
run 100–450 entries.** If a mini-split ever enters this library, the fan's 47 stops
being representative by an order of magnitude. Two consequences: the 255-char problem
gets worse, and —

**`buffer_size: 2kb` in `ir-blaster-livingroom.yaml` is a capture ceiling.** Fine for
47 entries, likely too small for a long HVAC frame. Raise it before anyone tries to
capture one, or the capture will silently truncate.

**`on_raw` will fire on every IR burst in the room**, including noise and unrelated
remotes. Capture needs gating — see the four-part gate under the proposed format.

**The receiver hears the transmitter.** RX and TX are on the same board. Armed capture
plus a transmit means the device captures its own output. Capture must be suppressed
during transmit.

**Rolling codes cannot be replayed, and cannot be detected from a single capture.**
[`captured-codes/README.md`](../captured-codes/README.md) flags the replay problem. The
subtler point: classification takes N captures of the same button and a diff — the fan
was only declared stable because **28 captures came back identical**. So the library
cannot mark a code fireable at capture time. Hence the three-state `status` field.

**The webhook id was rotated on 2026-08-05 — it is not burned.** *(Corrected this
revision; the previous claim came from `my-bangle/apps/irblast/NOTES.md`, which predates
the rotation.)* Evidence in `home-assistant-pi`: `CHANGELOG.md:270` records the rotation
and `home-assistant-pi-zs4` as closed; `automations/bangle_irblast.yaml` names the
previous id (`...183fda0f...`) as dead and states the new one was generated on the Pi
with `openssl rand -hex 16` and never entered a session; `CHANGELOG.md:153` records the
Cloudflare Access application scoped to the **rotated** path. This is a repo read, not a
read of the Pi's `secrets.yaml` — confirm before relying on it, but if it holds, H1 is
already resolved and only the widening decision remains.

**Widening is still a real decision, and rotation does not settle it.** Going from
"fires one fan" to "fires anything in the library" changes what a leaked id can do, and
the fire path is unauthenticated by design so that it works from outside the LAN. The
proposal is to split the ids — see the endpoints section.

**The HA↔ESP32 link has been observed dropping, and every fire in this design rides it.**
`home-assistant-pi/CHANGELOG.md:1427-1441` records **five** consecutive `Referenced
entities button.ir_blaster_living_room_fan_power are missing or not currently available`
failures at 21:38:44–21:39:48Z on 2026-08-04 — minutes *after* the successful press —
plus a `ResolveTimeoutAPIError` and an `EncryptionHelloAPIError`. The device dropped
**after** the press, and that entry explicitly declines to call it settled. The board is
running with **no DHCP reservation** (`:1446`, `:1468`), on a bare-IP pin whose value
has since changed — `192.168.0.6` through 2026-08-09, and `host: 192.168.0.3` as read
from `core.config_entries` on 2026-09-01, matching where the device actually was.

Two consequences for this plan. **A fire can fail at HA's entity-availability check and
never reach the API at all** — a third outcome distinct from "the device received it and
did nothing", and one an observer watching the fan cannot distinguish. And it strengthens
the case for the H4 transmit counter: a counter that does *not* move is the only thing
that separates "never delivered" from "delivered and the IR did nothing".

**Every watch action depends on two Android processes.** Gadgetbridge *and* the
Internet Helper add-on. The helper is not currently exempt from battery optimization
(`my-bangle-o7i`), and a killed helper breaks `Bangle.http` while Gadgetbridge still
looks connected.

**Deploying to the watch takes the BLE slot the phone uses.** The switching loop is
solved and documented (`my-bangle` repo, `docs/troubleshooting.md`) — phone Bluetooth
off, Make Connectable, connect, answer Yes, phone Bluetooth on. No unpairing. But
Chrome's device chooser needs a human click, so a human must open each window. **Favour
fewer, larger deploys over many small ones.**

## Proposed library format and endpoints

**PROPOSED, not agreed.** This is the seam. It is written down as a strawman so the two
sides have something concrete to disagree with, rather than diverging in parallel.
Nothing here is built or tested.

### Storage

A JSON file at `/config/ir_codes.json` on the Pi, repo-mirrored via the existing
`tools/sync-from-pi.sh`.

- **Read at fire time:** a `command_line` sensor (`cat /config/ir_codes.json`) with the
  table in `json_attributes` and a short `value_template` (the generation counter),
  because entity state caps at 255 chars. Needs a `recorder:` exclude or it bloats the DB.
- **Write:** `shell_command` → a script that validates and rewrites the file, then
  `homeassistant.update_entity`. `python_script` cannot do this — it is sandboxed with
  no file I/O.

**The design rule behind this: read at runtime, never at config-load.** A `!include`d
YAML library was considered and rejected — `automations/bangle_irblast.yaml` documents
that a bad `!include` fails `ha core check` and lands Core in **safe mode**. A malformed
runtime JSON read breaks one automation; a malformed config-load read breaks the
instance.

### Schema — two files, one private and one projected

**These are different shapes and conflating them is the likeliest way the two sides
diverge.** The private record holds the raw timing arrays; the public one never does.

> **⚠️ R1 changed this section's premise and it is not yet fully reworked.** The fan
> remote decodes as **Symphony `data=0xD81, nbits=12`**; "everything is an unknown raw
> code" was an artefact of inverted RX polarity. ESPHome has a matching
> `transmit_symphony` action (`remote_base/__init__.py:1173`) taking `data`, `nbits` and
> `command_repeats`.
>
> **A decoded record is ~3 small fields against a 120-entry array — roughly 50× smaller.**
> That lands directly on the watch's response-size limit (R2), which was the tightest
> constraint on the whole read path. The library should therefore be **protocol-first,
> raw-fallback**: store the decode when there is one, raw only when there is not. The
> schema below is still raw-only and needs a `protocol` variant adding once R11 sampling
> shows how many other remotes decode.

**Private — `/config/ir_codes.json`.** Read by the `command_line` sensor at fire time.
This is the only place a raw array exists.

```json
{
  "generation": 7,
  "codes": [{
    "slug": "fan.power",
    "short": "Fan Power",
    "name": "Ceiling fan power toggle",
    "device": "fan",
    "status": "confirmed",
    "captured_at": "2026-08-04",
    "raw": [1280, -420, 1230, -420, 430, -1270, 1230],
    "prior_raw": [1280, -420, 1230, -420, 430, -1270, 1230]
  }]
}
```

**Public — `/local/ir-codes.json`.** A projection of the above, regenerated on every
write. Identical minus `raw`/`prior_raw`, plus `entries` as a size hint.

```json
{
  "generation": 7,
  "codes": [{
    "slug": "fan.power",
    "short": "Fan Power",
    "name": "Ceiling fan power toggle",
    "device": "fan",
    "status": "confirmed",
    "entries": 47,
    "captured_at": "2026-08-04"
  }]
}
```

**Raw arrays are deliberately never published to `/local/`.** Three reasons: `/local/`
is unauthenticated to anyone who can reach HA; the arrays are the bulk of the payload
and R2 may find a tight body-size ceiling; and the watch has no use for them — it fires
by slug and HA does the lookup.

`prior_raw` is what makes rolling-code detection possible. The write path compares a new
capture for an existing slug against it: identical → promote toward `confirmed`,
different → mark `rolling`. Without it there is nothing to diff and the `rolling` status
could never be reached.

- `slug` — `<device>.<function>`, lowercase `[a-z0-9_]` per segment, from a controlled
  vocabulary shipped in the same payload (see H3). **The join key between both files.**
- `entries` — public only, `len(raw)`. A size hint for the UI, not authoritative.
- `short` — **required, ≤14 chars, enforced on the write path** so the watch never has
  to truncate. 14 is the Bangle agent's measured comfortable width at 176 px.
- `status` — `unverified` | `confirmed` | `rolling`.
  - `unverified` is **the default on every new capture** — see the rolling-code
    constraint. Fireable, but the UI should mark it.
  - `confirmed` — re-fired and the target responded.
  - `rolling` — two captures of the same slug differed. **Render unfireable.**
- `generation` — bumped on every write, so the watch can detect a stale cache without
  diffing the table. Codes appear and disappear; the watch must handle both.

### Endpoints

*Reflects the H1 decision: everything works remote. Supersedes the earlier LAN-only
read plan.*

| endpoint | shape | exposure | notes |
|---|---|---|---|
| list | `GET /local/ir-<secret>/codes.json` | **LAN + away** | Access bypass on the secret prefix. Regenerated on every library write |
| fire | `GET /api/webhook/<blast-id>?code=<slug>` | **LAN + away** | Existing webhook, unchanged. **name lookup only** |
| status | `GET /local/ir-<secret>/status.json` | **LAN + away** | `{tx_counter, last_slug, last_fired_at, last_error}` |
| write / arm | HA dashboard | **LAN + away, authenticated** | Behind an Access **login**, not a bearer URL |

**All three watch-facing endpoints share one regime, and the secret is what makes that
safe.** The path segment is a 128-bit capability — the same model as the webhook id.
A plain `/local/ir-codes.json` would have been a guessable path with no credential;
bypassing Access on *that* would publish the library to the internet.

**One failure mode survives the fix and the watch must handle it.** If the secret is
wrong, rotated, or Access is misconfigured, the response is a **302 to a login page —
HTML, not JSON**. With no status code on this transport that is indistinguishable from
data: it will fail `JSON.parse`, or worse half-parse (the R3 failure mode), with no way
to report why. **The watch must treat an unparseable list/status response as
"unreachable", never as an empty library.** This is why R5 and new question A (does the
relay follow redirects?) matter more than they first looked.

**Never accept a raw array over the webhook.** `?code=<slug>` is bounded to codes you
captured. `?raw=1280,-420,...` would be arbitrary IR transmission by anyone holding the
id, drive-by triggerable via `<img src>` exactly as `automations/bangle_irblast.yaml`
describes. The arbitrary-array path stays on the authenticated HA API.

**~~Split the ids~~ — SUPERSEDED by the H1 decision.** There is no second webhook. The
one endpoint that mutates state goes through the authenticated dashboard instead, which
is stronger than a second bearer URL would have been. The webhook count stays at one.

**The `/api/webhook/` prefix rule still stands, and applies to the new secret prefix
too.** `home-assistant-pi` warns that a `/api/webhook/` **prefix** bypass would expose
the live `Pixel 9` `mobile_app` webhook, which accepts `call_service`. The reason a
prefix bypass is acceptable for `/local/ir-<secret>/` is *only* that the prefix itself
contains the secret and has no siblings. **Never** widen it to `/local/`.

**The confirmation mechanism.** `tx_counter` is incremented **on the ESP32**, not in HA,
and surfaced through the API. That distinction is the whole value: HA's `last_triggered`
only proves HA *tried*, whereas a device counter proves IR left the LED. Watch reads the
counter, fires, waits, re-reads. Moved = transmitted. It also gives bad slugs somewhere
to surface (`last_error`) instead of vanishing into an indistinguishable 200. Cost:
**two round trips per fire**, which is why R7 matters.

*Deferred option, noted so it is not rediscovered:* the receiver hears the transmitter,
so a transmit could be confirmed by the RX path observing its own emission — the R9
footgun inverted into a feature. Genuinely strong, but it couples capture and blast
paths that are otherwise independent. Not for v1.

### Capture gating (four parts)

Gating on the device beats gating in HA — no bus traffic from ambient IR at all.

1. **Arm switch** — a template switch checked inside the `on_raw` lambda.
2. **Auto-disarm** on a timeout, so a forgotten arm does not leave the room recording.
3. **First-capture-wins**, then disarm. Remotes repeat frames — the fan code visibly
   contains two halves plus a 7120 µs gap, and NEC sends distinct repeat codes.
4. **Suppress during transmit.** See the receiver-hears-transmitter constraint.

## Phases

Ordered so the load-bearing unknown fails first and cheapest.

### Phase 0 — prove the premise · *IR agent*
Confirm `on_raw:` fires on real hardware and that its array round-trips to toggle the fan.
**Everything else is wasted effort if this fails.**

**Phase 0 runs entirely on-device — no Home Assistant in the loop.** The lambda feeds
`remote_transmitter.transmit_raw:` directly in the YAML. That is deliberate: it tests the
one unproven thing (the receiver and the array shape) without depending on the HA hop, so
a failure is unambiguous. `send_raw_ir` and the HA path are proven separately in Phase 2.

### Phase 1 — measurement session · *joint*
Run the research items below. Converts the transport assumptions into numbers. Needs the
watch on the phone (not the Mac) and a temporary HA endpoint. IR agent provides the
endpoint; Bangle agent runs the watch side; a human opens the BLE window.

### Phase 2 — library format · *IR agent, Bangle agent reviews*
Ratify or replace the strawman above, with Phase 1's sizing as input. **This is the seam
and the only real coupling.**

### Phase 3 — read-only watch UI · *Bangle agent*
List codes, fire one, poll the counter. Cache the library to Storage so the menu renders
without a round trip. Honest state labels throughout — no claim the transport cannot
support.

**Capture still exists in v1 — it is just not armed from the wrist.** The firmware
capture path and the HA write path are both in scope here, arming from an HA dashboard
toggle. Without them the library cannot grow and this is a nicer UI over one hard-coded
array.

### Phase 4 — arm capture from the watch · *deferred, both agents agree*
You must be in the room pointing a remote anyway, so watch-initiated arming buys little
and costs the most complexity — a live polling screen and a naming affordance the watch
is bad at. Defer until Phase 3 is in daily use. Needs H2 to make it explicit.

---

# Before we begin

Everything blocking is listed here, grouped by who does it. IDs are stable so they can be
cross-referenced; `R` items are empirically answerable, `H` items need judgement or
physical access.

## IR agent — this repo *(core agent for the build)*

Runnable without the watch and without the Bangle agent. **R1 blocks everything.**

- [x] **R1 — PASS, 2026-08-09. The premise holds.** `on_raw` fired, the captured array
      replayed through `transmit_raw`, and the fan toggled with no second press. Took two
      runs: run 1 exposed inverted RX polarity (0/24 signs matching the known-good array),
      run 2 with `inverted: true` matched 24/24 and actuated. Closed four `validation.md`
      items at once — receiver dump, round trip, OTA (twice), and the transmit path.
      Three findings came with it: **RX polarity was documented backwards**, the fan
      remote **decodes as Symphony `0xD81`/12** rather than being unknown, and a capture
      holds **~2.5 repeats** rather than one frame.
- [ ] **R12 — Correct `validation.md`: HA *adoption* is proven, but only adoption.**
      Adoption stands on entity **enumeration** — `button.ir_blaster_living_room_fan_power`
      exists, and HA only creates it from a list-entities response over the ESPHome
      integration. That is a separate claim from delivery (R15), and conflating the two is
      what went wrong three revisions running. Tick adoption, cite enumeration, leave
      delivery open.
      **Also disambiguate `validation.md:26-27` while in there.** It ticks "actuates the
      real device" but qualifies it "over the network via **REST**" — most plausibly
      ESPHome's own `web_server`, which bypasses HA's native API entirely. If so, it is
      not evidence for R15 and the line should say which REST it means. Three rows have
      now turned out stale or ambiguous; assume the rest are too.
- [x] **R15 — HA → ESP32 delivery. CLOSED 2026-08-07, PASS.** Owner pressed
      `button.ir_blaster_living_room_fan_power` in HA and watched the fan toggle, on the
      standing production firmware with nothing flashed beforehand. Phase 2 is unblocked.
- [x] **R13 — capture length. Fixed, but the first attempt targeted the wrong knob.**
      `receive_symbols: 256` in all three configs (~512 entries). **`buffer_size` is not
      the per-frame ceiling** — it floors a queue of completed events awaiting the main
      loop (`remote_receiver_rmt.cpp:107`), and its esp32 default is already `10000b`
      (`__init__.py:112`), so the original "raise 2 kb → 8 kb" was still *below* stock and
      fixed nothing. The real ceiling is `receive_symbols`, the `rmt_receive` length
      (`:109`), default 192 × 2 pulses per symbol ≈ **384 entries** — under the 450 an
      HVAC frame reaches, truncating silently. `buffer_size` now pinned at `10000b`
      explicitly so it is not quietly reduced again. All three configs validate; the
      round-trip firmware recompiles clean.
- [x] **Noise guard added to the R1 latch.** `filter_symbols` defaults to 0, so the ISR
      filter rejects nothing and any blip past the 50 µs `filter` arrives as a short
      array. Unguarded, one spurious burst would latch a 2-entry capture, discard the real
      remote press, and transmit garbage — reported as "R1 PARTIAL", misdiagnosing the
      gate. Now requires ≥10 entries and logs what it ignored.
- [ ] **R11 — Measure real array lengths** for whatever remotes are actually in the room.
      Partly answered already: 30–70 typical, 100–450 for HVAC. What is unknown is which
      of those this household has.
- [ ] **R14 — Confirm the rotation against the Pi**, not against the repo. The correction
      above rests on `home-assistant-pi`'s own `CHANGELOG.md` and automation comments. One
      `ssh pi5` read of `secrets.yaml` settles it and closes H1.
- [ ] Provide the temporary echo endpoint the Bangle agent needs for R2–R4 (ramped
      payload sizes, query-string echo). Blocks their half of Phase 1.
- [ ] Build the library format and the three endpoints, once Phase 1 sizing lands.

## Bangle agent — `my-bangle` repo *(watch-side dev and measurement)*

All of these need the watch on the phone and a human to open the BLE window (H5), so
they want to be batched into one sitting. Answers feed directly into Phase 2 sizing.

- [ ] **R2 — Can `Bangle.http` return a non-empty body, and how large?** Never observed.
      Ramp against the IR agent's endpoint: 100 B → 1 KB → 4 KB → 16 KB. Find the ceiling.
      **This is the highest-value unknown on the watch side** — the whole list endpoint
      depends on it, and it inverts the current app's assumption that a non-empty body
      means failure.
- [ ] **R3 — Does an over-size response truncate or reject?** If it truncates silently,
      the payload needs a length field or checksum. Truncated JSON usually fails
      `JSON.parse` — but not always, and a half-parsed library is a bad failure mode.
- [ ] **R4 — Do query strings survive the relay intact?** `?a=b&c=d`, echoed back. No
      stripping, no double-encoding. **This one has no clean workaround** — HA webhooks do
      not route on path segments. The only fallback is one webhook trigger *per code* in a
      single automation, distinguished by `trigger.id`, which means a separate bearer
      credential and a separate Access destination for every code. It does not scale, so
      treat a negative result here as a serious finding rather than a detail.
- [ ] **R5 — Is HTTPS reachable?** Specifically `https://ha.raylytics.io`. Determines
      whether the external path depends on a Cloudflare Access bypass.
- [ ] **R6 — Does mDNS resolve from the phone?** Could the watch hit
      `http://ir-blaster-livingroom.local/` directly? Would cut HA out of the read path.
      Worth knowing even if not taken.
- [ ] **R7 — Real round-trip latency, tap → resolved.** Ten samples. The proposed
      confirmation costs **two** round trips per fire, so this decides whether that design
      is viable or whether confirmation must be deferred.
- [ ] **R8 — `JSON.parse` cost on-device for a few KB.** Feeds the flat-vs-nested
      decision in the schema.
- [ ] **R9 — Back-to-back requests.** The current app deliberately allows one in flight
      (a double-tap must not fire the fan twice). Fetch-list-then-fire, and
      fire-then-poll-counter, are both sequential patterns that are untested.
- [ ] **R10 — Watch RAM headroom.** `process.memory()` before and after holding a parsed
      library. Storage is not the constraint; RAM is.
- [ ] **Correct `apps/irblast/NOTES.md:43` in the sibling repo.** *"A tap on the watch
      fired the fan"* reads as a physical observation but the same sentence grounds it in
      *"Measured from Home Assistant rather than taken on report"*, and that note's own
      chain table ends at `webhook → automation → button`. It has now been mis-cited as
      delivery evidence three times — once by the IR agent, from this repo. Reword it to
      what was actually observed, or add the missing `button → ESP32 → fan` leg if someone
      did watch the fan and it simply went unrecorded.
- [ ] **Review the schema above** and push back on `short` ≤14, the `status` enum, and
      flat-vs-nested. Sizing is yours to constrain.
- [ ] Phase 3 build: app, transport, UI, caching.

## Human — owner decisions and physical access

- [x] **H1, H2, H3, H4, H6 — DECIDED 2026-08-07.** See
      [Decisions taken](#decisions-taken--2026-08-07). Do not re-litigate.
- [x] **Board access — answered.** On WiFi, USB-C data cable available as the OTA
      fallback.

Still open:

- [x] **H5 sitting 1 (hardware) — DONE 2026-08-09.** R15 ✅, R13 ✅, R1 ✅, OTA ✅.
      **The hardware gate is fully closed and Phase 2 is unblocked.**
      *Outstanding from that sitting:* restore production firmware
      (`esphome run ir-blaster-livingroom.yaml`) if not already done, and optional R11
      sampling of other remotes — now much more valuable, since it decides how many codes
      can use the compact decoded form rather than raw arrays.
- [ ] **H5 sitting 2 (watch) — now schedulable.** Watch on the phone, Chrome's device
      chooser needs a human click. Covers the Bangle agent's R2–R10. No longer gated.
- [ ] **H7 — Does the watch's `irblast.json` hold the rotated id?** The single button is
      reported working, which implies yes, but the rotation runbook lists re-flashing the
      watch as part of the change. **Not blocking the hardware work**, but confirm before
      the watch app is rebuilt on top of it.
- [ ] **New — generate the read secret on the Pi.** Same discipline as the webhook id:
      `openssl rand -hex 16` on the Pi, never through a session, never committed. Needed
      before the read endpoints exist, not before the hardware work.

---

## Boundary between the two projects

| owner | scope |
|---|---|
| **IR agent** (this repo) | ESP32 firmware and triggers, HA-side code library, all three HA endpoints, capture gating |
| **Bangle agent** (`my-bangle` repo) | Watch app, BLE transport behaviour, UI, on-device caching |
| **Joint** | The library format (Phase 2) and the measurement session (Phase 1) |
| **Human** | Owner decisions above, and physical access for every watch session |

**The library format is the only real coupling.** It is where two agents working in
parallel would otherwise diverge, so it should be agreed explicitly and written down
before either side builds against it. The strawman above exists to be argued with.
