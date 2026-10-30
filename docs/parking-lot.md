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



## Verify the device on hardware

The machine/panel split has only ever been compiled. Everything the specs now promise still needs a
device: the serial and Bluetooth surfaces with writes accepted from any link, `device get` reporting
the gauge and the unit beside the machine's fields, the panel's page and routine live whatever else
is connected, and Back+OK rebooting.

## Thermal and continuous-duty protection

`pump_max` is a duty cap, not a thermal limit: a long hold at the top of the range can overheat the
pump. A maximum continuous run, a derating curve, or a current or temperature sense would cover it.
Blocked by: nothing on the rig senses temperature or pump current, and neither pump has a measured
continuous-duty figure to set a body-safe default from. A single hold is now bounded at fifteen
minutes, which is the whole of the protection there is.

## Low-battery start refusal

`startRefusal()` should answer `low_battery` when a gauge is fitted and the pack is below a
threshold, so a brownout is refused rather than survived. Blocked by: no pack fitted to the device
yet — the alpha PCB leaves the divider deferred until a Super Mini variant with one is chosen.

## A local event log

The last N commands, stops, transitions and faults kept in NVS and readable over the serial
line and the Bluetooth diagnostics, so an incident can be understood after the fact. Blocked by:
nothing decided about where it lives, how it is bounded, or what reads it back.

## Pairing

Just Works is proximity rather than authorization: anyone in radio range of an advertising machine
can pair and command pressure, and bonds are uncapped. Until a pairing window or passkey replaces
it, the exposure should be bounded: cap the bonds at four and add a local action that forgets every
bond. Blocked by: no spare input has been chosen for a window — a jumper read at power-up is the
only candidate so far.

## An overlay that adapts to the panel

The page area above the overlay reflows to the panel's size, but the overlay is still a fixed 12
rows. Blocked by: nothing decided about whether the overlay should scale with the panel or stay
sized to its own contents.

## The overlay as cells the panel hands out

`glass::Overlay` reads the unit, the links, the run state and the gauge itself and builds the
overlay's four paints in a fixed order, so what the overlay shows is decided in one paint and a
device with no gauge or no radio still contributes a cell that draws only the absence. Each cell
could be an object the panel context returns — the unit, the links, the run state, the gauge —
drawn in the order it is given them, so a fitted part adds its cell and a missing one is not there
at all. Blocked by: the strip is one paint rather than a set of parts, so the container, the order
and which side re-reads the machine each frame are all undecided.

## Berry drawing through a canvas

Every `.be` page draws through the flat `screen` verbs onto the one full-page canvas the panel owns,
so a component's drawing is only inside its box by discipline and there is nowhere for a window over
the page to draw. A canvas over an extent, handed to a scene or a window, would clip each one to its
own box and let a window compose over what is under it. Blocked by: nothing in the scene stack is a
window — every scene takes the whole page — and `screen` is a published surface, so the extent, the
buffer and the flush order a canvas would need are all undecided.

## The canvas as a property of the display

`PanelContext::canvas()` has to be there because a `Canvas` owns the rectangle of pixels written
since the last flush and `DisplayDevice::rootCanvas()` builds a fresh one on every call: the panel
holds the one root and hands out a reference to it. If the device held its root canvas and answered
with a reference, `canvas()` could leave the context and both headers would describe what actually
happens.
Blocked by: a canvas needs the buffer, the geometry and the depth, which are virtual and exist only
once the driver is up, so a base-class member cannot be built in the constructor; and `Canvas` is
neither copyable nor movable, so a getter cannot hand one out by value or keep one without an
`optional` or a member the driver owns.

## The stored default target

`device.target()` reads `[device] target` and the home page uses it as the depth a run works to,
but nothing in the firmware writes the key any more: `device.set_target` went with the panel's other
dead write, the shipped `settings.ini` does not carry it, and the wire's `device set` takes only the
unit. So it is a value a hand-edited or reflashed document sets and nothing else can. Blocked by:
nothing has decided whether that is the shape it should have, or whether the key, the getter and the
three lines in `home.be` should go with the writer.

## The Berry host library's dependency

The library holding the interpreter's install, its six native modules, the page scene and the draw
tree compiles against [libraries/Gardener](../libraries/Gardener) for the machine's bounds, the
published state and the buzzer — more than `PanelContext` carries. A surface declared in that library
and implemented by the firmware would let it build with only the vendored VM and Gfx beside it.
Blocked by: the `vacuum` module reaches past the context into `machine/Machine.h` for the two target
bounds, and `device` into `panel/parts/BuzzerPlayer.h` for the piezo's actions, and nothing has
chosen one shape for a reading and an action.

## A spec for the image's check

The host check compiles the image, drives its scenes and writes its captures, and nothing states
what it loads, what it drives or what it writes. Blocked by: the check is in a known broken state, so
a spec written today would describe a tool that does not run.

## One file to compile

A task in [scripts/arduino.py](../scripts/arduino.py) that keeps a verbose build's compile command
and replays it with `-fsyntax-only` for one file, so a library change is checked in seconds rather
than by a whole build. Blocked by: nothing has decided whether the core library's host test suite
answers the same question for the source it compiles, which would leave the task unused.

## A battery that is not fitted

The panel build sets BatteryFitted whatever the assembled board carries, so a machine with no gauge
still reports one. Blocked by: nothing has decided whether that is a build option or a run-time flag.

## An allocator that fails on demand

`be_newcomobj` stores the object it is handed only if its own allocation succeeds, and frees nothing
when it does not, so an owner or a scene is dropped silently at the moment memory is already short. A
host build whose allocator fails a chosen allocation would show that and every other path that hands
ownership over in an argument list. Blocked by: nothing builds one, and the interpreter's check that
would host it is in a known broken state.