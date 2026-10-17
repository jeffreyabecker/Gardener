# Key-value store and link-layer backlog

Status: **proposed — not started.** This document is the plan; no code has changed.

## Goal

One refactor, two pieces:

1. Replace the `SystemSnapshot` struct with a general string key-value **state store** that is the
   single source of truth for device state.
2. Introduce a **link-layer abstraction** over an internal **publish/subscribe** core, so the
   serial link is one of several link layers. Every link layer can **issue commands**, **read
   state**, and **subscribe to events** — and none of them own the logic that produces a response.

The serial link is not ever responsible for directly producing responses. It parses its own
transport into a normalized command, hands it to a shared dispatcher, and renders the returned
result to the wire. For push, it subscribes to an internal event bus and renders each event it
receives. Other link layers do the same with different codecs.

## Core principle

A link layer is a **codec, not a brain**:

- it parses its own transport into a normalized `Command` and submits it to a shared dispatcher;
- the dispatcher executes against the state store and the domain modules, and returns a normalized
  `Result`;
- the link layer renders that `Result` in its own wire format — it never produces a response
  itself;
- for push, the link layer subscribes to the internal event bus and renders each `Event` to its
  transport.

## Locked-in decisions

1. **Strings, not types.** Keys are strings, values are strings, and the set is open at runtime.
   The store is a fixed-capacity map of `String` key to `String` value. Magic strings are
   accepted; the key *name* is the contract. The protocol's `-` ("unset") is the literal string
   `"-"`.
2. **The store is authoritative.** Published state has one copy, in the store. After an action or
   write mutates real state, the affected keys are re-published *before* the result is rendered,
   so `plant start` still answers `state=PULL` — the freshness invariant today guaranteed by
   `WireView` copying `system_.state()` live.
3. **Link layers are renderers and parsers.** Command execution, validation, and event semantics
   live in shared core (store + dispatcher + event bus), never in a link layer.
4. **Push by default, suppress per link.** The serial link renders change records for every event
   it receives, automatically. Each link layer owns an internal suppression list ("unsub") of
   keys whose events it does not render; a controller silences a noisy key by adding it to that
   list. Existing commands and their replies stay byte-identical proto 2; the new unsolicited
   change records are the additive surface.
5. **All keys are wire-visible by default.** Every store key can be queried by name, even when it
   is not worth a `help` row. The serial layer keeps a string-keyed schema for `help` and write
   validation; the schema is a curated subset, not the visibility boundary.
6. **Link-local settings are per-link.** `term`, `log`, and `attached` belong to each link layer,
   not to the global store. The dispatcher resolves link-local fields against the link that issued
   the command.

### Superseded decisions

The earlier "in-process notifications only" decision is replaced by decision 3: the notification
mechanism is now the shared event bus, and link layers subscribe to it — all events by default,
minus a per-link suppression list.

## Architecture

```
                                 application core
producers ── upsert ──► StateStore (String -> String) ── key_changed ──┐
(VacuumSystem,           ^                                             │
 Battery, App)           │ read / enumerate                            ▼
                         │                                        EventBus (pub/sub)
                         │                                             ▲
                    CommandDispatcher ── publish(Event) ───────────────┤
                    (command surface,                                  │
                     validation, writes, actions)                      │
                         ▲                                             │
                         │ execute(Command) ─► Result                  │
                         │                                             │
                    LinkLayer (interface) ── subscribe ────────────────┘
                    parse() / renderResult() / renderEvent()
                         │  (one per transport)
                         ▼
                    SerialLink (proto 2 codec)
                    term / log / attached (link-local)
```

### Components

**StateStore** — fixed-capacity `String -> String` map with per-key revision. `upsert`, `get`,
`enumerate`. Emits a `key_changed` event only when a value actually changes. `-` is the unset
value.

**EventBus** — internal publish/subscribe. `publish(Event)` delivers to subscribers registered by
type and/or key. Bounded subscriber list, no allocation.

**Event** — `{ type, key, value }`, where `key`/`value` are optional. Types:

| Type | Producer | Payload |
| --- | --- | --- |
| `key_changed` | StateStore | `key`, `value` |
| `run_state` | VacuumSystem | `key=state`, `value` |
| `fault` | VacuumSystem | `key=fault`, `value` |
| `vent_timeout` | VacuumSystem | — |
| `controller_attached` / `controller_lost` | link layer | link name |
| `log` | any core component | `level`, text |

**CommandDispatcher** — owns the command surface: the `ResourceSpec`/`FieldSpec`/`ActionSpec`
tables and the write/action logic, moved out of the serial link. `execute(Command, LinkContext)`
returns a normalized `Result` (`{ ok, code, items[] }` of `{key, value}` pairs — no text).
Link-local fields are resolved through the `LinkContext`.

**LinkLayer** — the interface: `begin()`, `poll()`, `renderResult(Result)`, `renderEvent(Event)`,
plus references to the store, dispatcher, and event bus. Each implementation owns its transport
codec and its link-local settings.

**SerialLink** — the first `LinkLayer`. Parses proto 2 text into `Command`; renders `Result` as
`ok key=value …` / `err code …`; renders events as pushed records by default, minus its
suppression list. `Items` becomes the serial renderer of a canonical item list.

**Overlay** — an in-process consumer of the store (and, if it wants, a subscriber); not a link
layer.

**App** — composition root: owns the store, event bus, dispatcher, link layer(s), and domain
modules; wires subscriptions (e.g. `controller_lost` → stop and vent).

## How the existing code maps

| Today | Becomes |
| --- | --- |
| `SystemSnapshot` | `StateStore` entries |
| `App::refreshSnapshot()` | producers upsert each loop |
| `WireView` | removed; dispatcher reads store + domain directly |
| `SerialLink::command()` / `resourceCommand()` | `serial parse → dispatcher.execute → serial render` |
| `FieldSpec` / `ResourceSpec` / `ActionSpec` + `read*` / `write*` | dispatcher's transport-neutral command surface |
| `Items` | serial renderer of a canonical item list |
| `SerialLink::info/warn/error` | publish `log` events; serial renders them when `log` is on |
| `SerialLink::consumeLinkLost()` | `controller_lost` event; `App` subscribes |
| `SerialLink::term/logEnabled_/attached_` | link-local members of `SerialLink` |

## Backlog

Dependencies are listed per task. "Ready" means all dependencies are done.

### Epic 1 — Core model

**link-design-command-result — Defining normalized Command and Result**

- `Command` = `{ resource, op, fields[] (name/value), rest }`; `Result` = `{ ok, code, items[] }`
  of `{key, value}` pairs. Transport-neutral; no wire text.
- Depends on: nothing.

**link-design-event — Defining the Event model**

- `Event` = `{ type, key, value }` with the table above. Subscriber registration by type and/or
  key.
- Depends on: nothing.

**link-design-interface — Defining the LinkLayer interface**

- `begin()`, `poll()`, `renderResult(Result)`, `renderEvent(Event)`, plus store/dispatcher/event
  bus references. Each implementation owns a suppression list of keys whose events it does not
  render, alongside its other link-local settings.
- Depends on: link-design-command-result, link-design-event.

### Epic 2 — Store

**store-implement — Implementing the string state store**

- Fixed-capacity `String -> String` map; `upsert` inserts or replaces; `get`; `enumerate`;
  per-key revision; `key_changed` emitted only on real change. Define overflow behaviour.
- Depends on: link-design-event.

**store-producers — Publishing keys from their owners**

- Format plant/device/panel/link state to wire-ready strings and upsert each loop
  (`kpa`/`vac`/`pct`/`duty`/`state`/`fault`/`sensor`/`target`/`ambient`/`floor`, battery,
  uptime, heap, chip, fw, proto, rotation, invert, w/h). Internal-but-queryable keys
  (`battery_present`, `battery_low`) included.
- Depends on: store-implement.

### Epic 3 — Dispatcher

**dispatcher-surface — Moving the command surface into the dispatcher**

- Move the resource/field/action tables, the readers, the writers, and the actions out of the
  serial link. Reads come from the store and domain modules; writes keep all-or-nothing validation
  and re-publish affected keys before returning the result.
- Depends on: link-design-command-result, store-implement.

**dispatcher-link-local — Resolving link-local settings**

- `term`, `log`, `attached` resolve against the `LinkContext` (the link that issued the command),
  not the global store.
- Depends on: dispatcher-surface, link-design-interface.

### Epic 4 — Events

**events-publish — Publishing domain events**

- VacuumSystem publishes `run_state`/`fault`/`vent_timeout`; link layer publishes
  `controller_attached`/`controller_lost`; core publishes `log`. Replaces the `info/warn/error`
  and `consumeLinkLost` one-shots.
- Depends on: link-design-event.

**overlay-store — Converting the overlay to read the store**

- `Overlay` reads `battery_present`/`battery_low`/`batt_pct`/`up` from the store instead of a
  `SystemSnapshot` reference.
- Depends on: store-implement.

**app-subscriptions — Subscribing App to core events**

- `App` subscribes to `controller_lost` (stop and vent) and any other events it owns, instead of
  calling `consumeLinkLost()`.
- Depends on: events-publish, link-design-interface.

### Epic 5 — Serial link layer

**serial-parse — Parsing proto 2 into Command**

- Keep the existing line grammar (`Tokens`, `splitValue`, `parseFlag`, `parseFloat`, `parseCoord`);
  produce a normalized `Command`.
- Depends on: link-design-interface, link-design-command-result.

**serial-render-result — Rendering Result byte-identically**

- Render `Result` as `ok key=value …` / `err code …`; `Items` becomes the renderer. Acceptance:
  byte-identical to today's proto 2 output.
- Depends on: serial-parse, dispatcher-surface.

**serial-render-event — Rendering pushed events (default on)**

- Render every `Event` as a change record by default; maintain the link's suppression list and a
  command to add or remove keys from it. Define framing, throttle, and reset on link loss.
- Depends on: serial-parse, link-design-event.

**serial-link-local — Keeping term/log/attached link-local**

- Move `term`/`log`/`attached` into `SerialLink` state, set and read through the dispatcher's
  link-local path.
- Depends on: dispatcher-link-local, serial-parse.

### Epic 6 — Docs and validation

**docs-update — Updating design docs and guidance**

- Update [README.md](../README.md) and the guidance in
  [.github/copilot-instructions.md](../.github/copilot-instructions.md) for the store + link-layer
  architecture. [serial-protocol.md](serial-protocol.md) gains the additive read-by-name and
  pushed-change-record notes (push by default, per-link suppression); every existing command and
  reply stays byte-identical proto 2.
- Depends on: serial-render-result, serial-render-event, dispatcher-surface.

**build-smoke — Building and smoke-testing**

- `powershell -File scripts/arduino.ps1 -Task build` is green.
- Smoke test over the monitor: `help`, `help plant`, `plant get`, `device get`, `panel get`,
  `link get`, a field read, `plant target=40`, `plant start`, `plant stop`, `plant reset`,
  `panel rotation=1 invert=on`, `device log=off` then `device log=on`; compare against the
  pre-change build for the byte-identical contract; verify a change record is pushed by default
  and that a suppressed key stops producing them.
- Depends on: docs-update.

## Execution order

1. `link-design-command-result`, `link-design-event` — always ready first.
2. `link-design-interface`, `store-implement` unblock.
3. `store-producers`, `dispatcher-surface`, `events-publish` unblock.
4. `dispatcher-link-local`, `overlay-store`, `app-subscriptions`, `serial-parse` unblock.
5. `serial-render-result`, `serial-render-event`, `serial-link-local` unblock.
6. `docs-update`, then `build-smoke`.

A suggested first implementation slice: `link-design-command-result` + `link-design-event` +
`link-design-interface` + `store-implement` + `store-producers`, ending with a build check that
producers upsert and the store enumerates and emits `key_changed`, before touching the dispatcher
or the serial render path.

## Risks and open questions

- **Push volume.** Push is on by default, so a link layer must throttle or batch change records,
  and the per-link suppression list is the controller's pressure valve for noisy keys like `kpa`.
- **Reply byte-identity.** Formatting (decimals, `on`/`off`, state names) now lives with the
  producers; the smoke test must compare captured output against the pre-change build.
- **Dispatcher neutrality.** The command surface must not smuggle serial-specific text into the
  dispatcher; `Result` items are `{key, value}` pairs, and each link layer owns its encoding.
- **Link-local scoping.** `term`/`log`/`attached` must resolve per issuing link, or one link's
  `device log=off` would silence another's diagnostics.
- **Event ordering and volume.** `key_changed` fires per changed key per loop; a link layer that
  subscribes to pressure keys needs a throttle or batching rule, or it will flood a slow transport.
- **Magic strings.** A typo in a key name fails at runtime, not compile time — accepted; the
  schema keeps each name's canonical spelling in one place.
- **Capacity.** The store and the event bus are fixed-capacity with defined overflow behaviour.
