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

Pin `huge_app` instead of `default` in [scripts/arduino.py](../scripts/arduino.py): one 3 MB
app slot and 896 KB of LittleFS reports ~18% instead of 43%. The OTA slot it drops is unused, and
the data image has to be uploaded again because the offset moves.

### Try `-fno-exceptions`

The image carries 36 KB of exception metadata, about 17 KB of it in objects this build compiles.
Nothing throws today, but any vendored C++ added later would have to make the same choice.

## The board's own free heap

The Berry check reports the interpreter's heap as the host bills it, which is an upper bound rather
than the board's figure. What the device actually has free, and how low a run takes it, is a number
only the board can answer. Blocked by: the serial protocol has no field for it, and nothing has
decided whether one belongs there.

## Loading routines on demand

Compile a routine when it is pushed rather than at boot, so only the active page's compiled form
bills the heap — about 25 KB on the board. Blocked by: the browser lists routines from the registry,
so an un-loaded routine has no row, and nothing has decided whether that is acceptable.

## One file to flash

A `--task release` that packs [firmware/data](../firmware/data) into the build's
`gardener.ino.merged.bin`, so a device is flashed at `0x0` in one esptool command instead of
`--task upload` and then `--task data`. What holds it back is that the image would ship whatever
`settings.ini` and routines are in the tree and clear NVS on the way, which nobody has decided on.



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

## An overlay that adapts to the panel

The page area above the band reflows to the panel's size, but the band is still a fixed 12 rows.
Blocked by: nothing decided about whether the band should scale with the panel or stay sized to its
own contents.

## Berry drawing through a canvas

Every `.be` page draws through the flat `screen` verbs onto the one full-page canvas the panel owns,
so a component's drawing is only inside its box by discipline and there is nowhere for a window over
the page to draw. A canvas over an extent, handed to a scene or a window, would clip each one to its
own box and let a window compose over what is under it. Blocked by: nothing in the scene stack is a
window — every scene takes the whole page — and `screen` is a published surface, so the extent, the
buffer and the flush order a canvas would need are all undecided.

# Battery Optional

We should stop assuming that the system has a battery. idk if this is a build time option or a flag of some sport