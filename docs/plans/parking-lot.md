# Parking lot

Unrealized "what if" ideas: work that has been considered and deliberately deferred. An entry is a
note, not a plan — a sentence or two saying what the idea is and what is holding it back. Someone
who picks the idea up does the design then; the details written today would be guesses. Nothing
here is a commitment: an item leaves when it is done and the result is in the tree, or when it is
dropped and the reason is in git.

## Second pump/sensor/solenoid combo

Add a second pump, pressure sensor and vent solenoid so the device can run two chambers. `AppContext`, the
`vacuum` wire domain and the alpha PCB all assume one combo today.

## Firmware size

Measured on 2026-10-19 with `--task build`: 566,997 bytes, 43% of the 1,310,720-byte app slot. An
empty sketch with the same FQBN is 298,069 bytes (22%). Two levers were identified and not taken.

### Revisit the partition scheme

Pin `huge_app` instead of `default` in [scripts/arduino.py](../../scripts/arduino.py): one 3 MB
app slot and 896 KB of LittleFS reports ~18% instead of 43%. The OTA slot it drops is unused, and
the data image has to be uploaded again because the offset moves.

### Try `-fno-exceptions`

The image carries 36 KB of exception metadata, about 17 KB of it in objects this build compiles.
Nothing throws today, but any vendored C++ added later would have to make the same choice.

## Venting down to a shallower target

A shallower target set while pulling or holding currently just stops the pump and coasts: the valve
stays closed, so the reading falls only as fast as the chamber leaks
([VacuumSystem::regulate](../../libraries/VacuumCore/src/app/VacuumSystem.cpp)). A controlled vent —
stop, open the valve, then close and restart as the reading crosses the new target — can be composed
from existing verbs by a routine or host, which is why it is deferred rather than designed. The open
question is the landing: the valve is bang-bang and the chamber is small, so overshoot toward
ambient is likely and the pump has to recover it.

## Stop building serial replies as String

The serial domains assemble each reply with `String` concatenation (`ok proto=1 fw=...`) and print
it. Writing fields straight to the stream would skip the per-reply allocation; it is deferred
because the firmware convention says `String` is good enough for replies and no reply has been
measured as worth the churn.

