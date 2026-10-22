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

The machine/panel split, the ownership stack and the two builds have only ever been
compiled. Everything the specs now promise still needs a device: the full device's serial
and Bluetooth surfaces with `control get` naming the owner, the panel driving by default
with Back releasing and Back+OK rebooting, and the chamber venting on a claim, a release
and a drop. The machine-alone build needs the same: `device get` reporting `caps` and `log` with no
battery or unit field, `fs` and `buzzer` answering `unknown_domain`, and a run that
cannot start unowned or survive its owner leaving.

# Battery Optional

We should stop assuming that the system has a battery. idk if this is a build time option or a flag of some sport