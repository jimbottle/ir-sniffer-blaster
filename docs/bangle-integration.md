# Bangle.js integration — project plan

Goal: **review captured IR signals and fire any of them from the Bangle.js 2 watch.**

Today the watch has a single full-screen button that fires one hard-coded webhook
(`my-bangle` repo, `apps/irblast`). This plan covers turning that into a browsable
library, and covers what would be needed for the watch to participate in capture.

Two repos and two agents are involved. The boundary is stated at the end, and the
task split is under [Before we begin](#before-we-begin).

> **Status: planning only.** Nothing here is built. The measurement session has not
> been run, so most sizing decisions below are open.

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
| That trigger **fires on this hardware** | **UNVERIFIED.** See `README.md` acceptance criteria: the ESPHome `remote_receiver` dump has not been observed at all. RX is proven only via the Arduino diagnostics, a different code path |
| A capture can round-trip `on_raw` → `send_raw_ir` → device responds | **UNVERIFIED.** This is the single test that validates the whole premise |
| The board is **adopted in Home Assistant** | **PROVEN**, and `validation.md` is stale on this — it still lists adoption unticked. Evidence is the *existence* of `button.ir_blaster_living_room_fan_power`, which HA only creates by enumerating the device over the ESPHome integration, and `automations/bangle_irblast.yaml` presses it by `entity_id`. Correcting `validation.md` is R12 |
| **HA → ESP32 over the encrypted API** delivers an action | **UNVERIFIED.** HA queues the command; nothing observed shows the device received it. See the standing rule below. Closing it is cheap — R15 |
| 🚨 **STANDING RULE: no HA-side artefact proves this hop.** | **Three consecutive revisions of this doc asserted it and were wrong, each on a different artefact.** Do not assert it again without device-side or physical evidence. What has been tried and does **not** work: (1) `automation.…last_triggered` — proves the automation started; (2) `button.….state` — **ESPHome buttons are stateless.** No `ButtonStateResponse` exists anywhere in the vendored protocol (zero hits across `site-packages`) and `ButtonCommandRequest` is fire-and-forget with no reply (`aioesphomeapi/client.py:1661`), so the state is a **locally-generated `last_pressed`** written by HA's own `ButtonEntity`; (3) `my-bangle/apps/irblast/NOTES.md:43` *"A tap on the watch fired the fan"* — **not an independent observation.** The same sentence says *"Measured from Home Assistant rather than taken on report"*, and that note's own chain table ends at `webhook → automation → button`, with no `button → ESP32 → fan` leg. It is a summary of artefacts (1) and (2) |
| `homeassistant.event` reaches HA **from** the device | **UNVERIFIED**, but low risk — and deliberately *not* grounded on the disputed hop. The justification is entity **enumeration**: HA only creates `button.ir_blaster_living_room_fan_power` by receiving a list-entities response *from* the device, which is device→HA traffic on the same persistent connection an event would use |
| **OTA** works on this board | **UNVERIFIED.** Both flashes to date were USB. The capture firmware change needs at least one flash — budget for USB and treat wireless as unproven |
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

| endpoint | shape | exposure | notes |
|---|---|---|---|
| list | `GET /local/ir-codes.json` | **LAN only** | HA serves `www/` unauthenticated on the LAN. From outside it is behind Access. Regenerated on every library write |
| fire | `GET /api/webhook/<blast-id>?code=<slug>` | **LAN + away** | The one Access-bypassed path. **name lookup only** |
| status | `GET /local/ir-status.json` | **LAN only** | `{tx_counter, last_slug, last_fired_at, last_error}` |

**⚠️ The three endpoints do not share one exposure regime, and v1 ships that way
deliberately.** Fire is bypassed through Cloudflare Access on its exact path; list and
status are `/local/` static files with no bypass. **So from away the watch can fire but
cannot read the library or poll the counter.** Consequences, stated here rather than
left for Phase 3 to discover:

- **Away-firing works, but blind.** No confirmation, and the menu comes from the watch's
  cached copy. That is acceptable for v1 — it is exactly today's behaviour plus a menu —
  but it means H4's confirmation answer is **LAN-only**, and the honest-UI argument has
  to say so.
- **An Access challenge returns a login page, not JSON.** With no status code on this
  transport, a 302-to-HTML is indistinguishable from data: it will fail `JSON.parse`, or
  worse half-parse (the R3 failure mode), with no way to report why. **The watch must
  treat an unparseable list/status response as "unreachable", never as an empty library.**
  This is why R5 and new question A (does the relay follow redirects?) matter more than
  they first looked.
- **The upgrade path, if away-reads are wanted later:** two more Access applications,
  exact-path scoped to `/local/ir-codes.json` and `/local/ir-status.json`. Cost is that
  the code library — an inventory of the room's devices — becomes readable by anyone on
  the internet who has the URL. Not secret, but not nothing. **Not recommended for v1**;
  noted so it is not rediscovered as a surprise.

**Never accept a raw array over the webhook.** `?code=<slug>` is bounded to codes you
captured. `?raw=1280,-420,...` would be arbitrary IR transmission by anyone holding the
id, drive-by triggerable via `<img src>` exactly as `automations/bangle_irblast.yaml`
describes. The arbitrary-array path stays on the authenticated HA API.

**Split the ids.** Blast keeps the existing rotated id and `local_only: false` — firing
from away is owner-confirmed twice. Any write/capture endpoint gets a **second** webhook,
`local_only: true`, so writes require LAN presence. Cost: a second Cloudflare Access
destination, which must be an exact path — `home-assistant-pi` warns that a
`/api/webhook/` **prefix** bypass would also expose the live `Pixel 9` `mobile_app`
webhook, which accepts `call_service`.

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

- [ ] **R1 — Does `on_raw:` fire, and does its array round-trip?** With the fan remote:
      `on_raw` fires → lambda captures `x` → **that array**, not the log text and not the
      committed array, is fed to `send_raw_ir` → the fan toggles. One test, and it
      collapses four unticked `validation.md` items at once. Two edge risks to watch: the
      receiver applies its own filtering, so a live capture may differ from the stored
      array at the **edges** (the committed array ends on a mark, `1230`, with no
      trailing space); and `buffer_size` bounds capture length. **Blocking everything.**
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
- [ ] **R15 — Prove HA → ESP32 delivery, or record it as open.** *(New; the hop three
      revisions asserted without evidence.)* The test is ~10 seconds: press
      `button.ir_blaster_living_room_fan_power` **from Home Assistant** and watch whether
      the fan physically toggles. Acceptable evidence is a human observing the fan, a
      device-side log line, or `tx_counter` moving — **never** an HA-side timestamp.
      Gates Phase 2, not Phase 0 (which runs on-device with no HA in the loop).
- [ ] **R13 — Raise `buffer_size` past 2 kb** and confirm the config still validates.
      Do it before anyone tries to capture an HVAC frame.
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

Agents should not settle these alone.

- [ ] **H5 — Physical access for the measurement session.** Phase 1 needs the watch on
      the phone plus somebody to open the BLE window; Chrome's device chooser needs a
      human click. Roughly one sitting, and it gates the Bangle agent's entire list above.
- [ ] **H6 — Sequencing: close the hardware acceptance criteria first?** The open items
      are the **receiver dump** (R1), **HA→ESP32 delivery** (R15) and **OTA**. Adoption
      came off the list; delivery went back on after review. Both agents recommend closing
      R1 before any UI work, and R15 is ~10 seconds so it should just be done. Needs a
      decision because it front-loads hardware work before anything visible ships.
- [ ] **H1 — Widening the webhook.** *Reduced in scope this revision* — rotation already
      happened, so this is no longer "rotate first." What remains: is broadening the id
      from one action to a whole library acceptable, and do you want the **split-id**
      proposal (blast stays `local_only: false`, writes go to a second `local_only: true`
      webhook)? Costs a second Cloudflare Access destination.
- [ ] **H2 — Is watch-initiated capture in scope at all?** Both agents now recommend
      **no for v1**. Needs an explicit decision so the HA side is not designed around a
      flow that is never built. Note this is *not* "no capture in v1" — the firmware and
      HA capture paths stay; only arming from the wrist is dropped.
- [ ] **H3 — What is the naming vocabulary?** `textinput` is not installed and is
      unpleasant at 176×176, so the practical option is two menu picks — device × function
      — from fixed lists shipped in the JSON. That constrains captures to a predefined
      vocabulary. Who maintains it, and how does a genuinely new device get added?
      **Better designed in deliberately than discovered later.**
- [ ] **H4 — Confirm the transmit-counter approach.** *Answered in principle this
      revision* — an ESP32-side counter is exposable, and it proves the device acted
      rather than that HA tried. What needs a decision is the cost: two round trips per
      fire, and a static-file rewrite per press. Fine at this volume, not free.
- [ ] **H7 — Does the watch's `irblast.json` hold the rotated id?** The single button is
      reported working, which implies yes, but the rotation runbook lists re-flashing the
      watch as part of the change. Worth confirming before anything is built on top.

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
