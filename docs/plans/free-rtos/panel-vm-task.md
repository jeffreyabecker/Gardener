# Panel / VM task

Paths are relative to the repo root. The panel task owns the glass and the interpreter: `Display`
(Adafruit + `Overlay`), `ConnectedUI`, `DisconnectedUI`, and `Routines` (the Berry VM). It is pinned
to core 0 at the lowest priority, so a slow routine can stall the panel but never the machine or the
wire. The display is private to this task — nothing else draws.

This is the file with the most rework, because the VM currently mutates `VacuumSystem` and `Settings`
directly ([Bindings.cpp](libraries/VacuumCore/src/app/routines/Bindings.cpp)). Those calls
become queue sends; reads stay dirty; drawing stays put.

## Page selection and drawing

`screenFor(owned)` moves here, reading `ownership.held()` dirty:

- held → `ConnectedUI`, which draws the serial/Bluetooth icon from `ownership.owner()` (dirty read).
- not held, routine active → the routine draws through the `screen.*` bindings.
- not held, no routine → `DisconnectedUI` draws the mark.

The static pages redraw on `cfg::UI_REFRESH_MS`; the routine polls on the 10 ms beat. `Overlay` is
unchanged except that it now runs on this task and reads `battery`/`vacuum` dirty, exactly as it does
today.

`DisconnectedUI::watchDefaultHold` moves here and reads the `okHeldMs` scalar the control task
publishes instead of calling `heldMs(Button::Ok)` on the shared `Buttons`; on fire it enqueues
`MachineCommand::Stop` and calls `showDefault()`.

## Routines signature

- `begin()` keeps its `Storage&` parameter: it runs in `setup()`, before the tasks exist, so loading
  `/modules/*.be` and `/routines/*.be` at boot is still single-threaded and safe.
- The runtime `storeGet`/`storeSet` no longer touch `Storage`: they resolve the active routine and
  delegate to the `StoreClient` (see [queues.md](queues.md)). `storeGet` drops its `const` because the
  round-trip uses the client's result slot.
- `takeStopped()` is reimplemented over the control-owned `stopSeq` counter: answer true once per
  increment, comparing against the panel's own last-seen value. The `bool stopped_` field goes away.
- The `Logger&` stays (the boot-chosen instance, writing `Serial` directly).

## Bindings — module by module

| Module / call | Today | Becomes |
|---|---|---|
| `vacuum.target` / `target_off` | `setTarget` / `clearTarget` | enqueue `SetTarget` / `ClearTarget` |
| `vacuum.start` / `stop` / `reset` | direct | enqueue `Start` / `Stop` / `ResetFault` |
| `vacuum.state()` | reads `VacuumSystem` + `Battery` | unchanged — dirty read |
| `vacuum.clamp` / `now_ms` | pure / `millis()` | unchanged |
| `vacuum.log(text)` | `gLogger->info` | unchanged — the panel's logger, direct |
| `vacuum.stopped()` | `takeStopped()` | `takeStopped()` over `stopSeq` |
| `device.unit` / `target` / `bluetooth` | reads `Settings` | unchanged — dirty read of interface-owned scalars |
| `device.set_unit` / `set_target` / `set_ble` | `settings.setX` | `StoreClient::setUnit` / `setTarget` / `setBluetooth` (one-way) |
| `device.units` | constant | unchanged |
| `store.get` | `Routines::storeGet` → LittleFS | `StoreClient::get` round-trip (blocks) |
| `store.set` | `Routines::storeSet` → LittleFS | `StoreClient::set` round-trip (blocks); the file's bool is the answer |
| `screen.*` draw verbs | draw through `gContext->display()` | unchanged — the display is panel-owned |
| `screen.unit()` | reads `Settings` | unchanged — dirty read |
| `buttons.take()` | `Routines::takePress` from the press ring | unchanged; the ring is filled from the `ButtonEvent` queue, not by the panel reading `Buttons` |
| `buzzer.tone` / `off` | `buzzer.tone` / `off` | enqueue `Tone` / `Off` |
| `buzzer.play` | `buzzer.play` | enqueue `Play`; the bool return comes from the pure `BuzzerPlayer::parse` here |
| `routines.*` | registry methods | unchanged — panel-local |

With the round-trip, `store.set`'s Berry bool is the file's answer, exactly as today — there is no
divergence there. The remaining Berry-visible divergences are the optimistic `vacuum start` reply and
the interleaved log lines, both documented in [queues.md](queues.md) and [interface-task.md](interface-task.md).

## The routine poll

`Routines::poll` and `invoke` are unchanged in shape: one `be_pcall` per beat, never re-entering the
VM from a native call. What changes is only that the native calls it reaches now send queues or read
dirty instead of writing the machine.

## Docs to update with it

[docs/specs/routines.md](docs/specs/routines.md) currently states "nothing is suspended and there is
no second core, which is why there are no locks and no callbacks." That rationale is superseded by
this conversion: the VM now runs on the second core, the machine is written only through the command
queue, and reads are dirty. The "one entry per beat" and "a `poll` that never returns blocks the
loop" lines stay true, but the loop they block is now the panel loop, not the control loop.
