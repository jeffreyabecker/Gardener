# Spec 01 — Execution model

Status: draft spec. Implements [the plan](../berry-on-device-control.md) §5 and constraint 1
("a runaway program must not delay or corrupt control").

The plan left the mechanism open on purpose and this spec is written the same way: what has to be
true is settled, the smallest answer comes first, and what a second execution context would cost
is the fallback rather than the design.

## 1. The loop

The device keeps the loop it has, with the program added to it:

```
loop:
  now = millis()
  battery.update(now)
  system.service(now)                    # sensor poll, pump ramp, PID, state machine
  buttons.update(now)                    # spec 04
  link.poll(now)                         # drain, dispatch, reply — every iteration
  program.run()                          # the VM; calls service() back via its hook (§5)
  refresh the snapshot in place          # spec 07
```

Everything above the program line exists today, and one function does it — call it the service
pass — because a program has to be able to trigger it too (§5). The sensor poll and the control
step are already time gated inside `service()`, and the loop itself never sleeps. There is no
second task, no IPC layer and no lock, because there is nothing to interleave.

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

"Shared" means reachable by both, and never at the same instant: everything above runs in one
loop. The service pass can be triggered from inside a program's execution (§5), so a `status`
reply may be produced while the VM is on the stack — but it only reads the snapshot and writes the
stream, and neither is being written by the program at that instant. The previous draft had to
argue for a list of tolerated races (adjacent-tick snapshot reads, interleaved `Wire` traffic, log
records behind a `portMUX`) because it had two contexts; with one loop, none of them exist.

Who owns which of those, and what a program may do with the store, is spec 02 §7 and spec 03 §5.

`ProgramStore` is the one item there written by one side and read by the other: upload, remove and
seed belong to the serial line, and a program reads the index through `programs.*` (spec 02 §7),
served from RAM. A parameter value is the one write a program can ask for — `programs.set_param`,
and the browser's editor — and it goes through the same store call the wire uses, so there is still
one writer at a time. The old draft needed a paragraph here about reads that race an upload
returning stale data; there is nothing to race.

## 4. What is never shared

- **The devices.** The sensor, the pump and the valve are driven from `VacuumSystem::service()` in
  the loop and nowhere else. A program commands them through `vacuum.*`, which is the same
  `VacuumSystem` the serial line uses, so there stays exactly one writer to the pump and the
  valve.
- **The panel.** Whoever is controlling draws it — the program in stand-alone, the serial line in
  peripheral — and the `Overlay` puts the battery on every flush either way.

## 5. Runaway programs

A program that never yields cannot starve the service pass; the hook drags it in:

- Berry's execution-limit hook fires every `cfg::VM_HOOK_INSTRUCTIONS` instructions, and the hook
  runs the service pass before returning to the VM. So a `while true` program with no
  `vacuum.delay` still gets the plant a tick and the serial line a poll — the hook is the yield
  point, whether or not the program intended one.
- `VM_HOOK_INSTRUCTIONS` is a `cfg::` constant in
  [core/Config.h](../../libraries/VacuumCore/src/core/Config.h), sized on the bench by the
  measurement in §6: the interval is how late a tick can be, so it is a small fraction of
  `CONTROL_PERIOD_MS`, and no smaller than that needs.
- The interval is an instruction count rather than a wall-clock deadline — a counter is cheap to
  check and needs no timer. `vacuum.delay(ms)` is the deliberate way to hand the pass over sooner.
- The hook is also where a stop is noticed: `stop`, a mode switch and the Back-hold abort set a
  flag, and the hook unwinds the VM out of its call at the next firing (spec 03 §3). This is the
  one place the VM is aborted rather than asked nicely, and it is why an unwilling program still
  stops.
- Nothing here suspends and resumes a Berry call: the hook calls in, then returns to the VM, so
  there is no coroutine and no saved stack to restore.

**Reentrancy is the thing the gate also has to answer.** The service pass runs from inside a
program's execution, so a command that arrives while one runs is executing reentrantly. Reads
(`status`, `info`, `list`) are safe; a command that would change what is running — `run`, an
upload, `set mode` — asks for a program rather than starting one, and the yield point applies it
(spec 05 §3). If that turns out to be unworkable, it belongs to the fallback below rather than to
a lock here.

## 6. The gate, and the fallback

**The gate.** With `default` and a deliberately hostile program (`while true`, no
`vacuum.delay`) both running, `status` keeps answering and the control step keeps
`CONTROL_PERIOD_MS`. Measure it on the bench: count control steps against `millis()` over a
minute with the hostile program running, and size `VM_HOOK_INSTRUCTIONS` from what it says.
Answer the reentrancy question at the same time: with the hostile program running, send
`status` (must reply) and `run other` (must take effect at the yield point, not inside the
program's stack).

**The fallback.** If the gate fails — no execution-limit hook in the vendored port, a hook
interval that cannot hold the tick, or service calls from inside the VM that turn out not to be
safe — then the program needs a context of its own, and this spec gets rewritten rather than
extended. A second context buys back everything §3 and §4 are currently spared: synchronization
around the plant, the snapshot as a cross-task boundary, two readers on the `Wire` (the sensor
read and the display flush), logs behind a `portMUX`, and a watchdog with a restart path for a VM
that will not stop. That is the price the plan's §5.3 lists, and it is paid only if the measurement
asks for it.

## 7. Acceptance criteria

- `set mode peripheral` stops the VM and gives the serial system full control;
  `set mode standalone` returns control to the VM (runs `default`).
- Attaching the serial link does not change the mode.
- A `while true` program with no `vacuum.delay` does not stop `status` replies or the
  PID tick.
- A busy program drawing as fast as it can cannot delay the control loop.
- Disconnecting in peripheral mode vents and returns to stand-alone (spec 05 §3).
- Buttons and the Back-hold abort keep working while a program runs (spec 04).
