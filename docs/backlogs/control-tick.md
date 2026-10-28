# The control tick as a unit

The tick in `tasks/MachineTasks.cpp` is reached only through the FreeRTOS task that runs it: its
state, its run log and its command queue are in that file's anonymous namespace, and the file names
`<freertos/queue.h>` and `<freertos/task.h>`, so no case can apply a queued command and service the
machine once. This is the work that makes the tick a unit a caller posts to and steps with a
millisecond, built from the mocks the plan names — three part fakes, a recording logger and a
queue — with the device behaving exactly as it does now. The requirement is in
`docs/plans/step-the-tick-without-the-scheduler.md`.

Every item leaves the tree working: `python scripts/arduino.py --task build` and
`python scripts/arduino.py --task build --sketch headless` compile, and the unit's source names no
FreeRTOS header. The cases the plan names are not written here, and neither is the host check they
would run in: this list is the split that makes them possible, and the tests are later work.

## Epic 1 — the queue a caller posts to

- [x] 1.1 — The queue the tick drains becomes a thing the control side is built with rather than a
  handle in the task file: a link posts into it, a case can fill it, and on the device it is the
  FreeRTOS queue the composition builds before either task runs. The links' outbox posts into that
  queue instead of calling a function the task file defines, so its post and flush are reached
  through the queue they are given. Verified: both sketches compile, and the outbox's source names
  no function of the task file.

## Epic 2 — the tick a caller steps

- [x] 2.1 — The tick becomes a unit with a way in and one call: a post of a command; a call with a
  millisecond that applies what is queued, updates the sensor, updates the pump, services the
  machine and writes the run log in that order; and a Reboot handed back rather than performed. The
  collapse of a burst and the run log's own state — the last state logged and the vent's start —
  move with it, so the control task holds none of what a tick decides. The control task keeps the
  two tasks' priorities, stacks and cores and the delay that asks for the beat, and its body is that
  call. Verified: both sketches compile with the task body calling the unit, the unit's source names
  no FreeRTOS header, and the task file keeps only the configuration and the beat.
