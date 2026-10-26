# Berry harness

The data image under `firmware/gardener/data` is compiled by the device at boot, so `arduino.py`
never sees it: a typo in a `.be` file costs a build, an upload and a serial console to find. The
harness in [tools/berry/](../../tools/berry) compiles and runs the whole image on the host instead,
against the vendored VM the firmware ships, and exits non-zero when a file fails to compile, a
registration does not look like a routine, or a page does not survive being driven.

## The parts

| File | What it is |
| --- | --- |
| `check.py` | the build and the command line: compiles the VM, links the host app, runs it |
| `main.c` | loads the files in the device's order and calls the checks |
| `frame.c` | the panel: a 128 x 64 one-bit frame and the Adafruit GFX drawing verbs |
| `frame.h` | the one function `main.c` calls to install the frame as a module |
| `harness.be` | the driver's bookkeeping: the log, the error count, and the routine count the summary prints |
| `driver.be` | the smoke walk: the drive, the settings walk, and the scene re-entry |
| `tests/suite.be` | the suite framework: `test()`, the assertions, `wait_until`, the runner |
| `tests/*.test.be` | the suites, one file per concern, each registering named cases |

`check.py` builds `libraries/berry` with the firmware's own `berry_conf.h`, so the host VM has the
device's 32-bit ints, single-precision reals and `BE_STACK_TOTAL_MAX` slot limit. Objects and the
linked binary land in `tools/berry/build/`, which is ignored; a `.be` edit reuses them and only
compiles and loads.

## Running it

```powershell
python tools/berry/check.py                              # compile, load and drive the image
python tools/berry/check.py --compile                    # compile and load only
python tools/berry/check.py --art interval --seconds 60  # animate a page instead of driving it
python tools/berry/check.py --scene browser              # draw one of the device's own scenes
python tools/berry/check.py --tests                      # run the test suites, a GIF per case
python tools/berry/check.py --test ramped_static         # run one suite (or part of one)
```

| Flag | Default | Meaning |
| --- | --- | --- |
| `--compile` | off | stop after compiling and loading every file |
| `--data DIR` | `firmware/gardener/data` | another data image, such as a scratch copy |
| `--no-cache` | off | rebuild the vendored VM objects |
| `--gfx DIR` | the installed library | the Adafruit GFX library the fonts come from |
| `--art NAME` | — | draw that routine and save an animation of it instead of driving |
| `--scene NAME` | — | draw that device scene — `home`, `browser` or `system` — and save an animation |
| `--seconds N` | `0` | with `--art` or `--scene`, run N seconds of virtual time first |
| `--sets N` | `12` | with `--art`, activate with N sets; `0` keeps the routine's own count |
| `--ceiling-kpa KPA` | `70` | the deepest depth the model chamber reaches |
| `--out PATH` | `build/<name>.gif` | where the GIF animation goes |
| `--tests` | off | run the test suites instead of the smoke walk |
| `--test NAME` | — | run only the cases whose full name contains NAME (implies `--tests`) |
| `--verbose` | off | print each compiler command |

The last line says how much loaded — `12 of 12 files loaded, check passed` — and the exit code is 0
only when every file compiled, every registration looked like a routine, and every page ran. In
`--tests` mode the same contract holds for the suites: `13 passed, 0 failed`, with `suites passed`.

## The load order

1. the image, in two parts:
   1. the files `/system/order` names, in the order it names them
   2. `/routines/*.be`, alphabetical
2. `harness.be`
3. `driver.be`

The order is not a preference. Berry resolves a global name when it **compiles** a file, so a file
may name only what an earlier file defined, and that reaches inside method bodies. A class a later
file needs has to be a global by the time the earlier file is compiled.

That is what `/system` is: the records the native calls answer with, the components and the scene
every page is written against, the settings view, and the registry. Its files do not sort into their
order, so `/system/order` states it — a path per line relative to `/system`, `#` for a comment, and
`components/text.be` beside `scene.be` for a file in a subfolder. A `.be` file in one of the folders
the order names, and not named by it, is reported as an error and not loaded. The routines load after
it, alphabetically.

In `--tests` mode two more load after those three: `tests/suite.be`, then `tests/*.test.be` in
alphabetical order. A suite can name every routine, module and `harness` drive because the whole
image is already compiled by then.

## What the harness provides

The records `reading`, `unit`, `bounds` and `jingle` — declared in `/system/records.be`, which the
image loads before anything else — and the three native modules a routine calls:

| Module | On the host |
| --- | --- |
| `vacuum` | a machine model: `state()`, `target()`, `start()`, `stop()`, `log()` |
| `screen` | every drawing verb forwarded to `frame` with its ink-or-paper colour, real `text_bounds`, and the unit |
| `device` | the unit, the jingle selection and library, the piezo counting sounds rather than playing them, a press queue, the reboot and reset requests, and `device.config` over `/cfg`, with `harness.writes()` counting the writes |

The registry the browser draws and the scene stack it pushes onto are not native: `/system/registry.be`
defines the global `routines` instance and `/system/ux.be` the global `ux`, which the driver and the
suites reach the same way a scene does.

The machine model is what a preview moves against. It starts at 0 kPa, pulls at 3 kPa/s, settles a
target when it is within 0.2 kPa of it, vents at 40 kPa/s, refuses a target below 5 kPa and caps at
the ceiling `--ceiling-kpa` sets, 70 kPa by default. One beat is 250 ms of virtual time.

## The frame and its pictures

`--art` and `--scene` run the real page and save what it drew as an animated GIF, one frame per
beat at the panel's 128 x 64, paper and ink, each held for a second so the run reads slowly. The
GIF is enlarged 2 x to 256 x 128 by `kRenderScale` in the GIF writer — nearest neighbour, because
the picture is one bit. The frame delay is `kGifDelayCs`, one line to change to slow the whole
animation down or speed it up. The drawing is not an approximation of the driver's — the primitives,
`drawChar`, `write` and `getTextBounds` are ports of the installed `Adafruit_GFX`, and the font data
is the library's own `glcdfont.c` and `Fonts/*.h`, found through `--gfx`, then `ADAFRUIT_GFX`, then
`arduino-cli`, then the usual sketchbook paths. A missing library stops the run with the
`arduino-cli lib install "Adafruit GFX Library"` hint rather than drawing in a stand-in face.

A captured GIF also carries the gesture that drove it: each press the driver injects is shown as a
full-width strip across the top of the frame it lands on — the key and its form, `OK`, `OK×2`,
`CANCEL 5s` — so a tutorial can show the button as the viewer watches the run. The strip is a
host-only callout drawn over the captured frame; the page and the band never see it. It stays
for one beat, behind `kGestureCalloutBeats` in `tools/berry/host/GestureCallout.h`, which is the
one line to change to keep the key on screen longer.

`gfx/Adafruit_GFX.h` is a shim that lets those font headers compile without `Arduino.h`.

Saving keeps the band the `ux` component draws: rows 52 to 63 are the device's own status row — the
unit, the serial and Bluetooth icons, the run-state icon and the battery — over whatever the page
put there.

The frame holds one bit and no display inversion, which is the panel's own depth and face.

## What the drive does

For every registered routine:

1. push with no settings map, then eight beats
2. one `ok:single`, then 400 s of virtual time, so a cycle's phases and its repeat are reached
3. an external stop notice, then every key's `single`, `up:double` with `back:single` to close the
   settings list it opened, and `cancel:double` — the routine has to survive all of it and hand the
   panel back

The settings view is walked first, through its whole key sequence: the drive opens it the way a
caller does — reading every value and applying the routine's rule — edits every row, and closes it
with Back, which is when the caller writes the set and the drive checks that every key landed.

## The test suites

The named checks live in `tests/*.test.be`, outside the data image so `arduino.py --task data`
never packs them. Each file sets `suite` to its own name, registers cases with `test(name, fn)` and
uses the helpers from `tests/suite.be`:

| Helper | What it is |
| --- | --- |
| `test(name, fn)` | register a case; `fn` is a function taking no arguments |
| `assert_true(cond, msg)` | raise when `cond` is false |
| `assert_eq(actual, expected, msg)` | raise when the values differ, with both in the message |
| `assert_nil(x, msg)`, `assert_not_nil(x, msg)` | raise on the wrong nil-ness |
| `wait_until(label, beats, pred)` | beat the harness until `pred()` is true, or raise naming `label` |

The runner executes every registered case (or those whose full `suite.case` name contains the
`--test` filter), each from a reset page, and reports one line per case: `[PASS] suite.case` or
`[FAIL] suite.case: assert: ...`, then `N passed, M failed`. A case that raises does not stop the
rest, and the run ends `suites passed` only when nothing failed.

Every case is also captured as an animation named `<stem>.<suite>.<case>.gif` — the stem is the
`--out` value with its `.gif` stripped, defaulting to `build/runs` — so a failing case leaves its
motion behind. A case that never beats has nothing to animate and writes nothing.

The suites cover what a walk cannot name: the jingle library, the System scene's unit, radio and
jingle rows, the interval's set count and its "after ok" pause, the ramp's three stages and its
pause and resume, and the registry's hidden-routine rule.

## Failing

A file that will not compile or load is reported the way the firmware reports it and the rest still
load:

```
[E] vm: routines/ramped_interval.be compile: ramped_interval.be:3: 'Unknown' undeclared
```

A check that fails says what it expected, prefixed with the page it was driving, and the run ends
with `check failed`. A failing suite case is a `[FAIL]` line carrying the assertion, and the run
ends `suites failed`. The VM's own failures — a missing routine, an unregistered name — are errors
of the image, not of the harness.
