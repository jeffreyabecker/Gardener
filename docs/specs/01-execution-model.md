# Spec 01 — Execution model

Status: draft spec. Implements [the plan](../berry-on-device-control.md) §5 and constraint 1
("a runaway program must not delay or corrupt control").

The plan left the mechanism open on purpose and this spec is written the same way: what has to be
true is settled, the smallest answer comes first, and what a second execution context would cost
is the fallback rather than the design.

## 1. The loop

The device keeps the loop it has, with the program added to it:

```
loop:                                    # free running, as today
  now = millis()
  battery.update(now)
  system.service(now)                    # sensor poll, pump ramp, PID, state machine
  buttons.update(now)                    # spec 04
  link.poll(now)                         # drain, dispatch, reply — every iteration
  program.runSlice(cfg::VM_SLICE_INSTRUCTIONS)
  refresh the snapshot in place          # spec 07
```

The sensor poll and the control step are already time gated inside `service()`, and the loop
itself never sleeps. Everything here except the program slice exists today. There is no second
task, no IPC layer and no lock, because there is nothing to interleave.

## 2. Modes

Contention is resolved by two mutually exclusive modes, not by locks:

- **Stand-alone** — the VM is running and has access to everything: the running program commands
  `VacuumSystem` and draws/flushes `Display` directly.
- **Peripheral** — the VM is stopped; the serial system has full control and does the same
  directly.

`set mode` (spec 06) switches modes; attaching the serial link does not. There is never a moment
when the program and the serial line are both controlling or both drawing, which is the whole
reason nothing below needs a lock.

## 3. What is shared

What both of them reach: `VacuumSystem`, the display, the snapshot, the button event ring and
`ProgramStore`. Nothing else.

"Shared" means reachable by both, not touched by both at once: a program runs inside the loop, so
a program instruction and a `status` reply are never in flight together, and there is no
interleaving to accept anywhere. That is the difference this model buys — the previous draft of
this spec had to argue for a list of tolerated races (adjacent-tick snapshot reads, interleaved
`Wire` traffic, log records behind a `portMUX`) and none of them exist while there is one loop.

Who owns which of those, and what a program may do with the store, is spec 02 §7 and spec 03 §5.

`ProgramStore` is the one that is written by one side and read by the other: upload, remove and
seed belong to the serial line, and a program only reads, through `programs.*` (spec 02 §7),
served from the in-RAM index. The old draft needed a paragraph here about reads that race an
upload returning stale data; with one loop there is nothing to race.

## 4. What is never shared

- **The devices.** The sensor, the pump and the valve are driven from `VacuumSystem::service()` in
  the loop and nowhere else. A program commands them through `vacuum.*`, which is the same
  `VacuumSystem` the serial line uses, so there stays exactly one writer to the pump and the
  valve.
- **The panel.** Whoever is controlling draws it — the program in stand-alone, the serial line in
  peripheral — and the `Overlay` puts the battery on every flush either way.

## 5. Runaway programs

A program that never yields does not stop the loop; it just gets less done:

- The slice ends when its budget is spent, and the loop runs its tick before the program resumes,
  so a `while true` program advances one slice per iteration and cannot hold the tick.
- `VM_SLICE_INSTRUCTIONS` is a `cfg::` constant in
  [core/Config.h](../../libraries/VacuumCore/src/core/Config.h), because it is sized on the bench
  by the measurement in §6 and not by argument. The size that matters: one slice must be a small
  fraction of `CONTROL_PERIOD_MS`, so a tick that comes due during it is late by that fraction and
  no more.
- The budget is an instruction count rather than a wall-clock deadline — a counter is cheap to
  check and needs no timer — and it is spent through Berry's execution-limit hook. `vacuum.delay`
  is the deliberate way for a program to yield; the budget is what makes an unwilling one yield
  anyway.
- Stopping a program — `stop`, the Back-hold abort, or a mode switch — means the loop stops
  calling it. There is no task to delete and nothing to restart.

## 6. The gate, and the fallback

**The gate.** With `default` and a deliberately hostile program (`while true`, no
`vacuum.delay`) both running, `status` keeps answering and the control step keeps
`CONTROL_PERIOD_MS`. Measure it on the bench: count control steps against `millis()` over a
minute with the hostile program running, and size `VM_SLICE_INSTRUCTIONS` from what it says.

**The fallback.** If the gate fails — the vendored Berry port has no execution-limit hook, or a
slice small enough to hold the tick leaves a program unusably slow — then the program needs a
context of its own, and this spec gets rewritten rather than extended. A second context buys back
everything §3 and §4 are currently spared: synchronization around the plant, the snapshot as a
cross-task boundary, two readers on the `Wire` (the sensor read and the display flush), logs
behind a `portMUX`, and a watchdog with a restart path for a VM that will not stop. That is the
price the plan's §5.3 lists, and it is paid only if the measurement asks for it.

## 7. Acceptance criteria

- `set mode peripheral` stops the VM and gives the serial system full control;
  `set mode standalone` returns control to the VM (runs `default`).
- Attaching the serial link does not change the mode.
- A `while true` program with no `vacuum.delay` does not stop `status` replies or the
  PID tick.
- A busy program drawing every slice cannot delay the control loop.
- Disconnecting in peripheral mode vents and returns to stand-alone (spec 05 §3).
- Buttons and the Back-hold abort keep working while a program runs (spec 04).
