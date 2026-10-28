# The control tick as a unit

The tick in `tasks/MachineTasks.cpp` is reached only through the FreeRTOS task that runs it: its
state, its run log and its command queue are in that file's anonymous namespace, and the file names
`<freertos/queue.h>` and `<freertos/task.h>`, so no case can apply a queued command and service the
machine once. This is the work that makes the tick a unit a caller posts to and steps with a
millisecond, compiled and driven against a minimal set of mocks — three part fakes, a recording
logger and a queue — with the device behaving exactly as it does now. The requirement is in
`docs/plans/step-the-tick-without-the-scheduler.md`.

Every item leaves the tree working: `python scripts/arduino.py --task build` and
`python scripts/arduino.py --task build --sketch headless` compile. The items from 1.3 on add cases
to the firmware's host check — the library's own C++ compiled and driven on the host with no
interpreter, filesystem, radio or RTOS. That check is not in the tree yet, and 1.3 is where the
first case needs it.

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

## Epic 2 — the burst

- [ ] 2.1 — The collapse is a case per burst: a start then a stop applies only the stop and logs the
  start before a stop; a start, a target 10 and a stop applies only the stop and logs both dropped
  ones; a target 10 then a target 20 applies only 20 and logs 10 for a later target; a stop then a
  target 5 applies both in the order they were sent; and an empty drain applies nothing. Verified:
  one case per burst, asserting the ops applied in order and the exact warn line, with the dropped
  target's reading `queue: dropped target 10.0 for a later target`.
- [ ] 2.2 — The outbox's boundary is a case against the same queue: 8 commands held and the 9th
  refused while the link has already answered ok, with flush retrying oldest first and stopping at
  the first that does not fit. Verified: a case asserts the 8th accepted, the 9th refused and flush
  delivering 1 to 8 in order, through the queue the tick drains.

## Epic 3 — the run log

- [ ] 3.1 — The run log is written by the tick, once per line, from the state the machine is in when
  the unit is built: IDLE -> PULL on a start on the first tick, PULL -> HOLD, HOLD -> VENT, and
  VENT -> IDLE after 1200 ms naming the elapsed time in milliseconds, with no line on a tick where
  the state did not change. Verified: a case per line, driven through the unit on the case's clock
  with the fakes and a recording logger.
- [ ] 3.2 — A fault and a deadline write their numbers: PULL -> FAULT (fault no_progress) carries
  the code the machine latched, and run: hold deadline reached after 900 s is written once for a run
  whose hold expires and never for one that stops first. Verified: a case for each, with a run that
  stops before its hold writing no deadline line.

## Epic 4 — the board

- [ ] 4.1 — A bench run writes the lines the cases assert: a start and a stop from the wire produce
  the same transitions and the same vent time in milliseconds, in the same order. Verified: a bench
  read of the serial log over a start and a stop.
