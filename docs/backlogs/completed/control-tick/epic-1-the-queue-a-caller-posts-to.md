# Epic 1 — the queue a caller posts to

The machine's queue was a QueueHandle_t inside `tasks/MachineTasks.cpp`, written through a free
function that only that file defined, so a link's outbox and the control tick could only meet
through it.

Item 1.1 replaced it with a type the composition builds. `machine/CommandQueue.h` names `post` and
`take` and the depth of 8; `tasks/RtosCommandQueue.h` is the device's FreeRTOS implementation. The
sketches build one and hand it to `buildPanelRoot`, to the serial link, to the Bluetooth link and to
`spawnMachineTasks`, so `CommandOutbox` holds the queue it posts into and the panel's, each link's
and the control task's writes all land in one place.

Verified: `python scripts/arduino.py --task build` and `python scripts/arduino.py --task build
--sketch headless` compile, and the outbox's source names no function of the task file. The
machine-alone build had not compiled since the routine setting store landed — the store is
panel-only and named a Gfx header without the guard its neighbours carry — so that guard went in
with the item. The interpreter's check is red on its harness naming `routines` and `ux`, which it
was before this item and is not this epic's subject.
