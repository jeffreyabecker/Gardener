# Parking lot

Unrealized "what if" ideas: work that has been considered and deliberately deferred. An entry is a
note, not a plan — a sentence or two saying what the idea is and what is holding it back. Someone
who picks the idea up does the design then; the details written today would be guesses. Nothing
here is a commitment: an item leaves when it is done and the result is in the tree, or when it is
dropped and the reason is in git.

## Second pump/sensor/solenoid combo

Add a second pump, pressure sensor and vent solenoid so the device can run two chambers. The machine
context, the `vacuum` wire domain and the alpha PCB all assume one combo today.

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

## One file to flash

A `--task release` that packs [firmware/gardener/data](../../firmware/gardener/data) into the build's
`gardener.ino.merged.bin`, so a device is flashed at `0x0` in one esptool command instead of
`--task upload` and then `--task data`. What holds it back is that the image would ship whatever
`settings.cfg` and routines are in the tree and clear NVS on the way, which nobody has decided on.

## Nudges inside a routine

A gesture that adjusts the run while it is going instead of ending or restarting it: deeper by a step,
one more set, or straight to rest and done. A routine page's free keys — Forward and Down — could
carry one, but no run has a mid-run adjustment in its phase machine yet.

## Verify the two builds on hardware

The machine/panel split and the two builds have only ever been compiled. Everything the specs now
promise still needs a device: the full device's serial and Bluetooth surfaces with writes accepted
from any link, the panel's page and routine live whatever else is connected, and Back+OK rebooting.
The machine-alone build needs the same: `device get` reporting `caps` and `log` with no battery or
unit field, `fs` and `buzzer` answering `unknown_domain`, and a run that a second link can command
mid-flight.

## Thermal and continuous-duty protection

`pump_max` is a duty cap, not a thermal limit: a long hold at the top of the range can overheat the
pump. A maximum continuous run, a derating curve, or a current or temperature sense would cover it.
Blocked by: nothing on the rig senses temperature or pump current, and neither pump has a measured
continuous-duty figure to set a body-safe default from. A single hold is now bounded at fifteen
minutes, which is the whole of the protection there is.

## Low-battery start refusal

`startRefusal()` should answer `low_battery` when a gauge is fitted and the pack is below a
threshold, so a brownout is refused rather than survived. Blocked by: no gauge on the
machine-alone build, and no pack fitted to either build yet — the alpha PCB leaves the divider
deferred until a Super Mini variant with one is chosen.

## A local event log

The last N commands, stops, transitions and faults kept in NVS and readable over the serial
line and the Bluetooth diagnostics, so an incident can be understood after the fact. Blocked by:
nothing decided about where it lives, how it is bounded, or what reads it back.

## Pairing

Just Works is proximity rather than authorization: anyone in radio range of an advertising machine
can pair and command pressure, and bonds are uncapped. Until a pairing window or passkey replaces
it, the exposure should be bounded: cap the bonds at four and add a local action that forgets every
bond. Blocked by: the machine-alone build has no panel to show a code, and no spare input has been
chosen for a window — a jumper read at power-up is the only candidate so far.

# Battery Optional

We should stop assuming that the system has a battery. idk if this is a build time option or a flag of some sport

## Rename and delete a saved routine

The browser can create a saved routine but not rename or delete one; that is done over `fs` or by
shipping a new image. Blocked by: no gesture has been chosen for either, and a delete has to decide
what happens to a run that was started from the routine.

## Preview a saved routine by name

`--art` and `--scene` draw a definition by name; a saved routine is only reachable by opening the
browser first, and no wire command opens one either. Blocked by: the harness drives definitions, and
naming a saved routine on the command line needs a flag whose shape nobody has decided.