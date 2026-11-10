# Parking lot

Unrealized "what if" ideas: work that has been considered and deliberately deferred. An entry is a
note, not a plan — a sentence or two saying what the idea is and what is holding it back. Someone
who picks the idea up does the design then; the details written today would be guesses. Nothing
here is a commitment: an item leaves when it is done and the result is in the tree, or when it is
dropped and the reason is in git.

## Second pump/sensor/solenoid combo

Add a second pump, pressure sensor and vent solenoid so the device can run two chambers. The machine,
the `vacuum` wire commands and the alpha PCB all assume one combo today.

## The board's own free heap

What the device actually has free, and how low a run takes it, is a number only the board can
answer, and `ESP.getFreeHeap()`, `getMaxAllocHeap()` and `getMinFreeHeap()` are all there to report
it. Blocked by: the serial protocol has no field for it, and nothing has decided whether one belongs
there.

## Thermal and continuous-duty protection

`PumpDutyMaxPercent` is a duty cap, not a thermal limit: a long hold at the top of the range can
overheat the pump. A maximum continuous run, a derating curve, or a current or temperature sense
would cover it. Blocked by: nothing on the rig senses temperature or pump current, and neither pump
has a measured continuous-duty figure to set a body-safe default from. The two-hour ceiling on a run
is the whole of the protection there is.

## Low-battery start refusal

`startRefusal()` should answer `low_battery` when a gauge is fitted and the pack is below a
threshold, so a brownout is refused rather than survived. Blocked by: no pack fitted to the device
yet, so there is no gauge to read — the alpha PCB leaves the divider deferred until a Super Mini
variant with one is chosen.

## A local event log

The last N commands, stops, transitions and faults kept in NVS and readable over the serial line, so
an incident can be understood after the fact. Blocked by: nothing decided about where it lives, how
it is bounded, or what reads it back.

## Verify the device on hardware

The serial-only device has only ever been compiled. Everything it promises still needs a board: a
set that pulls to its depth and holds, a vent that reaches the floor, a run that releases when its
deadline expires, a fault that latches and a reset that clears it, and the valve venting when the
coil is not held.

## One file to compile

A task in [scripts/arduino.py](../scripts/arduino.py) that keeps a verbose build's compile command
and replays it with `-fsyntax-only` for one file, so a library change is checked in seconds rather
than by a whole build. Blocked by: nothing has decided whether the host suite answers the same
question for the source it compiles, which would leave the task unused.
