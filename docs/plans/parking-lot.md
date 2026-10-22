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

## One file to flash

A `--task release` that packs [firmware/gardener/data](../../firmware/gardener/data) into the build's
`gardener.ino.merged.bin`, so a device is flashed at `0x0` in one esptool command instead of
`--task upload` and then `--task data`. What holds it back is that the image would ship whatever
`settings.ini` and routines are in the tree and clear NVS on the way, which nobody has decided on.

## Nudges inside a routine

A gesture that adjusts the run while it is going instead of ending or restarting it: deeper by a step,
one more set, or straight to rest and done. What holds it back is the gesture map — OK is the page's
primary action, Up+Down is the settings view of any page with `describe()`, and Back is the whole stop —
so a nudge has no press to take that does not first take a meaning away from one of those.


## Verify the two builds on hardware

The machine/panel split, the ownership stack and the two builds have only ever been
compiled. Everything the specs now promise still needs a device: the full device's serial
and Bluetooth surfaces with `control get` naming the owner, the panel driving by default
with Back releasing and Back+OK rebooting, and the chamber venting on a claim, a release
and a drop. The machine-alone build needs the same: `device get` reporting `caps` and no
battery, log or unit field, `fs` and `buzzer` answering `unknown_domain`, and a run that
cannot start unowned or survive its owner leaving.

## Berry pages as a stack of components

A page can hold exactly one editor (`Page.editor`), so the routine settings module in
[modules/settings.be](../../firmware/gardener/data/modules/settings.be) carries the list, the edit
stage and the commit for a routine's `describe()` list, and the device's own settings screen in
[routines/system.be](../../firmware/gardener/data/routines/system.be) hand-rolls a second one for
unit, target, Bluetooth and jingle. The idea is a stack instead of one field: a value editor is
pushed with a current value, its kind and bounds, takes the keys and the screen while it is on top,
and pops with the value it settled on, so routine settings becomes a composition of several of them
and the app's own settings screen can invoke the same ones. What holds it back is the page lifecycle
and the gesture meanings: Back is pinned to the level — it closes the editor, cancels an edit, or
stops the run — and `draw()` chooses between the one editor and `draw_page()`, so a stack has to
settle who draws underneath the top and what a pop hands back.

## Delete the store bus

[StoreBus.h](../../libraries/PanelLink/src/StoreBus.h) is a queue between the VM and the
interface task, and both ends are now the panel task — the VM polls and the server drains on the same
beat — so it is the panel queueing to itself. The idea is to delete `StoreClient`, `StoreServer` and
the queue handle, and let the `store` binding and `Routines` reach `Storage` and `Settings` directly,
which also drops the result slot and the notify. What holds it back is the `fs` serial domain, which
still touches `Storage` from the interface task, so the queue is not yet the only thing keeping two
tasks out of LittleFS.

## Gesture Classifier

gestures become either an enum or a string with a predictable format state machine functions become a
lot less nested. maybe expose this at the c++ level.

# Battery Optional

We should stop assuming that the system has a battery. idk if this is a build time option or a flag of some sport