# Interface task

Paths are relative to the repo root. The interface task owns the wire and the flash: `SerialLink`,
`BleLink` (loop side), `Ownership`, `Settings`, `Storage`, and the one shared `logEnabled` flag. It is
pinned to core 0 with the NimBLE host. Its loop is in [tasks.md](tasks.md).

## The log flag

`AppContext::logEnabled_` stops being a per-copy `bool&` inside the copied `AppContext` and becomes
one shared `volatile bool` (a `SharedLog` struct created in `buildAppContext()`), written only by
`device set log` here and read dirty by every task's own `StreamLogger`. `StreamLogger` itself is
unchanged; it keeps taking `const bool&`.

## Serial — the domain by domain change

`SerialLink::poll` keeps its current non-blocking `available()` loop and the `DomainList` dispatcher
verbatim. The domains change only in *what they touch*:

| Domain / verb | Today | Becomes |
|---|---|---|
| `control get` | reads `Ownership` | unchanged — direct read |
| `control acquire` | `ownership.acquire(Serial)` | unchanged — `Ownership` is interface-owned |
| `control release` | `ownership.release(Serial)` then `vacuum.stop()` | release direct; `vacuum.stop()` → enqueue `MachineCommand::Stop` |
| `device get` | reads battery, settings, display, log | unchanged reads — battery is a dirty read, settings/log are direct, `display().width()/height()` are constants |
| `device set log` | writes the flag | unchanged — writes the shared `volatile bool` |
| `device set unit` | `settings.setUnit()` | unchanged — `Settings` is interface-owned |
| `device reboot` | `safeReboot()` inline | reply `ok\n` first, then enqueue `MachineCommand::Reboot` |
| `vacuum get` | reads `VacuumSystem` | unchanged reads, now dirty |
| `vacuum target` | `setTarget` / `clearTarget` | enqueue `SetTarget` / `ClearTarget`; reply built from the parsed argument |
| `vacuum start` | `start()` + `startRefusal()` | refusal from a dirty read (`startRefusal()` is pure); enqueue `Start` only if it would succeed; optimistic reply |
| `vacuum stop` | `stop()` | enqueue `Stop`; reply `ok state=VENT` optimistically |
| `vacuum reset` | `resetFault()` | enqueue `ResetFault`; reply `ok state=IDLE` optimistically |
| `buzzer get` | reads `BuzzerPlayer` | unchanged read, now dirty |
| `buzzer tone/play/off` | `tone` / `play` / `off` | enqueue `Tone` / `Play` / `Off` |
| `fs *` | `Storage` | unchanged — `Storage` is interface-owned, LittleFS stays on this task |

`buzzer play`'s reply needs the bad-token answer. The parse is the pure static
`BuzzerPlayer::parse(text, notes, &badToken)`, so the domain calls it here for the reply and enqueues
the raw `text`; the control task parses again inside `play()` (a few notes, negligible).

`vacuum start`'s optimistic reply is the one deliberate divergence and is documented in
[queues.md](queues.md).

## BLE — the loop side

`BleLink::poll` still runs `applyPending()` then `publish(now)` then the disconnect handling, and the
`volatile` callback slots (`control_`, `target_`, `unit_`, `command_`, `linkLost_`, `subscriptions_`,
`baseline_`) are untouched — callbacks still land on the NimBLE host task, the interface task applies
them. The change is inside the apply/publish half:

- `control_` write → `ownership.acquire/release(Bluetooth)` direct; a release enqueues
  `MachineCommand::Stop`.
- `target_` write → enqueue `SetTarget` / `ClearTarget` (0 clears); the read-back for the notification
  is a dirty `vacuum.targetKpa()`.
- `unit_` write → `settings.setUnit()` direct.
- `command_` opcode → `execute()` now computes the result byte from a dirty read for `start`, and
  enqueues `Start` / `Stop` / `ResetFault` / `Reboot`; the result byte is written to the Command
  characteristic as today. `Reboot` no longer calls `safeReboot` inline — it enqueues.
- `publish` / `describe` → read the live `VacuumSystem`, `Settings`, `Ownership` directly (dirty),
  as they do today, only now from this task.

`onDisconnect` and the `owned()`/`subscribed()` helpers are unchanged.

## Storage is single-owner by rule, not by lock

`Storage` (LittleFS) has exactly one owner: this task. The serial `fs` domain runs here. The panel
task's Berry `store.*` and `device.*` modules do not touch LittleFS at all — they arrive as
`StoreRequest` items and are applied by `StoreServer::drain()`. See [queues.md](queues.md) for the
`StoreClient`/`StoreServer` abstraction and [panel-vm-task.md](panel-vm-task.md) for the binding
changes.
