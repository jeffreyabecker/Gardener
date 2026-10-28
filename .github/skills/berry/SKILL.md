---
name: berry
description: 'Writing the Berry data image under firmware/data. Use when writing, changing, debugging or reviewing a .be routine or module, or a C++ binding in libraries/BerryHost/src/: the load order the image compiles in, the names a class may not take, and which side of the Berry boundary a piece belongs on. The host harness in tools/berry/ is broken and is not to be run.'
---

# Berry

## When to use

- Editing anything under `firmware/data/system/` or
  `firmware/data/routines/`
- Editing `libraries/BerryHost/src/` — the install, the four native modules, the page scene and the
  draw tree — or `routines/Routines.cpp`, which owns the VM and the beat

A `.be` file is compiled by the device at boot, so `arduino.py build` never sees it and `--task data`
only packs it. It is checked on the device: `--task upload`, then a look at the panel.

A change to the C++ under `libraries/Gardener` or `libraries/Gfx` is not this skill's business and
does not need the interpreter: `python tools/unit/check.py` compiles and runs it on the host.

> **NEVER RUN THIS SCRIPT — the berry check in `tools/berry/` is in a known broken state.** Ignore
> the rest of the harness documentation; it is commented out below, ready for when the check works
> again.

<!-- Harness notes, commented out while `tools/berry/check.py` is broken.

- A page misbehaves on the panel and you want the answer without a build and a flash

The harness in [tools/berry/](../../../tools/berry) compiles and runs the whole image on the host
instead.

## Run it

```powershell
python tools/berry/check.py                              # compile, load and drive the image
python tools/berry/check.py --compile                    # compile and load only
python tools/berry/check.py --art interval --seconds 60  # animate a page instead of driving it
python tools/berry/check.py --scene browser              # draw one of the device's own scenes
python tools/berry/check.py --tests                      # run the test suites, a GIF per case
python tools/berry/check.py --test ramped_static         # run one suite (or part of one)
```

The last line says how much loaded — `12 of 12 files loaded, check passed` — and the exit code is 0
only when every file compiled, every registration looked like a routine, and every page ran. The
build lands in `tools/berry/build/`, which is ignored, so a `.be` edit only recompiles and loads.

The flags, the load order, the six modules and records a routine compiles against, the frame and
what the drive does are held by `tools/berry/` itself, and that is the whole statement of them:
`harness.be` for the driver's bookkeeping, `driver.be` for the smoke walk, and `tests/suite.be`
plus `tests/*.test.be` for the named suites.

**Look at the picture.** `--art` draws the page the panel would, in the face the panel would pick;
`--scene home`, `--scene browser` and `--scene system` draw the device's own well-known scenes.
The output is an animated GIF — one frame per beat, each held a second by `kGifDelayCs`,
enlarged 2 x by `kRenderScale`. The GIF draws
a full-width strip across the top showing each injected press (`OK`, `OK×2`, `CANCEL 5s`) for one
beat, behind `kGestureCalloutBeats`; a suite's captures hash the overlay band instead of drawing
the device's status row, so the page the case drove reads as the whole picture. Writing a frame out
as text loses the thing the render is for.

**The frame belongs to the person who opens it.** Nothing in a run reads a capture back, and
nothing it prints describes one: a `.gif` under `tools/berry/build/` or
`tools/berry/artifacts/` is there for a human to look at. Do not convert one, enlarge it,
transcribe it or diff two of them to decide whether a change is right. That is a reading the
harness never made a claim about, of pixels that are the host's rendering rather than the
panel's, and an approval it cannot support. Say what the code draws and where; leave the file for
the human, and let a layout question be answered by the numbers a case asserts — the ink a `Text`
measures, the box a component places.

## When to run it

The check proves a change; it is not a step in making one. A run compiles the image, loads it and
drives a page for every registered routine, then prints a line per page and per case — long output,
most of it the same lines as last time. Read the code while working, and run the check when the
change is done, or when a failure needs a diagnosis that reading the code cannot give.

Two things make a run worth nothing. The first is a tree the last run already answered for. The
second is a change that cannot reach the image: a document, a `.ini` seed, an `assets/` image, a
comment, or C++ outside `libraries/berry/`. A refactor that moves code without changing what it
draws is the same case one step in: a frame captured before the edit and again after it, to diff
them, is two runs to learn what the diff of the code already said. Write a frame when a human needs
to see a layout, a face or a glyph, and leave it alone when the picture is not in doubt. And a suite
already red for a reason that has been diagnosed stays red: that failure is the baseline, not an
invitation to run it again.

When several edits are ahead, hold the check until the last of them; a run between them is a run
that the next edit invalidates. When the user asks for one, run it. Otherwise one check at the end
of a change that can break a routine is the verification, and a second is waiting for an answer it
already gave.

-->

## Reading the output

Two things bite when the image loads:

- A file resolves globals **when it compiles**, method bodies included, so a class a file names has
  to be a global an earlier file defined. A new class belongs in an earlier module, or the call is
  inverted; it never works defined below its first user. `/system` is loaded first and does not
  sort: `/system/order` names its files in the order they compile, so a new system file is an edit
  to that file, and one it does not name does not load.
- A builtin's name is not a class's to take. `size`, `str`, `int`, `real`, `bool`, `number`, `type`,
  `classname`, `classof`, `module`, `list`, `map`, `range`, `bytes`, `compile`, `call`, `format`,
  `print`, `input`, `assert`, `super`, `open`, `isinstance`, `issubclass`, `__iterator__` are the
  VM's own; a class named `size` fails at load with no message and exit code `0xC0000005`, which
  reads as a crash rather than an error.

<!-- Harness output, commented out while `tools/berry/check.py` is broken.

- A page that never returns hangs the check. There is no instruction budget, on either side.

| Message | What it means |
| --- | --- |
| `'X' undeclared (first use in this function)` | The file names a global no earlier file declared: a load-order problem, a missing stub, or a typo |
| `value 'nil' does not support set super` | The base class's file failed to load; find its error first |
| `X needs activate() and poll()` | The registered class is not a scene, or a method is missing or misspelt |
| `value_error: screen.draw_text: unknown font X` | The face is not one of the compiled ones |
| `art: no routine named X` | `--art` was given a name no file registered |
| `art: no scene named X` | `--scene` was given a name that is not `home`, `browser` or `system` |
| `art: cannot write PATH` | A GIF could not be written, or the run had no frames |
| `attribute_error: the 'reading' object has no attribute 'X'` | The record class has no such field |
| `settings: X has no settings` | The routine registered no settings, or the name is not registered |
| `vm: X poll: ...` | A run-time error; the page is dropped, as it would be on the device |

## What it cannot tell you

The pump, the sensor and Bluetooth are models or counters and the clock is virtual, so timing and
the actual vacuum stay the device's. `print` writes nowhere, here as on the device: `vacuum.log()`
lands in the output as `[I]`. LittleFS is a map, so a write that would fill a filesystem succeeds.
-->

## A binding is a wrapper

A class a page is written against is a C++ class in the library, and the file in
`libraries/BerryHost/src/` is what presents it: the `be_pushclass` table, the callbacks that unpack
their arguments and push the class's answer, and the hook that reaches a page's override back
through the interpreter. What belongs on each side of that line is the Berry boundary rule in
[the firmware skill](../firmware/SKILL.md). A model kept in a `.be` file beside its wrapper is a
second implementation: the two drift, and the image pays for both at every boot.

## Still the device's job

A change to the panel's look, the pump's behaviour or the sensor is checked with
`python scripts/arduino.py --task upload` and `--task data`, then a glance at the panel and the `[E]`
lines on the serial console.
