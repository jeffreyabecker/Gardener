# Should the panel be a build variant?

Every board this firmware serves carries a 128×64 panel, four keys, a piezo and the interpreter that
runs the disconnected pages. None of that is needed to hold a vacuum: a controller over serial or
Bluetooth can drive the whole machine. What is being weighed is whether a build without the screen,
keys, buzzer, routines and their assets should exist — a second sketch beside this one — so that a
device nobody owns is only a peripheral waiting to be given work.

Two things push the question now. The image is 566,997 bytes, 43% of the app slot
([parking lot](../plans/parking-lot.md)), and the panel, the fonts, the SH1106 driver, the VM and its
eight native modules are the largest block no command needs. And the alpha board keeps the OLED and
the key module as plugged parts, so a board with those headers empty is the same product minus two
connectors.

What is actually in doubt is not the mechanism — that is a second composition root, and the options
below say why — but whether a second shape of device exists at all, and what one does while nobody
owns it, because the panel is not decoration here. It is where the device's own stop lives.

## What the line would cut through

* **The composition root.** `buildAppContext()` news the display, the buttons, the buzzer player, the
  overlay and the VM, and hands them to the context; the sketch holds nothing else, because all of
  that lives in one place.
* **The boot's own failure rule.** The panel is the one part with no fallback: a panel that does not
  answer aborts, deliberately, rather than coming up blind and silent.
* **The control task's safety read.** The Back hold, OK during the hold and the Back+OK reset are
  read from the buttons inside the control task, which runs whether or not anything is drawn. This
  is the part that is not merely "the UI".
* **A whole task.** The panel task, its gesture queue, the overlay's link icons and one VM entry per
  pass.
* **The interpreter.** The Berry VM, `Routines`, and the eight bindings — including `screen`,
  `buttons` and `buzzer`, which would be native modules for parts that are not fitted.
* **The wire's own edge.** The `buzzer` domain calls the player directly; a build without one either
  refuses the command or reports a capability it does not have.
* **The data image.** `/routines/` and the mark are read only by the UI, so a headless data image is
  settings alone.

## Options

### A second composition root

The headless rig gets its own sketch and its own `buildAppContext()`, compiling the machine, the
sensor, pump and valve, storage, ownership and the two links, and nothing else. No `#if` anywhere,
and the saving is genuine: unreferenced code is not linked, so the panel stack, the VM, the fonts and
the SH1106 driver never reach the image. The alpha plan's rule — a board variant edits the build
function and `Config.h` and nothing under `hal/impl/` — already assumes this shape is possible.

The cost is the same coupling seen from the other side. The wire layer formats replies from the
context, so a second root needs either a second context type, after which the domains cannot be
shared unchanged, or a nullable context whose gaps every shared caller must respect. Two sketches
also mean two build tasks, two upload paths, and a data image question that the present single image
does not have.

### Delete the panel

The honest version of the same conclusion. If no one would ever run the UI build, there should be one
build and no panel, and the disconnected pages, the page standards in
[gestures](../specs/gestures.md), the routine and bindings specs, the asset tool and the data image
go with it. The alpha board still reserves the OLED and key headers, so this is a product decision
rather than a build one, and nothing here argues for it yet.

### Ruled out

**A define in the one sketch.** `-DGARDENER_UI=0`, alongside the NimBLE defines the build already
passes, would give one sketch and one build task, told apart by a define. The `#if`s would land where
they cost most, though: `Build.cpp` exists so that one file names the rig, and `AppContext` is a
complete graph
whose accessors are dereferences, so a context missing a part is either a null check at every call
site or a different type — and the call sites that would carry the checks are the wire domains and
the control task. It is also two products in one file, which is what the project's guidance asks a
flag to earn before it appears.

## The constraint that decides it

The device's own stop is a button. The Back hold stops the pump and vents from every page, with or
without a controller, and Back+OK is the only local way to clear an ownership record a vanished
controller left held. Both are read in the control task, not drawn on the glass — so "the UI" and
"the device's safety gestures" are the same code, and a build without keys has no local stop at all.

That would be tolerable if an unowned device were inert, and at rest it is: idle means the valve is
open and nothing is drawing. The problem is the run in between. A controller that drops while owning
changes nothing — the run continues and the ownership record stays held
([Bluetooth](../specs/bluetooth.md)) — and the firmware has no timeout, no maximum run time and no
watchdog by design; `HOLD` is a cue, not a countdown ([vacuum control](../specs/vacuum-control.md)).
Without keys, a chamber left pulling has no way back except reconnecting as the same controller or
cutting the power.

So a headless build has to answer what a held run does when its owner disappears, and the two answers
are both new requirements: release the chamber on link loss, which is the timeout the firmware
refuses today, or require an external stop and say so. Either way this changes a safety contract
before it changes a build, and that is the real question this idea opens.

## What would have to be true

* The saving is measured. Nobody has built the headless image; the parking lot's figures put the
  ceiling at roughly 269 KB — the current image down to an empty sketch — and the UI's share of that
  is unmeasured. Building in removal order — bindings, then VM, then panel, then keys and buzzer —
  answers it before either build exists.
* The unowned policy above is decided, and written where it belongs rather than in a build script.
* The wire says what the device has. A headless device cannot tone a piezo or read a key, so the
  `buzzer` domain and any button surface either refuse or are absent, and a controller has to be able
  to tell which device it is talking to.
* The data image is understood. Routines and the mark are UI-only, so the headless image is settings
  alone — which makes the "one file to flash" parking-lot item harder, not easier, because the two
  builds no longer want the same filesystem.

## What is still unknown

* Whether a panel-less unit is being built or only wished for. The alpha board keeps the OLED and the
  key module; nothing has said a board ships with those headers empty.
* Which is larger, the VM and its bindings or the panel stack. That is a measurement, not a guess,
  and the removal order would produce it.
* Whether image size is the motive at all. The parking lot already holds two levers that buy more
  bytes for much less design — `huge_app` pinning (about 18% of a 3 MB slot instead of 43% of the
  current one) and `-fno-exceptions` (about 36 KB of metadata) — so if size is the reason, a build
  variant is the wrong lever.
* Whether the routines are separable from the panel at all in the sense that matters: the pages draw
  through `screen`, so a headless build with routines would be an interpreter with no output. The
  options above assume routines leave with the panel; nothing forces that, but nothing needs it
  either.

## What I think

The headless build is a second composition root: its own sketch, its own `buildAppContext()`, with
the panel, keys, buzzer, VM and assets absent from the tree it compiles. Not a define, because this
project draws the line at the one file that names the rig and two rigs are two roots. That much is
decided, and the size measurement does not reopen it — the measurement decides whether the second
sketch earns its place, not what shape it takes.

What is not decided, and is why this is still an idea rather than a plan, is what the device does
when nobody owns it. A headless unit that can be left holding a chamber with no local stop is a new
safety contract rather than a smaller build, so until
[vacuum control](../specs/vacuum-control.md) and [gestures](../specs/gestures.md) say what a held run
does when its owner disappears, there is no requirement to write down.

Held firmly on the shape, and not at all yet on whether the build ships. If the point turns out to be
image size alone, take a parking-lot lever and leave the panel where it is.
