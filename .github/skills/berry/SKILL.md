---
name: berry
description: 'Checking the Berry data image under firmware/gardener/data without flashing. Use when writing, changing, debugging or reviewing a .be routine or module, or a C++ binding under routines/bindings/: compile, load and drive the whole image on the host with the vendored VM, the eight native modules, the records and the settings editor. Covers the load order, the surface the test app must stub, and the failures each message means.'
---

# Berry

## When to use

- Editing anything under `firmware/gardener/data/routines/` or `firmware/gardener/data/modules/`
- Editing `routines/Bindings.cpp`, `routines/Routines.cpp` or a file under
  `routines/bindings/`
- A page misbehaves on the panel and you want the answer without a build and a flash

A `.be` file is compiled by the device at boot. Nothing in the repo checks it: `arduino.py build`
never sees it, and `--task data` only packs it. A typo costs a compile, an upload and a serial
console, which is why the check below exists.

## Run it

```
python .github/skills/berry/check.py              # compile, load and drive the image
python .github/skills/berry/check.py --compile    # compile and load only
python .github/skills/berry/check.py --data DIR   # another data image, e.g. a scratch copy
python .github/skills/berry/check.py --no-cache   # rebuild the vendored VM objects
python .github/skills/berry/check.py --art interval --seconds 30
```

The first run builds `libraries/berry` for the host into `.github/skills/berry/build/` and links
[main.c](./main.c) and [frame.c](./frame.c). The objects are cached; a check after a `.be` edit only
loads and drives. It uses the firmware's own `berry_conf.h`, so 32-bit ints, single-precision reals
and the `BE_STACK_TOTAL_MAX` slot limit are the ones the device has. Exit code 0 means every file
compiled, every registration looked like a routine, and every page ran.

`--art NAME` draws instead of driving: it runs that routine and saves the frame it left as a one-bit
BMP, `build/<name>.bmp` unless `--out` says otherwise. `--seconds N` presses OK first and runs N
seconds of virtual time, so a cycle can be caught mid-phase. See [Draw a page](#draw-a-page).

## The load order, and why it matters

Berry resolves a global name when it **compiles** a file: `'FixedText' undeclared` is a compile
error, not a run-time one. So the check loads exactly what the firmware loads, in the order it
loads it:

1. [harness.be](./harness.be) — the record classes and the eight native modules, as globals
2. `data/modules/*.be` — in packed order, which is alphabetical
3. [driver.be](./driver.be) — it names `Settings`, so it loads after `/modules`
4. `data/routines/*.be` — in packed order

[main.c](./main.c) registers one more global before any of them: `frame`, the panel the check draws
on. It is a native module from [frame.c](./frame.c), not a Berry stub.

A module may name a class only from a file that loads **before** it, and that rule reaches inside
method bodies. `page.be` used to construct `Settings` from `settings.be`, which sorts after it;
that was refused at load on the device and every page then failed with `set super nil`. The page
now takes an editor (`open_editor(Settings("interval"))`) and the dependency points the right way.
When a new module needs another, put the class in the earlier file or invert the call; do not rely
on the listing order.

## What the harness stubs, and why each one is there

Every name a routine may use has to exist before its file compiles. The list below is the whole
surface: if a binding gains a call or a field, add it here in the same change, or the check will
report a page that works on the device as `undeclared`.

### Records — the classes `Bindings.cpp` compiles

| Class | Fields | Answered by |
| --- | --- | --- |
| `reading` | `kpa, target, duty, floor, min, state, fault, batt, sensor_ok` | `vacuum.state()` |
| `unit` | `name, suffix, per_kpa, whole_step, decimals` | `screen.unit()` |
| `bounds` | `w, h` | `screen.text_bounds()` |
| `event` | `btn, press` | `buttons.take()` |
| `setting` | `key, label, kind, step, min, max, values, nullable` | the routine's `describe()` |

A field the class does not declare raises `attribute_error` when read, which is why the harness
copies the field lists from `kRecords` rather than inventing an empty class.

### The eight modules — the calls the bindings install

| Module | Calls |
| --- | --- |
| `vacuum` | `target, target_off, clamp, start, stop, reset, state, now_ms, log, stopped` |
| `screen` | `draw_clear`, `draw_pixel`, `draw_line`, `draw_hline`, `draw_vline`, `draw_rect`, `draw_fillrect`, `draw_roundrect`, `draw_fillroundrect`, `draw_circle`, `draw_fillcircle`, `draw_ellipse`, `draw_fillellipse`, `draw_triangle`, `draw_filltriangle`, `draw_rotatedrect`, `draw_fillrotatedrect`, `draw_text`, `text_bounds`, `flush`, `unit` |
| `device` | `units, set_unit, target, set_target, bluetooth, jingle, set_jingle` |
| `buttons` | `take` |
| `buzzer` | `tone, play, off` |
| `jingles` | `ids, title, label, notes, current, play_current, default_id` |
| `routines` | `register, set_active, names, get, icon, has` |
| `store` | `get, set` (both the two- and the three-argument forms) |

Values that must match the firmware:

- `vacuum.clamp()` holds a target to `CONTROL_MIN_TARGET_KPA` … `CONTROL_FLOOR_KPA` (5 … 60 kPa).
- `vacuum.state().state` is `IDLE`, `PULL`, `HOLD`, `VENT` or `FAULT`; `target` is 0 when none is
  set; `batt` is -1 with no gauge.
- `vacuum.stopped()` is a one-shot, false on the second read.
- `screen.draw_text` and `screen.text_bounds` reject a face not in the compiled list
  (`sans9`…`sans24`, `sansbold9`…, `mono9`…, `monobold9`…, `tomthumb`, `picopixel`), and `nil`,
  `"builtin"` or no argument mean the 5×7.
- `screen.unit()` and `device.set_unit()` share the four tokens `kpa`, `psi`, `inhg`, `mmhg`.
- `store.get(key, fallback)` addresses the active routine; `store.get(name, key, fallback)` names
  one. The fallback's type is the type the text comes back as.
- `jingles.ids()` is the identifiers from `/modules/jingles.txt`, in file order; the first is the
  fallback for `jingles.current()`.
- `jingles.play_current()` starts the current jingle the same way `buzzer.play()` starts a sequence,
  and answers `false` when the library is empty.

### The editor, the store and the files

- `data/modules/*.be` are the real shipped files, not copies. `Settings`, `Page`, `List`,
  `Component` and the rest are the code under review.
- Every `data/routines/*.cfg` seeds the store under its own name, and `data/settings.cfg` seeds
  `device`, so a routine reads the values its image ships with.
- `data/modules/jingles.txt` is seeded as `jinglelibrary`, so the `JinglesStub` checks the real
  identifiers, titles, labels and note strings the image ships.
- `page.be`, `settings.be` and the components are exercised through the same calls the device
  makes, including the settings editor's whole key sequence.

## Draw a page

`--art NAME` is how a layout is judged without a panel. `ScreenStub` forwards every drawing verb to
the `frame` module, which holds a 128 x 64 one-bit frame and draws it with ports of the Adafruit_GFX
the device compiles: `writeLine`, the circle and ellipse helpers, the filled triangle, the rotated
rectangle, `drawChar` for both the classic 5x7 and the GFX faces, `write` with the same advance and
wrap, and `getTextBounds`. So `screen.text_bounds` answers the real ink box and `Text.fit()` picks
the face the glass would — which is why a render is worth trusting where the old `6 x scale` stub
was not.

```
python .github/skills/berry/check.py --art default
python .github/skills/berry/check.py --art ramped_static --seconds 30 --out C:\tmp\ramp.bmp
```

The font data is the installed Adafruit GFX library's, the same headers the device build compiles:
`glcdfont.c` and `Fonts/*.h`. `check.py` finds the library through `--gfx`, then `ADAFRUIT_GFX`,
then `arduino-cli config get directories.user`, then the usual sketchbook paths, and stops with the
`arduino-cli lib install "Adafruit GFX Library"` hint when it cannot. `gfx/Adafruit_GFX.h` is a
shim that lets those headers compile without `Arduino.h`.

What the frame does not have: the overlay's 12 px band (it is C in `Overlay.cpp`, and the frame only
holds what a page drew), inversion, and any depth beyond one bit. A BMP opens anywhere and
`scripts/asset.py` reads the same 1-bit format.

## What the drive does

For every registered routine, hidden pages included:

1. `activate(nil)`, then eight beats.
2. One OK, then 400 s of virtual time at 250 ms a beat, so a cycle's work and rest phases and the
   repeat are reached.
3. The device's Back hold: `vacuum.stopped()` turns true once, and the page must abandon.
4. Every button with every press — `up`, `down`, `ok`, `back` × `single`, `double`, `long` — and
   the Up+Down chord, a few beats apart, which walks the gesture table and the browser's page switch.
5. `activate(nil)` again, then `activate(settings)` with a map built from `describe()` and the
   stored values, which is the switch path.
6. For a routine with `describe()`: open `Settings`, edit every setting (OK, Up, OK, with the
   second OK a duration needs), and close it with Back.

The machine is a model: a target starts a pull, HOLD happens at the target, a shallower target
vents, stop vents to zero, and 250 ms of time passes per beat. A switch asked for inside a call is
deferred to the next beat, as it is on the device.

## What the check cannot tell you

- The pump, the sensor and BLE are models or counters, and the clock is virtual: timing and the
  actual vacuum are the device's. A frame is drawn, but only the page's half of it — the overlay's
  band and the panel's contrast and inversion are not simulated.
- `print` writes nowhere, here and on the device: it is the same `be_port.c`. Use `vacuum.log()`,
  which lands in the check's output as `[I]`.
- LittleFS is a map. A write that fails on a full filesystem succeeds here.
- A page that never returns still hangs the check. There is no instruction budget, on either side.

## When it fails

| Message | What it means |
| --- | --- |
| `'X' undeclared (first use in this function)` | The file names a global no earlier file declared: a module order problem, a missing stub here, or a typo |
| `value 'nil' does not support set super` | The base class's file failed to load; find its error first |
| `X needs activate() and poll()` | The registered object is not a routine instance, or a method is missing or misspelt |
| `value_error: screen.draw_text: unknown font X` | The face is not one of the compiled ones |
| `art: no routine named X` | `--art` was given a name no file registered |
| `io_error: frame.save: cannot write the file` | The `--out` path is not writable |
| `attribute_error: the 'reading' object has no attribute 'X'` | The record class has no such field |
| `settings: X open() refused` | `describe()` raised, returned nil or returned an empty list |
| `routines.names() ...` | The hidden flag or the registration is wrong |
| `vm: X poll: ...` | A run-time error; the page is dropped, as it would be on the device |

## Still the device's job

The check is the compile-and-contract loop. A change to the panel's look, the pump's behaviour or
the sensor still needs `python scripts/arduino.py --task upload` and `--task data`, then a glance
at the panel and the `[E]` lines on the serial console.
