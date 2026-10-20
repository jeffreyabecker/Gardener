# FreeRTOS conversion — plan

Repo root: `C:\ode\eggplan-vacuum` (paths below are relative to it).

## Problem

The firmware is a single cooperative `loop()`: [App::update](libraries/VacuumCore/src/app/App.cpp)
calls every device's `update(now)`, the PID `vacuum.service(now)`, the serial and BLE polls, the
buzzer, the button holds, run logging and the screen, in order, once per pass. Consequences:

- The PID runs "as fast as `loop()` returns", which varies with the SH1106 frame flush (whole-buffer
  I2C), LittleFS commands (`fs`, `settings`, `store`), and a Berry routine's `poll()`.
- A slow routine `poll()` blocks the control loop — the docs' known cost ("a `poll` that never
  returns blocks the loop").

NimBLE already runs its host on core 0 with the loop on core 1, and BLE writes already cross cores
through `volatile` single-value slots in [BleLink](libraries/VacuumCore/src/app/ble/BleLink.h) — so
cross-core handoff is an existing, working pattern here. This plan replaces the one loop with three
FreeRTOS tasks under one rule: **one writer per piece of state, updates serialized into the owning
task, reads direct and dirty with no lock.**

## Architecture

Three pinned tasks, one owner per object. ESP32 Arduino already links FreeRTOS
(`<freertos/FreeRTOS.h>`, `task.h`, `queue.h`, `semphr.h`), so no new dependency.

### Task 1 — Control (core 1, priority 2, 50 ms via `vTaskDelayUntil`)

The machine's owner. Each tick: `sensor.update`, `pump.update`, `battery.update`,
`vacuum.service`, `buzzer.update`, the Back-hold stop, the Back+OK reset (`safeReboot`), run-state
logging, and draining the machine-command queue. Other tasks read these objects directly.

Owns: `VacuumSystem`, `Pump`, `PressureSensor`, `Battery`, `BuzzerPlayer`/`Buzzer`, `Buttons`
(debounce + press classification).

### Task 2 — Interface (core 0, priority 1)

The wire and storage owner. Runs `SerialLink::poll`, `BleLink::poll` (applyPending/publish), drains
the store queue, and writes the link replies to `Serial`. Log lines are written
straight to `Serial` by whichever task produces them — logging has no IPC of its own.

Owns: `Ownership`, `Settings`, `Storage` (LittleFS — not re-entrant, so one owner), `SerialLink`,
`BleLink` loop-side, and the `device set log` flag — written here, read dirty by every task's own
`StreamLogger`.

### Task 3 — Panel / VM (core 0, priority 0, ~10 ms beat)

The glass and the interpreter owner. Picks the page by reading the live `Ownership` and
`VacuumSystem` (connected page / routine / mark), redraws the static pages at `UI_REFRESH_MS`, polls
the Berry VM when a routine is active, consumes button-press events, and watches the OK
default-hold.

Owns: `Display` (Adafruit + `Overlay`), `ConnectedUI`, `DisconnectedUI`, `Routines` (the VM). The
display is private to this task — nothing else draws — so there is no shared display.

## State sharing

**Dirty reads are fine. No locks, no mutexes, no semaphores.** The vast majority of what crosses a
task boundary is read-only or read-infrequently data — pressure, target, state, fault, battery,
ownership, settings, button hold times — and a dirty read of it is harmless: the value is re-read on
the next poll anyway. So the rule is one writer per field, direct reads everywhere else:

- Each object has exactly one owning task that writes it.
- Every other task reads the live object directly (`vacuum.currentKpa()`, `ownership.owner()`,
  `settings.unit()`, `heldMs(...)`). A read may see a half-updated view (one field from this tick,
  one from the last); that is accepted and not guarded.
- No snapshot structs, no seqlock, no mutex. A lock appears only if a future change gives a field a
  second writer — and that is a review catch, not something built ahead of time.

The only IPC that remains is three queues, and each exists for one reason: to move a **write** to the
object's owning task. A queue serializes a write without ever locking the data, which is why it is
used instead of a lock; two tasks writing one field is the only situation this plan would lock for,
and it never arises.

### IPC budget — the complete list, each justified

Reads use none of these. No mutex, no semaphore, no seqlock, no snapshot exists in this design.

| Queue | Producer → Consumer | Carries | Why it exists |
|---|---|---|---|
| machine commands | Interface, Panel → Control | `start`, `stop`, `setTarget`, `clearTarget`, `resetFault`, `reboot`, `tone/play/off` | `VacuumSystem`/`BuzzerPlayer` have one writer (control); serial, BLE and the VM originate the writes. |
| store | Panel (Berry `device.*`, `store.*`) → Interface | `get`, `set`, `set_unit`, `set_target`, `set_ble` | `Settings`/`Storage` have one writer (interface); LittleFS is not re-entrant, and the VM originates the writes. |
| button events | Control → Panel | one classified `Press` per edge | `Buttons` have one writer (control, the task that owns the safety holds); the VM consumes classified presses. |

FreeRTOS queues, so a blocked consumer back-pressures the producer instead of dropping events (the
current button ring and BLE last-write-wins slots keep their own semantics where they already exist).

Logging is the deliberate exception: every task writes its log lines straight to `Serial`, and
interleaved output is an expected, accepted outcome — logs are not worth an IPC primitive.

## Sub-plans

- [tasks.md](tasks.md) — boot, task creation (cores, priorities, stack), the three loop bodies,
  timing, and how the lines of `App::update()` map to them.
- [queues.md](queues.md) — the ownership matrix, the three queue payloads, and the one
  request/response (`store.get`) with its justification.
- [control-task.md](control-task.md) — the control task: the tick, applying machine commands, the
  button ownership, and the safety holds.
- [interface-task.md](interface-task.md) — the interface task: the domain-by-domain serial change,
  the BLE loop-side change, the log flag, and LittleFS single-owner.
- [panel-vm-task.md](panel-vm-task.md) — the panel/VM task: page selection, the display, and the
  `Routines`/`Bindings` module-by-module change.
- [backlog.md](backlog.md) — the implementation backlog: every change above, broken into
  individually completable tasks, in build order.

## Behaviour notes (deliberate, small divergences)

- Reply latency: commands that go through a queue land on the next 50 ms control tick, so a
  `vacuum start`/`stop`/`reset` reply can be up to one tick late. `vacuum get` and ownership replies
  are immediate (direct dirty reads). The wire contract (grammar, field order, values) is unchanged.
- `vacuum start` reply is optimistic (decided from a dirty read) — the accepted divergence is spelled
  out in [queues.md](queues.md).
- The Berry `poll` beat becomes the panel task's ~10 ms cadence instead of "as fast as `loop()`
  returned". `poll(now)` still receives `millis()`.
- Boot (`App::begin`) stays single-threaded in `setup()`: bring up Wire1/panel, sensor, pump, vent,
  battery, buzzer, storage, settings, routines, links in the current order, then spawn the three
  tasks. `loop()` becomes `vTaskDelete(NULL)`.

## Order of work

1. Foundation — `Tasks.h/.cpp`, the three queue handles, task creation, empty `loop()`; the build
   still compiles with the old `update()` in place ([tasks.md](tasks.md)).
2. Control task ([control-task.md](control-task.md)).
3. Interface task, serial then BLE ([interface-task.md](interface-task.md)).
4. Panel/VM task ([panel-vm-task.md](panel-vm-task.md)).
5. Delete `App::update()`, wire `gardener.ino` to the tasks.
6. Verify: `python scripts/arduino.py --task build`; review for the `hal/`→`app/` layer
   rule and for any cross-task write (grep every `context_.<thing>().` mutation and confirm it runs
   on the owner task).

## Global notes

- The one rule, everywhere: **dirty reads are fine; add a lock only when a value gains writers on
  more than one task.** The three queues move writes to their owner; no lock exists. If a future
  change gives an object a second writer, that is where a lock or a queue appears — not before.
- LittleFS and the `Wire1`/SH1106 device are single-owner by rule, not by lock; the reviewer confirms
  no second task reaches them (a review catch, not a build failure).
- No host test suite exists; validation is the build plus careful concurrency review.
