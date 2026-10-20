# FreeRTOS conversion — backlog

The implementation backlog for the FreeRTOS conversion. Each item is one individually completable
change. Order within an epic is the build order; epics run in the order listed, and the "Wire-up"
epic is the point where the old loop comes out.

The plan this backs is [\_index.md](_index.md), with the detail in [tasks.md](tasks.md),
[queues.md](queues.md), [control-task.md](control-task.md), [interface-task.md](interface-task.md)
and [panel-vm-task.md](panel-vm-task.md). The rule that governs every item: **one writer per piece
of state, reads direct and dirty with no lock** — a queue exists only to move a write to its owner.

Legend: `[ ]` not started · `[x]` done · `[~]` in progress. Items are not time-estimated.

---

## FND — Foundation

- [x] FND-001 Create `app/Tasks.h`: declare the `MachineCommand` and `ButtonEvent` structs, the
  three queue handles, the shared scalars `okHeldMs` and `stopSeq`, the three task entry points,
  and the `spawnTasks(AppContext&)` declaration.
- [x] FND-002 Create `app/Tasks.cpp`: define the three queue handles and the two shared
  `volatile` scalars as globals.
- [x] FND-003 Define `MachineCommand` with the `Op` enum `{Start, Stop, ResetFault, SetTarget,
  ClearTarget, Reboot, Tone, Play, Off}` and the payload fields `value`, `durationMs`, `text`
  ([queues.md](queues.md)).
- [x] FND-004 Define `ButtonEvent` with the `Button` and `Press` fields.
- [x] FND-005 Declare the shared `volatile uint32_t okHeldMs` (control writes, panel reads).
- [x] FND-006 Declare the shared `volatile uint32_t stopSeq` (control increments on Back hold,
  panel's `takeStopped()` compares).
- [x] FND-007 Add the three task entry-point declarations: `controlTask`, `interfaceTask`,
  `panelTask` (all `void(void*)`).
- [x] FND-008 Create the machine-command queue with `xQueueCreate`, depth 8, item size
  `sizeof(MachineCommand)`.
- [x] FND-009 Create the store queue with `xQueueCreate`, depth 4, item size `sizeof(StoreRequest)`
  (created alongside the store bus in STORE, not here — this item is the handle + size).
- [x] FND-010 Create the button-event queue with `xQueueCreate`, depth 8, item size
  `sizeof(ButtonEvent)`.
- [x] FND-011 Add a per-task state struct (e.g. `TaskState` in `app/Tasks.h`) holding the pointers
  each task owns — the context, the link/UI/logger objects and the store bus ends — so the three
  bodies receive what they own without sharing what they must not.
- [~] FND-012 Move the `SerialLink`, `BleLink`, `StreamLogger`, `ConnectedUI` and
  `DisconnectedUI` construction out of the `App` constructor into the task spawner (or
  `buildAppContext()`), so `App::begin()` and the task bodies reach the same objects.
- [x] FND-013 Implement `spawnTasks(AppContext&)` to create the three tasks with
  `xTaskCreatePinnedToCore` using the cores, priorities and stack sizes from
  [tasks.md](tasks.md) (control: core 1 / prio 2 / 4096; interface: core 0 / prio 1 / 4096;
  panel: core 0 / prio 0 / 8192).
- [x] FND-014 Call `spawnTasks(...)` from `setup()` in `firmware/gardener/gardener.ino` after
  `app.begin()`.
- [~] FND-015 Make `loop()` in `gardener.ino` `vTaskDelete(NULL)` (free the Arduino loop task).
- [x] FND-016 Keep the old `App::update()` in place through this epic so the build stays green
  while the foundation lands; delete it only in WIRE.
- [x] FND-017 Verify: `python scripts/arduino.py --task build` still compiles with the
  old loop and the new scaffolding coexisting.

## LOG — Logger abstraction

- [x] LOG-001 Add `app/Logger.h`: the `Logger` interface — pure-virtual `info`, `warn` and
  `error` over `const char*`, plus a virtual destructor.
- [x] LOG-002 Add `app/NilLogger.h`: a `Logger` whose three methods do nothing.
- [x] LOG-003 Make `StreamLogger` derive from `Logger`; change its constructor to
  `StreamLogger(Stream&)` and delete the `enabled_` flag and the gate in `write()`.
- [x] LOG-004 Replace `StreamLogger` with `Logger` across the app: `App::logger_` becomes
  `Logger*`, `Routines::begin`/`reportError` and `berry::install`/`gLogger` take
  `Logger&`/`Logger*`, and `DisconnectedUI` takes and holds `Logger&`. Headers forward-declare
  `Logger`; only the construction site includes the concrete headers.
- [x] LOG-005 Replace `SerialLink::diag`'s inline `[I]`/`[W]` writes with a `Logger&` handed to
  the link; delete the `context_.logEnabled()` check.
- [x] LOG-006 Remove `logEnabled_` and `logEnabled()` from `AppContext`.
- [x] LOG-007 Add `logEnabled()` / `setLogEnabled()` to `Settings`, persisted under `[device]`
  in `/settings.ini`, default on — the same pattern as `bluetooth`.
- [x] LOG-008 Choose the logger once at boot, in `App::begin()` after `settings.begin()`:
  `settings.logEnabled() ? (Logger*)new StreamLogger(Serial) : (Logger*)new NilLogger()`, and
  hand that one instance to the link, the UI and the routines.
- [x] LOG-009 Wire: `device get` reports `log=` from `settings.logEnabled()`; `device set log`
  calls `settings.setLogEnabled(...)` and replies `ok log=<on|off>`; a change takes effect on the
  next reboot (no live toggle).
- [~] LOG-010 Give the control and panel tasks a `Logger&` to the same boot-chosen instance for
  run-transition, hold and VM logging.
- [x] LOG-011 Update [docs/specs/serial-protocol.md](../specs/serial-protocol.md): `log` is a
  persisted boot preference, and `set log` applies at the next reset.
- [x] LOG-012 Verify: `python scripts/arduino.py --task build` compiles, and no signature or
  member outside the construction site names `StreamLogger` / `NilLogger`.

## CTRL — Control task

- [x] CTRL-001 Implement `controlTask(void*)`: the `for (;;)` body with
  `vTaskDelayUntil` on `cfg::CONTROL_PERIOD_MS` ([tasks.md](tasks.md)).
- [x] CTRL-002 Move `sensor().update(now)` from `App::update()` into the control loop.
- [x] CTRL-003 Move `pump().update(now)` into the control loop.
- [x] CTRL-004 Move `battery().update(now)` into the control loop.
- [x] CTRL-005 Move `vacuum().service(now)` into the control loop (the PID now ticks on a fixed
  50 ms beat).
- [x] CTRL-006 Move `buzzer().update(now)` into the control loop.
- [x] CTRL-007 Implement `drainMachineCommands()`: pop every pending `MachineCommand` and apply it.
- [x] CTRL-008 Apply `MachineCommand::Start` → `vacuum.start()`.
- [x] CTRL-009 Apply `MachineCommand::Stop` → `vacuum.stop()`.
- [x] CTRL-010 Apply `MachineCommand::ResetFault` → `vacuum.resetFault()`.
- [x] CTRL-011 Apply `MachineCommand::SetTarget` → `vacuum.setTarget(value)`.
- [x] CTRL-012 Apply `MachineCommand::ClearTarget` → `vacuum.clearTarget()`.
- [x] CTRL-013 Apply `MachineCommand::Reboot` → `safeReboot(context_)` (the vent-wait loop runs
  here, not on the interface task).
- [x] CTRL-014 Apply `MachineCommand::Tone` → `buzzer.tone(hz, ms)`.
- [x] CTRL-015 Apply `MachineCommand::Play` → `buzzer.play(text.c_str())`.
- [x] CTRL-016 Apply `MachineCommand::Off` → `buzzer.off()`.
- [x] CTRL-017 Implement `buttonsTick(now)`: `consumePress(Button)` for all four keys, pushing a
  `ButtonEvent` per completed press (overflow logged, matching today's `pressOverflow_`).
- [x] CTRL-018 Implement the `okHeldMs` publish: write `heldMs(Button::Ok)` into the shared
  `volatile` scalar each tick.
- [x] CTRL-019 Move `watchBackHold` into the control loop: Back 3 s and not yet fired →
  `vacuum.stop()`, then `stopSeq++` (replacing the old `routines().notifyStopped()` call).
- [x] CTRL-020 Keep the `backStopFired_` latch in the control task, cleared when Back comes up.
- [x] CTRL-021 Move `watchResetHold` into the control loop: Back+OK 5 s → `safeReboot(context_)`.
- [x] CTRL-022 Move `logRunChanges` into the control loop, using the control task's logger.
- [x] CTRL-023 Remove the `sensor/pump/battery/vacuum/buzzer`, hold-watch and run-log calls from
  `App::update()` so they run once, on the control task.

## SER — Serial domains

- [x] SER-001 Confirm `SerialLink::poll` keeps its non-blocking `available()` loop and the
  `DomainList` dispatcher verbatim (no change expected).
- [x] SER-002 `control release`: keep `ownership.release(Serial)` direct; replace the inline
  `vacuum.stop()` with an enqueued `MachineCommand::Stop`.
- [x] SER-003 `device reboot`: write the `ok\n` reply first, then enqueue `MachineCommand::Reboot`
  instead of calling `safeReboot()` inline.
- [x] SER-004 `vacuum target`: enqueue `SetTarget` / `ClearTarget`; build the reply from the parsed
  argument (refusals for range still answered from the parse, not the machine).
- [x] SER-005 `vacuum start`: compute the refusal from a dirty `startRefusal()` read; enqueue
  `Start` only when it would succeed; reply optimistically.
- [x] SER-006 `vacuum stop`: enqueue `Stop`; reply `ok state=VENT` optimistically.
- [x] SER-007 `vacuum reset`: enqueue `ResetFault`; reply `ok state=IDLE` optimistically.
- [x] SER-008 `buzzer tone`: enqueue `Tone` (range check stays in the domain).
- [x] SER-009 `buzzer play`: call the pure static `BuzzerPlayer::parse(text, notes, &badToken)`
  for the reply; enqueue `Play` with the raw text (control re-parses inside `play()`).
- [x] SER-010 `buzzer off`: enqueue `Off`.
- [x] SER-011 Confirm `control get`, `device get`, `vacuum get`, `buzzer get` and `fs *` are
  unchanged — reads are now dirty but the grammar and field order are identical.

## BLE — BLE link loop side

- [x] BLE-001 Confirm the `volatile` callback slots (`control_`, `target_`, `unit_`, `command_`,
  `linkLost_`, `subscriptions_`, `baseline_`) and `onDisconnect` are untouched.
- [x] BLE-002 `control_` write: keep `ownership.acquire/release(Bluetooth)` direct; a release
  enqueues `MachineCommand::Stop` instead of the inline `vacuum.stop()`.
- [x] BLE-003 `target_` write: enqueue `SetTarget` / `ClearTarget` (0 clears); the read-back for
  the notify is a dirty `vacuum.targetKpa()`.
- [x] BLE-004 `unit_` write: keep `settings.setUnit()` direct (Settings is interface-owned).
- [x] BLE-005 `execute(Start)`: compute the result byte from a dirty `startRefusal()` read; enqueue
  `Start`; write the result byte to the Command characteristic.
- [x] BLE-006 `execute(Stop)`: enqueue `Stop`; write the result byte.
- [x] BLE-007 `execute(ResetFault)`: enqueue `ResetFault`; write the result byte.
- [x] BLE-008 `execute(Reboot)`: enqueue `Reboot`; no inline `safeReboot()`.
- [x] BLE-009 Confirm `publish` / `describe` read the live `VacuumSystem`, `Settings`, `Ownership`
  directly (they already do; now from the interface task).

## STORE — Store bus

- [x] STORE-001 Create `app/StoreBus.h`: `StoreResult`, `StoreRequest` (ops `Get`, `Set`,
  `SetUnit`, `SetTarget`, `SetBluetooth`), `StoreClient`, `StoreServer`
  ([queues.md](queues.md)).
- [x] STORE-002 Implement `StoreClient::get`: fill a `StoreRequest`, post it, block on
  `xTaskNotifyWait`, return `result.ok` and copy `result.value` into the out param.
- [x] STORE-003 Implement `StoreClient::set`: same shape as `get` with `Op::Set`, returning
  `result.ok`.
- [x] STORE-004 Implement the one-way `StoreClient::setUnit` / `setTarget` / `setBluetooth` (null
  result and waiter, post without waiting).
- [x] STORE-005 Implement `StoreServer::drain()`: `xQueueReceive` in a loop, `handle` each item.
- [x] STORE-006 Implement `handle(Get)`: build `/routines/<name>.ini`, read, parse `Ini`, fill the
  result, `xTaskNotifyGive(waiter)`.
- [x] STORE-007 Implement `handle(Set)`: read/parse, skip the repeat-value write, write, fill the
  result, notify.
- [x] STORE-008 Implement `handle(SetUnit/SetTarget/SetBluetooth)`: call the matching
  `settings_.setX(...)` and return without answering.
- [x] STORE-009 Construct the store queue and the `StoreClient`/`StoreServer` pair in
  `buildAppContext()` (or the task spawner), handing the server to the interface task and the
  client to the panel task.

## PANEL — Panel / VM task

- [x] PANEL-001 Implement `panelTask(void*)`: the `for (;;)` body with a 10 ms `vTaskDelay`
  ([tasks.md](tasks.md)).
- [x] PANEL-002 Implement `drainButtonEvents()`: route each press to the VM (`pushPress`) when a
  routine is active, else log it, matching today's `logButtonPresses()`.
- [x] PANEL-003 Move the page selection (`screenFor(owned)`) into the panel task, reading
  `ownership.held()` dirty.
- [x] PANEL-004 Move `ConnectedUI`/`DisconnectedUI` update into the panel task, redrawing the
  static page at `cfg::UI_REFRESH_MS`.
- [x] PANEL-005 Move `watchDefaultHold` into the panel task: read the `okHeldMs` scalar (not
  `heldMs` on the shared `Buttons`); on fire enqueue `MachineCommand::Stop` and call `showDefault()`.
- [x] PANEL-006 Confirm `Overlay` is unchanged except that it runs here and reads `battery`/
  `vacuum` dirty; the display is private to this task.
- [x] PANEL-007 Remove the `ownedShown_`/`ui_` swap and the `setConnections` call from
  `App::update()` (now the panel's business).

## ROUT — Routines

- [x] ROUT-001 Keep `Routines::begin()` taking `Storage&` — it still runs in `setup()` before the
  tasks exist, so the module/routine load stays single-threaded.
- [x] ROUT-002 Change `Routines::storeGet(key, out)` to resolve the active routine from the
  registry and delegate to `StoreClient::get` (drops `const`; `active_ < 0` still returns false
  without touching the bus).
- [x] ROUT-003 Change `Routines::storeGet(routine, key, out)` to delegate to `StoreClient::get`
  with the named routine (the `registered` check stays here).
- [x] ROUT-004 Change `Routines::storeSet` (both forms) to delegate to `StoreClient::set`.
- [x] ROUT-005 Reimplement `Routines::takeStopped()` over the `stopSeq` counter: true once per
  increment; delete the `bool stopped_` field.
- [x] ROUT-006 Keep the press ring (`presses_`, `takePress`, `pushPress`) but fill it from the
  `ButtonEvent` queue instead of the panel reading `Buttons` directly.
- [x] ROUT-007 Give `Routines` a `StoreClient&` (constructed in STORE-009); remove its direct
  `Storage` dependence for runtime `store.*`.

## BIND — Berry bindings

- [x] BIND-001 `vacuum.target` / `target_off`: enqueue `SetTarget` / `ClearTarget` instead of
  `setTarget` / `clearTarget`.
- [x] BIND-002 `vacuum.start` / `stop` / `reset`: enqueue `Start` / `Stop` / `ResetFault`.
- [x] BIND-003 `vacuum.state()`: unchanged — dirty read of `VacuumSystem` and `Battery`.
- [x] BIND-004 `vacuum.clamp` / `now_ms`: unchanged (pure / `millis()`).
- [x] BIND-005 `vacuum.log(text)`: unchanged — the panel's logger, written directly.
- [x] BIND-006 `vacuum.stopped()`: `takeStopped()` over `stopSeq` (ROUT-005).
- [x] BIND-007 `device.unit` / `target` / `bluetooth`: unchanged — dirty read of `Settings`.
- [x] BIND-008 `device.set_unit` / `set_target` / `set_ble`: `StoreClient::setUnit` /
  `setTarget` / `setBluetooth` (one-way).
- [x] BIND-009 `device.units`: unchanged (constant).
- [x] BIND-010 `store.get`: `StoreClient::get` round-trip (blocks).
- [x] BIND-011 `store.set`: `StoreClient::set` round-trip (blocks); the file's bool is the answer.
- [x] BIND-012 `screen.*` draw verbs: unchanged — the display is panel-owned.
- [x] BIND-013 `screen.unit()`: unchanged — dirty read of `Settings`.
- [x] BIND-014 `buttons.take()`: unchanged — the ring, now filled from the button-event queue.
- [x] BIND-015 `buzzer.tone` / `off`: enqueue `Tone` / `Off`.
- [x] BIND-016 `buzzer.play`: enqueue `Play`; the bool return comes from the pure
  `BuzzerPlayer::parse` here.
- [x] BIND-017 `routines.*`: unchanged — panel-local registry methods.
- [x] BIND-018 Add the `StoreClient` (as a new `gStore` global) to `berry::install`'s parameters.

## WIRE — Wire-up and delete the old loop

- [ ] WIRE-001 Delete `App::update()`.
- [ ] WIRE-002 Shrink `App` to `begin()` plus the task-spawning glue; remove the fields the tasks
  now own (`link_`, `ble_`, `logger_`, `connectedUi_`, `disconnectedUi_`, `ui_`, `ownedShown_`,
  `backStopFired_`, `lastState_`, `ventStartMs_`).
- [ ] WIRE-003 Update `App::begin()` to use the shared link/UI/logger objects from FND-012 and the
  boot-chosen logger from LOG-008, with the boot order unchanged.
- [ ] WIRE-004 Confirm `gardener.ino` spawns the tasks after `app.begin()` and `loop()` is empty.
- [ ] WIRE-005 Verify the three task bodies are the only places that call the devices, the links,
  the store bus and the UI after this epic.

## DOCS — Documentation

- [ ] DOCS-001 Update [docs/specs/routines.md](../specs/routines.md): supersede the "nothing is
  suspended and there is no second core, which is why there are no locks and no callbacks"
  rationale — the VM now runs on the second core, writes the machine only through the command
  queue, and reads dirty; `poll` blocks the panel loop, not the control loop.
- [ ] DOCS-002 Update [README.md](../../README.md) where it claims "lives in the main loop, so
  nothing blocks" and any control-model prose that names the single loop, to describe the three
  tasks.
- [ ] DOCS-003 Delete this plan set (`docs/plans/free-rtos/`) once the conversion is in — the
  spec/README carry what the change established, and nothing may reference a plan.

## VER — Verification

- [ ] VER-001 `python scripts/arduino.py --task build` clean at the end of every epic.
- [ ] VER-002 Grep every `context_.<thing>().` mutation and confirm each runs on the owning task
  (no cross-task writer).
- [ ] VER-003 Review the layer rule: no `hal/` → `app/` include introduced by the new files.
- [ ] VER-004 Confirm LittleFS (`Storage`) and the `Wire1`/SH1106 device are reached from exactly
  one task each (interface and panel respectively).
- [ ] VER-005 Confirm no runtime log flag remains: every consumer holds a `Logger`, and the only
  writer of the persisted `logEnabled` setting is the interface task (via `device set log`).
- [ ] VER-006 Flash the sketch and confirm the serial contract over USB CDC, then a BLE
  read/write, then a routine exercising `store.*` and the `vacuum.*` bindings.
