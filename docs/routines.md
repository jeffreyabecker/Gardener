# Routines

The disconnected page is a Berry program. This is the reference for what a routine can call, when
the firmware calls it, and the handful of rules that follow from how the interpreter is embedded.

## Where a routine lives and what it must look like

Every file under `/routines/` ending in `.be` is loaded at boot. A file's top level runs once; it
registers an object and is done:

```berry
import string

class Readout
  def activate()
    # the page became visible; draw the first frame and re-read anything cached
  end

  def poll(now)
    # called once per loop pass while this page is showing; `now` is millis()
  end
end

routines.register("default", "Pressure readout", Readout())
return Readout
```

The object must be an **instance** with both `activate()` and `poll(now)`. A file that registers
anything else is refused at load with the missing method named; the other files still load.

The routine registered as **`default`** is the page. If no file registers one, the panel shows the
device's mark and nothing runs.

A routine's name is also the name of the file its values are kept in (see `store` below), so it has
to be a plain word: a name containing `/` or `\` is refused at load.

Routines ship with the data image: put them in `firmware/gardener/data/routines/` and upload with
`powershell -File scripts/arduino.ps1 -Task data`. See the README for that flow.

## When the firmware calls into a routine

| Moment | Call |
| --- | --- |
| boot, after the files load | nothing — the page's `activate()` follows |
| the page becomes visible | `activate()` |
| every loop pass while the page is shown | `poll(now)` |
| a host attaches | nothing; the page stops being polled |
| a host detaches | `activate()` — the page is visible again |

`activate()` runs again on every page entry on purpose: the display unit may have changed over
serial while a host was attached, and a routine should not trust what it cached.

## One entry per beat

The runtime is the only thing that enters the interpreter, and it does so from its own frame — one
`be_pcall` per `poll`, never from inside a Berry call. Nothing is suspended and there is no second
core, which is why there are no locks and no callbacks.

What that costs, stated plainly:

- **A `poll` that never returns blocks the loop.** There is no instruction budget in this build, so
  returning is a convention the author keeps. Keep `poll` short and let it return.
- **`print` writes nowhere.** Use `vacuum.log(text)`; it reaches the serial line as a `[I]` record
  and honours `device log=off`.
- A routine that raises is **dropped**: one `[E]` line names the routine, the method and the
  message, and the page falls back to the mark until the next boot.

## Values

Berry's numbers here are 32-bit integers and single-precision reals.

Three of the calls answer a **record**: a small object whose fields are read with a dot.

```berry
var s = vacuum.state()
var depth = s.kpa          # not s["kpa"]
```

A record is the machine as it stood when the call returned, not a live view: writing a field
(`s.kpa = 1.0`) changes the routine's copy and nothing else. Reading a field the record does not
have raises `attribute_error`, and the fields are the ones listed under each call.

## `vacuum` — the machine

| Call | Returns | Notes |
| --- | --- | --- |
| `vacuum.target(kpa)` | — | sets the depth to pull to, in kPa below ambient. A value below `min` is ignored by the machine |
| `vacuum.target_off()` | — | clears the target; releases the chamber if it was pulling or holding |
| `vacuum.clamp(kpa)` | `real` | the same number held to the machine's bounds, `min` … `floor`. See below |
| `vacuum.start()` | `bool` | false when the machine refuses: no target, no valid reading, or a latched fault |
| `vacuum.stop()` | — | stops the pump and vents. The target is kept |
| `vacuum.reset()` | — | clears a latched fault, then vents |
| `vacuum.state()` | a `reading` record | see below |
| `vacuum.now_ms()` | `int` | `millis()` |
| `vacuum.log(text)` | — | one `[I]` diagnostic record |

`vacuum.state()` answers a `reading`, read with a dot:

| Field | Type | Meaning |
| --- | --- | --- |
| `kpa` | real | current depth below ambient |
| `target` | real | the target, **0 when none is set** |
| `duty` | real | pump duty, percent |
| `floor` | real | deepest target worth asking for |
| `min` | real | shallowest target the machine accepts |
| `state` | string | `IDLE`, `PULL`, `HOLD`, `VENT` or `FAULT` |
| `fault` | string | the latched code, or `""` |
| `batt` | int | percent, or **-1 when no gauge is fitted** |
| `sensor_ok` | bool | a valid pressure reading is available |

Every pressure is kPa whatever the panel is printing. `screen.unit()` says what the glass shows.

`vacuum.clamp(kpa)` is the bounds rule in one place: below `min` it answers `min`, above `floor` it
answers `floor`, otherwise the number it was given. A target editor steps through it, a stored
target is restored through it, and a page that reads a pressure to start from uses it too — so no
routine carries its own copy of the two constants:

```berry
var s = vacuum.state()
var start = vacuum.clamp(s.kpa)         # a safe starting point for the editor
vacuum.target(vacuum.clamp(stored))     # and a stored one it cannot trust yet
```

`min` is inclusive: `vacuum.target(min)` is taken, and anything shallower is refused silently.

## `screen` — the panel

Coordinates are panel pixels, origin top left. Nothing reaches the glass until `flush()`.

| Call |
| --- |
| `screen.draw_clear()` |
| `screen.draw_pixel(x, y)` |
| `screen.draw_line(x0, y0, x1, y1)` |
| `screen.draw_hline(x, y, w)` |
| `screen.draw_vline(x, y, h)` |
| `screen.draw_rect(x, y, w, h)` |
| `screen.draw_fillrect(x, y, w, h)` |
| `screen.draw_roundrect(x, y, w, h, r)` |
| `screen.draw_fillroundrect(x, y, w, h, r)` |
| `screen.draw_circle(x, y, r)` |
| `screen.draw_fillcircle(x, y, r)` |
| `screen.draw_ellipse(x, y, rw, rh)` |
| `screen.draw_fillellipse(x, y, rw, rh)` |
| `screen.draw_triangle(x0, y0, x1, y1, x2, y2)` |
| `screen.draw_filltriangle(x0, y0, x1, y1, x2, y2)` |
| `screen.draw_rotatedrect(cx, cy, w, h, angle)` |
| `screen.draw_fillrotatedrect(cx, cy, w, h, angle)` |
| `screen.draw_text(x, y, size, inverted, text)` |
| `screen.flush()` |

`draw_text` scales the built-in face: `size` 1 is the small face, 3 is the height used by the
readout. `inverted` draws paper on ink.

`screen.unit()` answers a `unit` record, read with a dot:

| Field | Type | Meaning |
| --- | --- | --- |
| `name` | string | `kpa`, `psi`, `inhg` or `mmhg` — the same token `device unit=` takes |
| `suffix` | string | what the glass prints after the number |
| `per_kpa` | real | multiply a depth in kPa by this to get the panel's number |
| `whole_step` | real | one "whole" step in the panel's unit, for an editor |
| `decimals` | int | digits the unit's fine step can express |

The same numbers live in `core/Units.h`, so a routine and the connected page cannot disagree about
what a psi is.

### The overlay owns its corners

The firmware draws three things on top of every `flush()`, so a routine does not have to:

- the battery icon at the top right;
- the run state's icon at `x` 64–78, `y` 0–14;
- the connection icons down the right edge from `x` 116.

Keep a page's own text and shapes to the left of `x` 110 above `y` 30, and clear of the state icon.

## `buttons` — the presses

| Call | Returns |
| --- | --- |
| `buttons.take()` | an `event` record, or `nil` when the queue is empty |

`ev.btn` is `up`, `down`, `ok` or `back`. `ev.press` is `short`, `long` or `double` — the device's
press classifier, so a double tap is one event and never two shorts.

A press is queued only while this page is showing. The queue holds eight; the oldest is dropped if a
routine falls behind, and that is logged once.

Draining the queue is the routine's job, usually at the top of `poll`:

```berry
def poll(now)
  while true
    var ev = buttons.take()
    if ev == nil
      break
    end
    # ev.btn is the key that moved, ev.press the gesture
  end
end
```

Holding **Back for 3 s** is the device's own stop: it stops the pump and vents, and no routine can
suppress it. The routine still sees the `long` Back press that follows.

## `routines` — the registry

| Call | Returns | Notes |
| --- | --- | --- |
| `routines.register(name, title, routine)` | `bool` | bind a routine to a name and a title. Called from a file's top level |
| `routines.set_active(name)` | `bool` | switch pages. Called from inside a `poll` it applies when that call returns, so the VM is never re-entered |
| `routines.names()` | list of strings | every registered name |

Registering the same name twice is one routine: the later file wins, and the replacement is logged.
The registry holds eight.

## `store` — the routine's own values

| Call | Returns | Notes |
| --- | --- | --- |
| `store.get(key, fallback)` | the fallback's type | what was written earlier, or `fallback` when the key was never set |
| `store.set(key, value)` | `bool` | false when the value could not be written |

A routine's values live in `/routines/<name>.ini`, named after the routine: they are that routine's
alone, and a data-image upload can ship initial ones beside its `.be`.

The **fallback sets the type** the stored text is read back as:

```berry
var target = store.get("target", 0.0)    # real
var count  = store.get("count", 0)       # int
var muted  = store.get("muted", false)   # bool: "true" and "1" are true
var label  = store.get("label", "")      # anything else: the text as written
```

`store.set` writes a string as it stands, and a number or a bool in the interpreter's own text form.
A key that was never set returns the fallback rather than `nil`, so a first boot needs no special
case.

The file is an INI document with no sections, which is what makes it readable off the device and
editable in the data image:

```
target=12.5
count=3
```

Two limits. It holds **16 keys** — a `set` past that returns `false`. And a `set` writes to flash,
though only when the value actually changed: setting the same value on every pass costs nothing,
while storing a *changing* value on every pass wears the partition. Write on the event, not on the
frame.

The store belongs to the routine that is **active**. Use it from `activate()` or `poll()`; a file's
top level runs before any routine is active, when there is no file to address.

## A complete example

The shipped routine, `/routines/default.be`, shows the whole surface in use: the readout, a target
editor stepped in whole display units, OK to start or commit, Back to vent, and the target kept
across a boot by `store`. It is the place to look for a working shape rather than a second
description here.
