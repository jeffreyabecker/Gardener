---
name: berry
description: 'Checking the Berry data image under firmware/gardener/data without flashing. Use when writing, changing, debugging or reviewing a .be routine or module, or a C++ binding under routines/bindings/: the host app in tools/berry/ compiles, drives and draws the whole image, so a typo costs a second rather than an upload. Covers the commands, the message each failure prints, and where the contract lives.'
---

# Berry

## When to use

- Editing anything under `firmware/gardener/data/routines/` or `firmware/gardener/data/modules/`
- Editing `routines/Bindings.cpp`, `routines/Routines.cpp` or a file under `routines/bindings/`
- A page misbehaves on the panel and you want the answer without a build and a flash

A `.be` file is compiled by the device at boot, so `arduino.py build` never sees it and `--task data`
only packs it. The harness in [tools/berry/](../../../tools/berry) compiles and runs the whole image
on the host instead.

## Run it

```powershell
python tools/berry/check.py                              # compile, load and drive the image
python tools/berry/check.py --compile                    # compile and load only
python tools/berry/check.py --art interval --seconds 60  # draw a page instead of driving
```

The last line says how much loaded — `12 of 12 files loaded, check passed` — and the exit code is 0
only when every file compiled, every registration looked like a routine, and every page ran. The
build lands in `tools/berry/build/`, which is ignored, so a `.be` edit only recompiles and loads.

The flags, the load order, the eight modules and records a routine compiles against, the frame and
what the drive does are the contract: [Berry harness](../../../docs/specs/berry-harness.md). Its
sources are in `tools/berry/` — `harness.be` for the stubs, `driver.be` for the checks.

**Look at the picture.** `--art` draws the page the panel would, in the face the panel would pick.
Convert the BMP to a PNG when the viewer will not open one and read the page off that; writing a
frame out as text loses the thing the render is for.

## Reading the output

Two things bite before the messages do:

- A file resolves globals **when it compiles**, method bodies included, so a class a file names has
  to be a global an earlier file defined. A new class belongs in an earlier module, or the call is
  inverted; it never works defined below its first user.
- A page that never returns hangs the check. There is no instruction budget, on either side.

| Message | What it means |
| --- | --- |
| `'X' undeclared (first use in this function)` | The file names a global no earlier file declared: a load-order problem, a missing stub, or a typo |
| `value 'nil' does not support set super` | The base class's file failed to load; find its error first |
| `X needs activate() and poll()` | The registered object is not a routine instance, or a method is missing or misspelt |
| `value_error: screen.draw_text: unknown font X` | The face is not one of the compiled ones |
| `art: no routine named X` | `--art` was given a name no file registered |
| `io_error: frame.save: cannot write the file` | The `--out` path is not writable |
| `attribute_error: the 'reading' object has no attribute 'X'` | The record class has no such field |
| `settings: X open() refused` | `describe()` raised, returned nil or returned an empty list |
| `vm: X poll: ...` | A run-time error; the page is dropped, as it would be on the device |

## What it cannot tell you

The pump, the sensor and Bluetooth are models or counters and the clock is virtual, so timing and
the actual vacuum stay the device's. `print` writes nowhere, here as on the device: `vacuum.log()`
lands in the output as `[I]`. LittleFS is a map, so a write that would fill a filesystem succeeds.

## Still the device's job

The check is the compile-and-contract loop. A change to the panel's look, the pump's behaviour or the
sensor still needs `python scripts/arduino.py --task upload` and `--task data`, then a glance at the
panel and the `[E]` lines on the serial console.
