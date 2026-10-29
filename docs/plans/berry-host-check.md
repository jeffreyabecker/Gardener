# The Berry host check

## What this is

`python tools/berry/check.py` is the only place the data image runs. It compiles the interpreter's
host side, loads `/system` and `/routines` the way the device does, drives the pages through the
modules they call and holds the suites under `tools/berry/tests/`. It has not compiled since the file
store was folded into `Config` and moved under `hal`, and it has not driven a page since the bindings
were split out of one file; the two failures are independent, and each hides the other.

Its own build stops first. `FIRMWARE_SOURCES` in `check.py` names
`libraries/Gardener/src/storage/Ini.cpp` and `storage/Jingles.cpp`, and neither file exists, so the
first `.be` file is never reached; `BERRYHOST_SOURCES` names seven of the ten sources in
`libraries/BerryHost/src`, leaving out `Routines.cpp`, `Settings.cpp` and `Ux.cpp`, so a build that
got past the missing files would fail to link on the three installers they carry. Behind that,
`harness.be`, `driver.be` and `suite.be` name a surface the tree has retired — `routines.keys`,
`ux.depth`, `ux.top`, `overlay_band_height`, `EditCfg`, `Scene`, `vacuum.now_ms`, `vacuum.target`,
`screen.draw_clear` and five component classes — and three suites assert a `chrome` field no page
keeps since pages began answering `draw_page()`.

This plan makes the check build itself from the libraries that exist, speak the modules that exist,
and fail when it drove nothing; and it states which suites the check has to hold, because for most of
the Berry boundary a suite is the only observation there is. It leaves the device's own verification
alone — upload, the panel, the `[E]` lines — and it does not restate the heap and refusal
requirements already written against the marshaling.

## The check compiles the libraries that exist

The two source lists in `check.py` are hand-written statements of each library's file split, so every
commit that moves a file is a silent break, and they no longer track the tree: `storage/Ini.cpp` and
`storage/Jingles.cpp` are folded away, and `installUxModule`, `installSettingsModule` and
`installRoutinesModule` live in files the list never gained.

What is true instead: the sources the check compiles are discovered, not listed —
`libraries/BerryHost/src` in full, `libraries/Gfx/src` in full, and the part of
`libraries/Gardener/src` the harness's own composition reaches, selected by what a source needs
rather than by its name, as `tools/unit/check.py` already selects its own. Adding a source to one of
those libraries or taking one away is not an edit to `check.py`. The object cache, the header-time
rule and `--no-cache` keep the behaviour they have.

Checked: with `tools/berry/build/` deleted, `--compile` compiles, links, loads every file the image
names and prints its load line; a `.cpp` added to `libraries/BerryHost/src` is compiled by the next
run with no edit to `check.py`.

## A run that drove nothing fails

`main.cpp` counts three things that can go wrong — a harness or driver file that does not load, a
suite file that does not load, and the driver's own error count — and exits 0 when all three are
zero. None of them is the image. `Routines::begin` already states what it loaded (`vm: 3 system, 3
routines from 6 files`) and `loadFile` already answers false for a file that did not compile, but
nothing consults either: an image whose every `.be` file failed to compile still resets to the
mark, registers nothing, and the run prints `[E] vm: /system/home.be compile: ...` above a last line
of `0 routines, check passed` and exits 0. The error line is written and no exit code reads it, which
is how this tool came to be described as broken while looking, in a log, like a run of failures that
the change under test had caused.

What is true instead: the last line of a run reports the files loaded of the files found and the keys
driven of the keys registered, and the exit code is 0 only when every file `/system/order` names and
every file in `/routines` compiled, every registration answered a class, and every registered key was
activated and polled for at least one beat. A file that did not load, a key that answered no page and
a `.be` file nothing names are each a failure with the path or the key in the line.

Checked: an image file with a deliberate syntax error makes the run exit non-zero naming that file;
so does a registration naming a class the file does not declare; so does a `.be` file dropped into
`/system` and left out of `order`.

## The registry answers its keys

`RoutineRegistry::keys()` exists and answers every registered key in registration order, with each
alternate after its record, but the `routines` module has no `keys` entry, so both `harness.be` and
`driver.be` raise `attribute_error` on it and the driver's walk iterates nothing. The routine count
the run prints comes from that same call, so it prints zero whatever the image registered.

What is true instead: `routines.keys()` answers every registered key in registration order as a list,
alternates after their record, and an empty list — not nil — for an image that registered nothing. The
count `main.cpp` prints is the length of that list, so `3 routines` is a statement about the image.

Checked: a run reports 3 keys for the image as it stands (`ramped_interval`, `interval`,
`ramped_static`), in that order; a suite that registers a routine of its own adds one to the count it
reads after registering and not to the count read at load.

## The stack can be observed

`gfx::ui::Ux` answers `depth()` and `top()`, and the `ux` module binds neither: `ux.push`, `ux.pop`
and `ux.pop_to_root` are the whole of it. Six calls across three suites read `ux.depth()` and three
more read `ux.top()`, so the machine suites cannot say that a finished run handed the panel back and
the page suites cannot reach the page they just pushed to put a mock in it.

What is true instead: `ux.depth()` answers the number of frames showing, which is 1 at boot and after
every reset, 0 with nothing showing, and never more than 8 (the stack's `MaxFrames`), and
`ux.top()` answers the page the top frame holds — the instance the image built for a Berry page, so a
case can set a field on it and call its methods; nil for a scene a native factory answered; nil with
nothing showing. Both are readable from a case and from the driver, and neither raises.

Checked: after `harness_activate`, `ux.depth()` is 2 and the instance `ux.top()` answers is the class
the record registered; `ux.push(nil)` answers false and leaves the depth alone; nine pushes leave the
depth at 8; after `pop_to_root` the depth is 1 and `ux.top()` is the home page.

## The drive names what exists

Six names the harness and the suites use have no declaration in the tree: `overlay_band_height`
(`suite.be`, for the band the ux draws, now `gfx::ui::BandHeight`), `EditCfg` (`driver.be`, for the
declaration record, now `RoutineSetting`), `Scene()` (both machine suites, for a frame to push above
the root), `vacuum.now_ms()` each beat and `vacuum.target(kpa)` in the driver's scene art and the
interval machine suite, and `screen.draw_clear()` plus five component classes (`Text`,
`PressureDisplay`, `ProgressBar`, `Scrollbar`, `Duration`) in `harness_art_component`. The clock is
`device.now_ms()`; the target is `vacuum.set_target_pressure`; the drawing surface is the draw tree,
which has no component classes at all.

What is true instead: a suite reads the clock the image reads (`device.now_ms()`), sets a pressure
through the call a page sets one through, tests a declaration with `isinstance(item, RoutineSetting)`,
and gets the band's row count from the stack that draws it rather than from a global. The framework
declares the page the machine suites push above the root, so `Scene()` is one place and not five.
`--component` is deleted with `harness_art_component`, `harness.save` and the `artifacts/components`
folder: a component with no Berry name has no picture to take, and `--art` and `--scene` are the two
ways to a picture. `check.py`'s own examples name routine keys that exist.

Checked: `python tools/berry/check.py` compiles the harness, the driver and every suite with no
undeclared name and no missing member; `--art ramped_static --seconds 30` and `--scene browser` each
write a GIF; `--component bar` is refused by the argument parser.

## A page's drawing is read from the tree it answers

Five assertions in three suites read `page.chrome.<node>` — `home.chrome.timer`,
`page.chrome.sets`, `page.chrome.status`, `page.chrome.bar` — and no page has kept a `chrome` field
since pages began answering `draw_page()`. `chromes.test.be` is the only suite that reads a tree, and
it keeps the `child_at` helper to itself. One case in it is also wrong about the clock's unit:
`home_chrome` takes seconds and the case hands it 65 000, comparing the result against the string a
65-second clock reads. The two chrome functions are not of one mind either — `home_chrome` takes the
run's seconds and `ramped_static_chrome` takes its milliseconds — so the same run reads two different
strings depending on the page showing.

What is true instead: a page's drawing is asserted by calling `draw_page()` on the instance and
reading the nodes it answers, through one `child_at` the framework declares; a case asserts the node
kinds and values the page depends on, never a field the page does not have; and the device's clock
reads the same string whichever page draws it, so home's 65-second run and the static profile's
65 000-millisecond run both draw what `duration_text` answers for 65 seconds.

Checked: the three page suites' drawing cases pass against `draw_page()`; the idle home page and the
ready static page are asserted to draw `--` for a clock that is not running; and the two clocks are
asserted equal for the same elapsed time.

## Every page the image registers has a suite

Two of the image's pages have none. `browser.be` has no suite at all, and it is the only path that
launches a routine — the rows it reads from the registry, the selection that wraps, the window's first
row and the settings list its Forward opens are all unobserved. `3_interval.be` registers `interval`
and has no page suite, so the routine whose page extends another's is the one nothing drives.

What is true instead: every registered key has a suite, and the browser has one of its own. The
browser's cases assert the rows it reads from the registry, the selected row inverted and no other,
the first row the window shows once the selection passes the bottom edge, OK launching the record's
page, Forward opening the list and launching once the list settles, and Back leaving. The interval's
cases assert its own declaration set, its inherited chrome and its flat ladder, which the ramp refuses
and it does not.

Checked: `routines.keys()` and the suites that set `suite` name the same three keys, and the browser
suite drives a launch from the rows to a page on the stack and back.

## Every module a page calls has a suite

Four modules a page calls have no case of their own: `screen`, `settings`, `routines` and `ux`. What a
page depends on there is the answer across the boundary, and `tools/unit/check.py` cannot see it: the
records, the draw tree and the modules are `libraries/BerryHost`, which needs the interpreter to
compile, and `tools/unit` does not compile it.

What is true instead: one suite per module, each asserting the answer a page relies on. `screen`
answers a node of the kind it names with its fields filled, `measure` answers the ink a string takes,
`duration_text` answers the string a clock reads and `--` for one that is not running, and `icon`
refuses a string that is not a bitmap. `settings` answers a `RoutineSetting` per declaration with the
bounds it was declared with, consumes a declaration once so a second record taking the same row fails,
and raises without changing the first record's rows for a list holding anything else. `routines`
answers keys, rows, a nil title for an unknown key, nil `settings_cfgs` for a record that declares
nothing, an activation that fills a missing setting with its default and refuses a value with a
message naming the key and the label, and a save that writes each key once and a key the map leaves
out as nothing. `ux` answers whether a page is showing from each of its three calls, refuses a handle
pushed twice, hands a popped page's value to the callback it was pushed with, and runs no callback
from `pop_to_root`.

Checked: each suite runs green under `--tests`, and each case fails when the answer it asserts is
changed in the module — which is the whole of what the suite is for.

## A drawn frame can be asked what it holds

A page answers a tree and `DrawTree.cpp` turns it into paints; nothing asserts the walk. A case can
read the nodes a page answers but not what they drew, so the two things that only the walk decides —
a node that names no size filling the cell the layout gave it, and `screen.inverted` flipping the
whole cell its parent resolved — are taken on trust, and the captures are left for a person to look
at rather than a case to read.

What is true instead: the drive answers the ink a rectangle of the frame holds, as a count of set
pixels, so a case states what a node drew in the box it was given. The cases are an inverted leaf over
a full cell, which is paper where the child drew and ink where it did not; a child that names no size,
which fills its cell; and a text node, whose ink is the width `screen.measure` answers for the string.

Checked: a suite draws a two-cell row through a page, one cell inverted, and asserts the ink of each
quarter of the frame; the count for the text case equals the width `measure` answers for the same
string at the same face.

## Consequences

`.github/skills/berry/SKILL.md` returns to documenting this tool: the note that it is broken and must
not run goes, the commented-out harness notes come back, and the line the notes describe as the last
line of a run is corrected to the counts this plan has the run print. The project instructions that
repeat "the Berry check is in a known broken state" go with it.

`docs/parking-lot.md` is unblocked in two places rather than changed: "A spec for the image's check"
no longer waits on a tool that does not run, and "A battery that is not fitted" no longer waits on a
check that cannot host the allocator it needs. Neither entry is done by this plan.

The host checks the marshaling requirements ask for — a page that is opened and closed holding
nothing, every native object released when the VM goes, a refused call moving neither heap — become
takeable, and this plan leaves them to be taken.

The build gains `routines.keys`, `ux.depth`, `ux.top`, the stack's band row count and a frame-ink
reading on the harness; it loses the `--component` flag, `harness.save` and `harness_art_component`.

Nothing in `docs/specs/` changes, because nothing there describes this tool — which is the parking
lot's complaint, not this plan's to answer.

## Deferred

- The spec for the check, the board's free heap, and the allocator that fails a chosen allocation.
  All three are parking-lot entries, and this plan only stops blocking them.
- Anything the host cannot answer: the pump, the sensor, the radio and the real timing stay models or
  counters, and the device keeps its own verification.
- The `Runtime` composition and the machine-alone build. Nothing here compiles them, and the check
  does not become a second statement of which firmware variant is being built.

## How it is known to hold

1. `python tools/berry/check.py --compile` compiles, links and loads the image, and prints the files
   loaded of the files found.
2. `python tools/berry/check.py` drives every registered page, reports the keys and the files, and
   exits 0 with `check passed` only when the counts agree.
3. `python tools/berry/check.py --tests` runs every suite, writes one capture per case, and exits 0
   with every case green.
4. `python tools/berry/check.py --art ramped_static --seconds 30` and `--scene browser` each write a
   GIF, and `--component bar` is refused.
5. `python scripts/arduino.py --task build` compiles the new bindings and the stack's band count.
6. `python tools/unit/check.py` is unchanged and still green: nothing this plan adds is in its
   inputs.
