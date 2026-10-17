# Key-value store backlog

Status: **proposed — not started.** This document is the plan; no code has changed.

## Goal

Replace the `SystemSnapshot` struct with a general key-value data structure that is the single
source of truth for published device state, and re-point the serial link's read path at it.

- **Consumers** can enumerate every key-value pair, get a value by key, and register for change
  notifications.
- **Producers** can upsert key-value pairs.

## Locked-in decisions

These came out of the ideation pass and are the contract for the tasks below:

1. **Strings, not types.** Keys are strings, values are strings, and the set is open at runtime —
   no `Key` enum, no `Value` union, no fixed enumeration. The store is a fixed-capacity map of
   `String` key to `String` value. Magic strings are accepted; the key *name* is the contract.
   The protocol's `-` ("unset / not applicable") is stored literally as the string `"-"`, so
   absence needs no separate flag.
2. **The store is authoritative.** Published state has one copy, in the store. After an action or
   write mutates real state, the affected keys are re-published *before* the reply is rendered, so
   `plant start` still answers `state=PULL` — the freshness invariant today guaranteed by
   `WireView` copying `system_.state()` live.
3. **In-process notifications only.** The local subscribe API exists because it is a stated
   requirement. The serial link does not use it and does not push.
4. **Existing replies stay byte-identical proto 2.** No subscription commands, no change counter,
   and no change to any command or reply a controller uses today. The only additive surface is
   decision 5. `serial-protocol.md` is not rewritten; the existing surface is produced byte-for-byte
   identically by the new rendering path.
5. **All keys are wire-visible by default.** Every store key can be queried by name, even when it is
   not worth a `help` row. Being left out of `help` never makes a key unqueryable.
   `battery_present` and `battery_low` are the first such keys: queryable, absent from `help`.

### The wire keeps its own metadata

`help` must still list fields with types, bounds, units and choices, and writes must still be
validated against bounds. So the serial layer keeps a **string-keyed schema table** — name to
`type`/`unit`/`choices`/`min`/`max`/`access` — for help and write validation. It is looked up by
string, so adding a key never means touching an enum.

The schema is a curated subset, not the visibility boundary. Every store key is wire-visible by
default and queryable by name; the schema decides which keys get a `help` row and how their writes
are validated. `battery_present` and `battery_low` are store keys with no schema row: queryable,
and absent from `help`.

### Consequence for `PlantState`

`VacuumSystem::state()` keeps returning a `PlantState` value — it is the module's own report and a
plain read, not the shared structure. `PlantState` stops being embedded in a cross-layer snapshot
and becomes the producer-side input that the plant publisher formats and upserts field by field.

## Current architecture

- [`core/Snapshot.h`](../libraries/VacuumCore/src/core/Snapshot.h) defines `PlantState` and
  `SystemSnapshot`. `SystemSnapshot` wraps `PlantState` plus battery and uptime fields.
- [`app/App.cpp`](../libraries/VacuumCore/src/app/App.cpp) — `refreshSnapshot()` rebuilds the one
  snapshot each loop iteration from `system_.state()`, `battery_.*()`, and `millis()`.
- [`app/Overlay.h`](../libraries/VacuumCore/src/app/Overlay.h) — holds `const SystemSnapshot&` to
  composite the battery icon on each flush.
- [`app/serial/SerialSurface.h`](../libraries/VacuumCore/src/app/serial/SerialSurface.h) — defines
  `WireView` and the `FieldSpec`/`ResourceSpec`/`ActionSpec` tables. Every wire field already has
  metadata (name, type, unit, choices, min/max) and a per-field `read` function.
- [`app/serial/SerialPayload.cpp`](../libraries/VacuumCore/src/app/serial/SerialPayload.cpp) — one
  hand-written `read*` function per field, plus the `resources()` table.
- [`app/serial/SerialLink.cpp`](../libraries/VacuumCore/src/app/serial/SerialLink.cpp) — command
  dispatch, `wireView()`, the writers (`writeDevice`/`writePlant`/`writePanel`/`writeLink`), and
  the actions.
- [`app/serial/LineGrammar.h`](../libraries/VacuumCore/src/app/serial/LineGrammar.h) — `Tokens`,
  `parseFlag`, `parseFloat`, `parseCoord`, `splitValue`. These stay as-is; the write path keeps
  using them.

The duplication this change removes: the wire already has a key-value surface described by tables,
and `SystemSnapshot` is a second, parallel, typed view of the same data. Collapsing them is the
"same behaviour, less surface" win.

## Target architecture

```
String-key store                    wire schema (serial layer)
String key -> String value          name -> type/unit/choices/min/max/access
+ per-key revision                   (curated help + write validation)
+ local subscribers                         |
        ^                                  | help + write validation
        | upsert("kpa", "61.26")           v
        |                          serial: get / help / write
   producers:                      (get enumerates the store verbatim;
   App, VacuumSystem,               any key is queryable by name)
   Battery, link settings
```

- The store is a fixed-capacity list of `{ String key; String value; }` with a revision per key.
- `upsert(key, value)` inserts or replaces; it bumps the key's revision only when the value string
  actually changes.
- `get(key)` and `enumerate()` are the consumer read path.
- `subscribe(key, callback)` / `unsubscribe(key, callback)` is the bounded, in-process change
  notification.
- Values are the exact wire-ready string; producers own formatting (decimals, `on`/`off`, state
  names, `-`). The serial layer does not parse values to render them.

## Backlog

Dependencies are listed per task. "Ready" means all dependencies are done.

### Epic 1 — Store core

**kv-define-key-model — Defining the string key-value model**

- Define the store entry as `String` key and `String` value; agree the literal `"-"` is the unset
  value (no separate present flag).
- Define key-name conventions and the documented-vs-undocumented distinction: keys with a
  `help` row vs keys like `battery_present` and `battery_low`, which are queryable but not listed.
- No enum, no union: magic strings accepted.
- Depends on: nothing.

**kv-implement-store — Implementing the string key-value store**

- Fixed-capacity list of `{ String key; String value; }` with per-key revision; `upsert` inserts or
  replaces; `get` by key; `enumerate` all entries.
- Revision bumps only on real value change. Define overflow behaviour once capacity is reached.
- Depends on: kv-define-key-model.

**kv-subscribe-api — Adding local change notifications**

- Bounded subscriber list (`String` key + callback); notify only on actual value change.
- `subscribe` / `unsubscribe`.
- Depends on: kv-implement-store.

### Epic 2 — Producers publish

**kv-unified-key-table — Building the wire schema table**

- String-keyed schema for the serial layer: name to `type`/`unit`/`choices`/`min`/`max`/`access`,
  used by `help` and write validation.
- Looked up by string; the schema is a curated subset — keys without a row are still queryable.
- Depends on: kv-define-key-model.

**kv-publish-plant — Publishing plant keys**

- Format plant fields to wire-ready strings and upsert `kpa`/`vac`/`pct`/`duty`/`state`/`fault`/
  `sensor`/`target`/`ambient`/`floor` from `VacuumSystem::state()` each loop.
- Re-publish synchronously after every plant action so replies stay truthful.
- Depends on: kv-implement-store, kv-unified-key-table.

**kv-publish-device-panel-link — Publishing device, panel and link keys**

- Upsert battery (`batt_v`/`batt_pct`/`batt` plus the undocumented `battery_present`/
  `battery_low`), uptime, heap, chip, fw, proto, rotation, invert, w/h, term, log, attached as
  strings from their owning modules or link state.
- Depends on: kv-implement-store, kv-unified-key-table.

**kv-overlay-reads-store — Converting the overlay to read the store**

- `Overlay` reads `battery_present`/`battery_low`/`batt_pct`/`up` strings from the store instead of
  holding a `SystemSnapshot` reference.
- Depends on: kv-implement-store.

**kv-remove-snapshot — Removing SystemSnapshot**

- Delete the `SystemSnapshot` struct and `App::refreshSnapshot`; remove `PlantState` duplication if
  superseded. Nothing may reference the snapshot once this lands.
- Depends on: kv-publish-plant, kv-publish-device-panel-link, kv-overlay-reads-store.

### Epic 3 — Consumers and wire

**kv-drop-wireview — Removing WireView**

- Delete `WireView`; field readers are replaced by verbatim store enumeration plus the
  string-keyed wire schema for help and validation.
- Depends on: kv-unified-key-table, kv-publish-plant, kv-publish-device-panel-link.

**kv-generic-get — Generic get and single-field read**

- `resource get` and single-field reads enumerate the store's entries for that resource and emit
  `key=value` verbatim; any store key is queryable by name even when it has no `help` row.
- Acceptance: every reply is byte-identical to today's proto 2 output (same key order, same
  decimals, same `-` handling).
- Depends on: kv-drop-wireview.

**kv-generic-help — Deriving help pages from the wire schema**

- `help` resource/field rows come from the string-keyed schema (fields, bounds, units, choices) —
  a curated subset, since keys without a schema row are queryable but not listed;
  the actions table and `help <resource> <action>` (draw ops) stay as they are.
- Depends on: kv-unified-key-table.

**kv-preserve-writes — Preserving write semantics**

- Wire writes stay all-or-nothing, validated against the wire schema (bounds/choices) with side
  effects, still using [`LineGrammar.h`](../libraries/VacuumCore/src/app/serial/LineGrammar.h)
  helpers.
- After a write or action, re-publish affected keys before echoing, so `plant start` still answers
  `state=PULL` and a write's echo still matches a read.
- Depends on: kv-generic-get.

### Epic 4 — Docs and validation

**kv-update-docs — Updating design docs and guidance**

- Update [README.md](../README.md) and the "a value beats a mechanism" guidance in
  [.github/copilot-instructions.md](../.github/copilot-instructions.md) for the store-based state
  architecture.
- [serial-protocol.md](serial-protocol.md) gains only the additive read-by-name note; every
  existing command and reply stays byte-identical proto 2.
- Depends on: kv-generic-get, kv-remove-snapshot.

**kv-build-smoke — Building and smoke-testing**

- `powershell -File scripts/arduino.ps1 -Task build` is green.
- Smoke test over the monitor: `help`, `help plant`, `plant get`, `device get`, `panel get`,
  `link get`, a field read, `plant target=40`, `plant start`, `plant stop`, `plant reset`,
  `panel rotation=1 invert=on`, and `device log=off` then `device log=on`.
- Compare output against the pre-change build for the byte-identical contract.
- Depends on: kv-update-docs.

## Execution order

The ready batch is the set of tasks with no unfinished dependencies:

1. `kv-define-key-model` — always ready first.
2. Once it lands: `kv-implement-store`, `kv-unified-key-table` become ready.
3. Then `kv-subscribe-api`, `kv-publish-plant`, `kv-publish-device-panel-link`,
   `kv-overlay-reads-store` unblock.
4. `kv-remove-snapshot`, `kv-drop-wireview`, `kv-generic-help` unblock next.
5. `kv-generic-get`, then `kv-preserve-writes`.
6. `kv-update-docs`, then `kv-build-smoke`.

A suggested first implementation slice: `kv-define-key-model` → `kv-implement-store` →
`kv-unified-key-table` → `kv-publish-plant` + `kv-publish-device-panel-link`, ending with a build
check that producers upsert and the store enumerates, before touching the serial render path.

## Risks and open questions

- **Reply byte-identity** is the sharpest acceptance criterion. Formatting (decimals, `on`/`off`,
  state names) now lives with the producers, so it is scattered by design; the smoke test must
  compare captured output against the pre-change build. A small shared formatting helper can keep
  the decimal counts in one place if drift becomes a problem. Byte-identity applies to every
  existing command; the only new surface is query-by-name for keys without a `help` row.
- **Magic strings.** A typo in a key name fails at runtime, not compile time — accepted. Keeping
  each key name's canonical spelling in one place (the wire schema) reduces drift, and the smoke
  test exercises every name.
- **Capacity.** An open, string-keyed set needs a bound. The store is fixed-capacity with a defined
  overflow policy, so a runaway producer cannot grow the table without limit or fragment the heap.
- **Notification users.** With the wire excluded, the subscribe API currently has no production
  caller. It is delivered because it is a stated requirement, not wired into the link; the first
  module that genuinely needs event-driven behaviour should adopt it rather than a new mechanism.
