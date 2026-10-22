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
| `frame.c` | the panel: a 128 x 64 one-bit frame, the Adafruit GFX drawing verbs and the BMP writer |
| `frame.h` | the one function `main.c` calls to install the frame as a module |
| `harness.be` | the records and the eight native modules a routine compiles against |
| `driver.be` | the checks themselves: the drive, the editor, and the phases worth walking |

`check.py` builds `libraries/berry` with the firmware's own `berry_conf.h`, so the host VM has the
device's 32-bit ints, single-precision reals and `BE_STACK_TOTAL_MAX` slot limit. Objects and the
linked binary land in `tools/berry/build/`, which is ignored; a `.be` edit reuses them and only
compiles and loads.

## Running it

```powershell
python tools/berry/check.py                              # compile, load and drive the image
python tools/berry/check.py --compile                    # compile and load only
python tools/berry/check.py --art interval --seconds 60  # draw a page instead of driving it
```

| Flag | Default | Meaning |
| --- | --- | --- |
| `--compile` | off | stop after compiling and loading every file |
| `--data DIR` | `firmware/gardener/data` | another data image, such as a scratch copy |
| `--no-cache` | off | rebuild the vendored VM objects |
| `--gfx DIR` | the installed library | the Adafruit GFX library the fonts come from |
| `--art NAME` | — | draw that routine and save its frame instead of driving |
| `--seconds N` | `0` | with `--art`, press OK and run N seconds of virtual time first |
| `--sets N` | `12` | with `--art`, activate with N sets; `0` keeps the routine's own count |
| `--out PATH` | `build/<name>.bmp` | where the frame goes |
| `--verbose` | off | print each compiler command |

The last line says how much loaded — `12 of 12 files loaded, check passed` — and the exit code is 0
only when every file compiled, every registration looked like a routine, and every page ran.

## The load order

1. `harness.be`
2. `/modules/*.be`, alphabetical
3. `driver.be`
4. `/routines/*.be`, alphabetical

The order is not a preference. Berry resolves a global name when it **compiles** a file, so a file
may name only what an earlier file defined, and that reaches inside method bodies. A class a later
file needs has to be a global by the time the earlier file is compiled: `FixedText` in
`/modules/components.be` is used by the pages, so it is defined in a module, not in a page.

## What the harness provides

The records `reading`, `unit`, `bounds`, `event` and `setting`, and the eight modules a routine calls:

| Module | On the host |
| --- | --- |
| `vacuum` | a machine model: `state()`, `target()`, `start()`, `stop()`, `log()` |
| `screen` | every drawing verb forwarded to `frame`, real `text_bounds`, ink and unit |
| `buttons` | a queue: presses are pushed and delivered one beat at a time |
| `buzzer` | counts sounds rather than playing them |
| `device` | the unit, the jingle, and the reboot and reset requests |
| `jingles` | the library read from `/modules/jingles.txt` |
| `store` | `/settings.cfg` and every `/routines/*.cfg`, as text, with the type taken from the default asked for |
| `routines` | the registry: `register()`, `names()`, `set_active()` and the switch a page asks for |

The machine model is what a preview moves against. It starts at 0 kPa, pulls at 3 kPa/s, settles a
target when it is within 0.2 kPa of it, vents at 40 kPa/s, refuses a target below 5 kPa and caps at
60 kPa. One beat is 250 ms of virtual time.

## The frame and its pictures

`--art` runs the real page and saves what it drew as a one-bit BMP: 128 x 64, rows bottom up, a
palette of paper and ink. The drawing is not an approximation of the driver's — the primitives,
`drawChar`, `write` and `getTextBounds` are ports of the installed `Adafruit_GFX`, and the font data
is the library's own `glcdfont.c` and `Fonts/*.h`, found through `--gfx`, then `ADAFRUIT_GFX`, then
`arduino-cli`, then the usual sketchbook paths. A missing library stops the run with the
`arduino-cli lib install "Adafruit GFX Library"` hint rather than drawing in a stand-in face.

`gfx/Adafruit_GFX.h` is a shim that lets those font headers compile without `Arduino.h`.

Saving paints rows 52 to 63 with a one-pixel checker: that is where `Overlay.cpp` draws the panel's
band, over whatever a page put there, and a preview has to show it.

The frame holds one bit and no inversion, so it is the page as the display would draw it and not the
panel's finished face.

## What the drive does

For every registered routine, hidden pages included:

1. activate with no settings map, then eight beats
2. one OK, then 400 s of virtual time, so a cycle's phases and its repeat are reached
3. a long Back with the machine reporting a stop, then every button and press pair, then the Up+Down
   chord — the page has to hand the panel back and survive all of it

The settings editor is walked first, through its whole key sequence.

Four checks are not a walk and are named for it: the jingle library, the System page's jingle editor,
the interval's set count and its "after ok" pause, and the ramp's three stages, its pause and its
resume.

## Failing

A file that will not compile or load is reported the way the firmware reports it and the rest still
load:

```
[E] vm: routines/interval.be compile: interval.be:3: 'Unknown' undeclared
```

A check that fails says what it expected, prefixed with the page it was driving, and the run ends
with `check failed`. The VM's own failures — a missing routine, an unregistered name — are errors of
the image, not of the harness.
