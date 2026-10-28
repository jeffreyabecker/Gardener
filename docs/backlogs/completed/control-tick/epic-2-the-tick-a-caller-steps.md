# Epic 2 — the tick a caller steps

The control tick's policy lived in `tasks/MachineTasks.cpp` behind FreeRTOS: the drain and the
collapse of a burst, the order of the part updates, the run log, and the queue's handle, all in that
file's anonymous namespace. Nothing outside it could apply a queued command and service the machine
once, and the file could not be compiled without the RTOS.

Item 2.1 moved that policy into `machine/ControlTick.h`: a `post`, a `tick(nowMs)` that drains,
updates the sensor, updates the pump, services the machine and writes the run log, and a
`takeReboot` that hands a queued Reboot back to the composition. The last state logged, the vent's
start and the queue are the unit's, and it is built from the vacuum system, a logger and the queue
the panel and the links already share, so its source names no FreeRTOS header.
`tasks/MachineTasks.cpp` keeps the task configuration, both task bodies and the beat: `controlTask`
is one call to the unit, a handed-back Reboot performed after it, and the delay.

Verified: `python scripts/arduino.py --task build` and `python scripts/arduino.py --task build
--sketch headless` compile with the task body calling the unit; `machine/ControlTick.h` and
`machine/ControlTick.cpp` name no `freertos` header; and the control task's state is the unit plus
the machine a Reboot belongs to.

The cases the plan names, and the host check that would run them, are not part of this epic. What it
leaves is a tick a case can build from three part fakes, a recording logger and a queue it fills.
