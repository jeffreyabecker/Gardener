# The control tick as a unit

The tick in `tasks/MachineTasks.cpp` is reached only through the FreeRTOS task that runs it: its
state, its run log and its command queue are in that file's anonymous namespace, and the file names
`<freertos/queue.h>` and `<freertos/task.h>`, so no case can apply a queued command and service the
machine once. This is the work that makes the tick a unit a caller posts to and steps with a
millisecond, compiled and driven against a minimal set of mocks — three part fakes, a recording
logger and a queue — with the device behaving exactly as it does now. The requirement is in
`docs/plans/step-the-tick-without-the-scheduler.md`.

Every item leaves the tree working: `python scripts/arduino.py --task build` and
`python scripts/arduino.py --task build --sketch headless` compile, and the unit's source names no
FreeRTOS header. The cases the plan names are not written here, and neither is the host check they
would run in: this list is the split that makes them possible, and the tests are later work.

## Epic 1 — the tick a caller steps

- [x] 1.1 — The queue the tick drains becomes a thing the control side is built with rather than a
  handle in the task file: a link posts into it, a case can fill it, and on the device it is the
  FreeRTOS queue the composition builds before either task runs. The links' outbox posts into that
  queue instead of calling a function the task file defines, so its post and flush are reached
  through the queue they are given. Verified: both sketches compile, and the outbox's source names
  no function of the task file.
- [ ] 1.2 — The tick's policy becomes a unit with a way in and one call: a post of a command, a call
  with a millisecond that applies what is queued, updates the sensor, updates the pump, services the
  machine and writes the run log in that order, and a Reboot handed back rather than performed. The
  last state logged and the vent's start are the unit's, so the control task holds none of it.
  Verified: both sketches compile with the control task's body being that call on the 50 ms beat,
  the task file keeps only the queue's creation, the two tasks' priorities, stacks and cores and the
  delay, and the unit's source names no FreeRTOS header.
- [ ] 1.3 — The unit and its first cases join the firmware's host check: a case builds it from fakes
  of the pump, sensor and valve, a recording logger and a queue it fills; a stop and a target 5 in
  one call are applied in that order before the parts update; the sensor and the pump update once
  and the machine services once per call; an empty call applies nothing but still updates and
  services; and a posted Reboot comes back to the caller unapplied, with the case stepping the beat
  itself. Verified: the check compiles the unit with no FreeRTOS, Berry, filesystem, Wire or NimBLE
  header on its path and passes those cases, and the case file names no Machine and no concrete
  part.

## Epic 2 — the tick a caller steps

- [ ] 2.1 — The tick becomes a unit with a way in and one call: a post of a command; a call with a
  millisecond that applies what is queued, updates the sensor, updates the pump, services the
  machine and writes the run log in that order; and a Reboot handed back rather than performed. The
  collapse of a burst and the run log's own state — the last state logged and the vent's start —
  move with it, so the control task holds none of what a tick decides. The control task keeps the
  two tasks' priorities, stacks and cores and the delay that asks for the beat, and its body is that
  call. Verified: both sketches compile with the task body calling the unit, the unit's source names
  no FreeRTOS header, and the task file keeps only the configuration and the beat.

## Epic 3 — the device

- [ ] 3.1 — A bench run writes the lines it wrote before: a start and a stop from the wire produce
  the same transitions, the same vent time in milliseconds and the same order. Verified: a bench
  read of the serial log over a start and a stop.
