# Step the control tick without the scheduler

`tasks/MachineTasks.cpp` holds the device's control tick. One list in it is the tick's policy: the
queue's commands and the collapse of a burst, the order of the part updates and the service call,
and the run log. The other is FreeRTOS: the queue's creation, the two pinned tasks, their
priorities, stacks and cores, and the `vTaskDelayUntil` that asks for the next 50 ms beat. Because
the first list is reached only through the second, a change to what a tick does is decided by
flashing a board and reading its log.

This plan separates the two. The tick becomes a unit a caller posts to and steps with a millisecond,
so the sequence it runs can be read off a host case; the task file keeps the queue's transport and
the schedule, and its body is one call to the tick. The device is meant to behave exactly as it does
now: the same commands, in the same order, with the same replies and the same log lines. The
interface task, the panel's task and the wire's surfaces are left alone.

## A tick is a unit a caller steps

Today the only runner of the control tick is `controlTask`, and the state it advances —
`ControlState`, holding the machine, the logger, the last state logged and the vent's start — is
local to `tasks/MachineTasks.cpp`, as are `drainMachineCommands` and `logRunChanges`. The queue
handle is created by `spawnMachineTasks` and read through `postMachineCommand`, both in that file.
Nothing outside it can apply a queued command and service the machine once, so every rule the tick
contains is decided only by a running task on a board.

A caller posts a command, and a call with one millisecond applies what is queued first, then
`sensor().update(now)`, `pump().update(now)`, `service(now)` and the run log, in that order, on
every call. The queue a call drains is one the tick is built with rather than a handle in its file:
on the device it is the FreeRTOS queue `spawnMachineTasks` creates, and under a case it is whatever
the case fills. `controlTask` is that call on the 50 ms beat and keeps only what a task can do:
creating the queue before either task runs, the two tasks' priorities, stacks and cores, and the
delay that asks for the next beat. A link's post stays non-blocking, so a command the queue cannot
take stays with the outbox and is retried on the link's next poll. A `Reboot` is still the
composition's: the tick hands it back rather than performing it, and the release-then-restart stays
where it is now. A host stub of `FreeRTOS.h` is not the way to get a queue under a case: the file
would compile while the tick still could not be called, which is the problem rather than the fix.

The numbers are the queue's depth of 8, `CONTROL_PERIOD_MS` 50, and the update order above.

Checked: a host case builds the tick from the fake pump, sensor and valve the control law already
takes, a recording logger and a queue it fills. A `stop` and a `target 5` in one call are applied in
that order before the fake parts' updates; the sensor and the pump are updated once and the machine
serviced once per call; an empty call applies nothing but still updates and services; a posted
`Reboot` comes back to the caller unapplied; and the board build compiles both sketches with the
task body calling the same entry point.

## A burst collapses to the same commands

`drainMachineCommands` holds a rule the controller relies on as much as any other: a stop discards
everything queued before it, so a start sent before a stop never runs after it, and repeated target
writes reduce to the last one, so a burst cannot walk the valve back and forth. Each command it
drops is logged with why. None of that can be reached from outside `tasks/MachineTasks.cpp` today.
`CommandOutbox::post` and `flush`, which the links use, call `postMachineCommand` and so depend on
the file's queue as well.

The collapse belongs to the tick, with the queue it drains: `[start, stop]` applies only `stop` and
logs the start `before a stop`; `[start, target 10, stop]` applies only `stop` and logs both dropped
ones; `[target 10, target 20]` applies only `20` and logs `10` `for a later target`;
`[stop, target 5]` applies both in the order they were sent; an empty drain changes nothing. The
outbox still holds 8 commands and refuses the 9th — the newest — while the link has already answered
`ok`, and `flush` still retries oldest first and stops at the first that does not fit.

Checked: one case per burst, asserting the ops applied in order and the exact warn line
(`queue: dropped target 10.0 for a later target`); and the boundary — the 8th accepted, the 9th
refused, and `flush` delivering 1 to 8 in order.

## The run log is written by the tick

`logRunChanges` is the only narrative a run has: one line per state change, the time a vent took
when the state leaves `VENT`, the deadline line once when a run expires, and the fault code when one
latches. A bench read is how a run is understood, and no case asserts a word of it.

The same lines come from the tick, once each, and a tick where the state did not change writes
nothing. The log begins from the state the machine is in when the tick is built, so a start on the
first tick still writes `IDLE -> PULL`, and a line that names a fault carries its code. The lines
are `IDLE -> PULL`, `PULL -> HOLD`, `HOLD -> VENT`, `VENT -> IDLE after 1200 ms`,
`PULL -> FAULT (fault no_progress)`, and `run: hold deadline reached after 900 s`.

Checked: a case per line, driven through the tick with the fakes and a recording logger — the four
transitions, the vent's elapsed time in milliseconds, the fault's code, and the deadline line once
for a run whose hold expires and never for one that stops first.

## Consequences

- Nothing the device does changes: the ops, the order they are applied in, the replies the links get
  and the lines a run writes all stay. The `ok` a link sees before its command runs is still the
  domain's, not the tick's.
- `CommandOutbox` no longer calls a function defined in the task file; the queue it posts into is
  the one the tick drains, so a full queue is that queue's refusal rather than a global function
  returning false.
- `tasks/MachineTasks.cpp` keeps the queue's creation, the two tasks and the beat, and the tick's
  rules have one home. The parking-lot entry on verifying the two builds on hardware is still the
  only place the task's own timing is observed.

## Deferred

- The schedule: the tasks' priorities, stacks and cores, `vTaskDelayUntil`, and any watchdog.
- The interface task: its 5 ms beat and the two presence globals the panel's overlay reads.
- `tasks/PanelTasks.cpp`, whose task is interpreter-bound and draws through the panel.
- A host RTOS port, as a way to run the tasks themselves: the tick does not need one, and a kernel
  port is heavier than the code it would run.

## How it is known to hold

1. The firmware's host check passes the tick, burst and log cases above.
2. `python scripts/arduino.py --task build` and
   `python scripts/arduino.py --task build --sketch headless` both compile, with the control task's
   body calling the tick.
3. On the bench, a start and a stop write the same lines the cases assert, in the same order.
