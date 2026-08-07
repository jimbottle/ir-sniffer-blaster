# Bangle.js integration — project plan

Goal: **review captured IR signals and fire any of them from the Bangle.js 2 watch.**

Today the watch has a single full-screen button that fires one hard-coded webhook
(`my-bangle` repo, `apps/irblast`). This plan covers turning that into a browsable
library, and covers what would be needed for the watch to participate in capture.

Two repos and two agents are involved. The boundary is stated at the end.

> **Status: planning only.** Nothing here is built. The measurement session in
> "Research needed" has not been run, so most sizing decisions below are open.

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

Two HA endpoints are needed:

| endpoint | direction | purpose |
|---|---|---|
| list | HA → watch | returns the code library as JSON so the watch can render a menu |
| fire | watch → HA | takes a code identifier, calls `send_raw_ir` with the stored array |

## What is proven, and what is assumed

This section exists because the plan below is only as good as this table. Mirrors the
discipline of [`validation.md`](validation.md).

| claim | status |
|---|---|
| ESPHome exposes a **raw receive trigger** | **VERIFIED** by reading this repo's own vendored ESPHome: `remote_base/__init__.py` has `@register_trigger("raw", RawTrigger, cg.std_vector.template(cg.int32))`. Payload is `std::vector<int32_t>` — the same shape `send_raw_ir` consumes |
| That trigger **fires on this hardware** | **UNVERIFIED.** See `README.md` acceptance criteria: the ESPHome `remote_receiver` dump has not been observed at all. RX is proven only via the Arduino diagnostics, a different code path |
| A capture can round-trip `on_raw` → `send_raw_ir` → device responds | **UNVERIFIED.** This is the single test that validates the whole premise |
| Watch can reach HA over **plain HTTP on the LAN** | **PROVEN** 2026-08-04 — a watch tap moved HA's `automation.bangle_ir_blaster_fan_power` `last_triggered` |
| `Bangle.http` resolves with the **response body** | **SOURCE READ ONLY.** `apps/android/lib.js:134-145` resolves `{t:"http", resp:"<body>", id}`. **No non-empty body has ever been observed on this setup** — every request so far hit a webhook that answers 200 empty |
| `Bangle.http` exposes an **HTTP status code** | **NO — it does not exist in the contract.** A 404, a captive-portal splash and a success are indistinguishable |
| Query strings survive the relay | **UNVERIFIED.** Everything today is path-only |
| HTTPS / mDNS reachable from the watch | **UNVERIFIED** |
| `textinput` available on the watch | **NO — not installed.** Device app list read 2026-08-06: 23 apps, no `textinput`. It exists in the App Loader and could be added |
| Watch storage headroom | **MEASURED** 2026-08-06: 7,071,108 bytes free. Not a constraint |
| Watch RAM headroom | **UNMEASURED.** This is the real constraint on Bangle.js 2, not storage |

## Constraints that shape the design

**The watch cannot confirm anything fired.** No status code exists on this transport.
The existing app says `RELAYED`, not `SENT`, for exactly this reason — an earlier build
printed `SENT` for requests Home Assistant never received. A multi-code UI makes this
worse: with one button you at least know *which* thing did not happen. **If the HA or
ESPHome side can expose a pollable success signal — `last_triggered`, a transmit
counter, anything — it materially improves this UI.**

**HA caps entity state at 255 characters.** The one captured code is 47 entries, ~250
characters serialised — already at the edge. Raw arrays must live in event payloads or
attributes, never in an entity state.

**`on_raw` will fire on every IR burst in the room**, including noise and unrelated
remotes. Capture needs gating — a learn-mode switch or a timeboxed window.

**Rolling codes cannot be replayed.** [`captured-codes/README.md`](../captured-codes/README.md)
already flags this. The library format must be able to mark a code unfireable, or the
watch will list dead buttons.

**The current webhook id is burned.** Tracked in the other repo as `my-bangle-8vt`:
the id was committed to git history and has not been rotated. Broadening it from "fires
one fan" to "fires anything in the library" widens what a leaked URL can do. **Rotate
before broadening.**

**Every watch action depends on two Android processes.** Gadgetbridge *and* the
Internet Helper add-on. The helper is not currently exempt from battery optimization
(`my-bangle-o7i`), and a killed helper breaks `Bangle.http` while Gadgetbridge still
looks connected.

**Deploying to the watch takes the BLE slot the phone uses.** The switching loop is
solved and documented (`my-bangle` repo, `docs/troubleshooting.md`) — phone Bluetooth
off, Make Connectable, connect, answer Yes, phone Bluetooth on. No unpairing. But
Chrome's device chooser needs a human click, so a human must open each window. **Favour
fewer, larger deploys over many small ones.**

## Phases

Ordered so the load-bearing unknown fails first and cheapest.

### Phase 0 — prove the premise
Confirm `on_raw:` fires on real hardware and that its array round-trips through
`send_raw_ir` to toggle the fan. **Everything else is wasted effort if this fails.**
Owner: IR agent.

### Phase 1 — measurement session
Run the tests under "Research needed" below. Converts the transport assumptions into
numbers. Needs the watch on the phone (not the Mac) and a temporary HA endpoint.
Owner: joint — IR agent provides the endpoint, Bangle agent runs the watch side.

### Phase 2 — library format
Agree slug → raw-array storage, its metadata, and the two HA endpoints. **This is the
seam between the two projects and the only real coupling.** Owner: IR agent, with the
Bangle agent's sizing constraints from Phase 1 as input.

### Phase 3 — read-only watch UI
List codes, fire one. Cache the library to Storage so the menu renders without a round
trip. Honest state labels throughout — no claim the transport cannot support.
Owner: Bangle agent.

### Phase 4 — capture from the watch *(optional, argue against)*
You must be in the room pointing a remote anyway, so watch-initiated capture buys
little and costs the most complexity — it needs a live polling screen and a naming
affordance the watch is bad at. Recommend deferring until Phase 3 is in daily use.

---

# Open questions

## Research needed

Empirically answerable. No judgement, just nobody has run them.

### R1 — Does `on_raw:` fire on this hardware, and is its array usable?
The trigger is verified to exist in ESPHome. Not verified to fire here, and the
ESPHome receiver dump has never been observed at all. Also unknown: whether the vector
is byte-identical in shape to what `dump: all` prints, and whether it feeds straight
back into `transmit_raw` unmodified. **Blocking everything.**

### R2 — Can `Bangle.http` return a non-empty body, and how large?
Never observed. Ramp a test endpoint 100 B → 1 KB → 4 KB → 16 KB. Find the ceiling.

### R3 — Does an over-size response truncate or reject?
If it truncates silently, the payload needs a length field or checksum. Truncated JSON
usually fails `JSON.parse` — but not always, and a half-parsed code library is a bad
failure mode.

### R4 — Do query strings survive the relay intact?
`?a=b&c=d`, echoed back by the endpoint. No stripping, no double-encoding. If they do
not survive, the fire endpoint needs a different shape — HA webhooks do not natively
route on path segments.

### R5 — Is HTTPS reachable from the watch?
Specifically `https://ha.raylytics.io`. Determines whether the external path depends on
a Cloudflare Access bypass.

### R6 — Does mDNS resolve from the phone?
Could the watch hit `http://ir-blaster-livingroom.local/` directly? Would cut HA out of
the read path entirely. Worth knowing even if not taken.

### R7 — Real round-trip latency, tap → resolved
Never measured. Ten samples. Decides whether a live "waiting… captured!" screen is
viable or whether confirmation must be deferred.

### R8 — `JSON.parse` cost on-device for a few KB
Unmeasured on 2v29. Feeds the flat-vs-nested decision.

### R9 — Back-to-back requests
The current app deliberately allows one in flight (a double-tap must not fire the fan
twice). Whether sequential requests are clean — fetch list, then fire — is untested.

### R10 — Watch RAM headroom
`process.memory()` before and after holding a parsed library. Storage is not the
constraint; RAM is.

### R11 — How long do raw arrays get in practice?
The fan is 47 entries. Anything materially longer changes the payload sizing and may
push past what R2 finds.

## Human input needed

Decisions, judgement, or physical access. An agent should not settle these alone.

### H1 — Rotate the webhook id before broadening it?
`my-bangle-8vt` records the current id as burned and unrotated. Going from one action
to many on a URL already in git history is a deliberate security choice. **Recommend
rotating first.** Owner's call.

### H2 — Is watch-initiated capture in scope at all?
Recommend no for v1, for the reasons in Phase 4. Needs an explicit decision so the HA
side is not designed around a flow that is never built.

### H3 — What is the naming vocabulary?
`textinput` is not installed and is unpleasant at 176×176. The practical option is two
menu picks — device × function — from fixed lists shipped in the JSON. That constrains
captures to a predefined vocabulary. **Better designed in deliberately than discovered
later.** Who maintains the vocabulary, and how does a genuinely new device get added?

### H4 — Can a verifiable success signal be exposed?
The watch cannot see HTTP status. If HA or ESPHome can offer something pollable — a
transmit counter, `last_triggered`, a sequence number — the UI stops having to
guess. Worth a small amount of HA-side work; changes the honesty ceiling of the whole
feature.

### H5 — Physical access for the measurement session
Phase 1 needs the watch on the phone, plus somebody to open the BLE window (Chrome's
device chooser needs a human click). Roughly one sitting.

### H6 — Should the ESPHome `remote_receiver` acceptance criteria be closed first?
`README.md` still lists the receiver dump, HA adoption and OTA as unexercised. Building
a watch UI on an unvalidated receiver would be building on sand. Recommend closing those
first; needs an owner decision on sequencing.

---

## Boundary between the two projects

| owner | scope |
|---|---|
| **IR agent** (this repo) | ESP32 firmware and triggers, HA-side code library, both HA endpoints, capture gating |
| **Bangle agent** (`my-bangle` repo) | Watch app, BLE transport behaviour, UI, on-device caching |
| **Joint** | The library format (Phase 2) and the measurement session (Phase 1) |

**The library format is the only real coupling.** It is where two agents working in
parallel would otherwise diverge, so it should be agreed explicitly and written down
before either side builds against it.
