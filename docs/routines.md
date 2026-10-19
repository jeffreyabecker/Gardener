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

An optional `describe()` gives the settings editor a list of values to edit (see below). A routine
without one simply has no settings.

The routine registered as **`default`** is the page. If no file registers one, the panel shows the
device's mark and nothing runs.

A routine's name is also the name of the file its values are kept in (see `store` below), so it has
to be a plain word: a name containing `/` or `\` is refused at load.

Routines ship with the data image: put them in `firmware/gardener/data/routines/` and upload with
`powershell -File scripts/arduino.ps1 -Task data`. See the README for that flow.

## Shared code in `/modules`

Every file under `/modules/` ending in `.be` is loaded before any file under `/routines/`. A module
file defines classes or functions and registers nothing: it has no page, no store file and no entry
in `routines.names()`. It is not an `import` target either — `import` reaches only the native
modules such as `string` — its classes are simply globals by the time the pages run.

Ship a module the same way as a routine: put it in `firmware/gardener/data/modules/` and upload the
data image.

Files within `/modules/` load in no particular order, so none may depend on another at its top
level. Define classes and functions there; leave constructing them to the page that uses them.

`/modules/settings.be` is the one shipped. `Settings(name)` edits the `describe()` list of the
routine registered under that name. **Double OK** is the standard gesture for opening it; a page
that wants the editor holds one and delegates to it:

```berry
def activate()
  self.editor = nil
end

def draw()
  screen.draw_clear()
  if self.editor != nil
    self.editor.draw()
  else
    # the page's own content
  end
  screen.flush()
end

def handle(ev)
  if self.editor != nil
    self.editor.handle(ev)
    if !self.editor.is_open()
      self.editor = nil
    end
    return true
  end
  if ev.btn == "ok" && ev.press == "double"
    var editor = Settings("mypage")
    if editor.open()
      self.editor = editor
    end
    return true
  end
  return false
end
```

`open()` calls the routine's `describe()` and reads each value from its store file. `draw()` paints
the list or the edit screen without clearing or flushing, so the page still owns the panel. Back at
the top closes the editor and `is_open()` turns false. The routine still implements `describe()`,
which remains the one description of what is editable.

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

The `setting` record that `describe()` returns is the same kind of object, but a routine builds it
itself rather than a call answering it; its fields are listed under `describe()` below.

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

Holding **OK for 3 s** is the page's way out: it stops and vents, then hands the panel back to
`default`. Whichever routine was showing is no longer polled, and `default` is activated again — the
same object that was registered at boot, so it keeps whatever state it had. Nothing is re-read from
disk; a routine that was left can be brought back with `routines.set_active()`.

The standard gestures a page should follow — double OK for its settings, OK for its primary action,
Back to leave a view, and the two holds above — are in [gestures.md](./gestures.md), with the matrix
of what every press does on the shipped pages.

## `buzzer` — the piezo

| Call | Returns | Notes |
| --- | --- | --- |
| `buzzer.tone(hz, duration_ms)` | — | a square wave for that many milliseconds; `0` means until `buzzer.off()`; `hz` is clamped to `BUZZER_MIN_HZ` … `BUZZER_MAX_HZ` |
| `buzzer.play(sequence)` | `bool` | a sequence of note/duration pairs; false when malformed |
| `buzzer.off()` | — | silence and stop any sequence |

`play` takes note names with an optional `#` or `b`, then one octave digit (`a4`, `c#5`, `bb3`); `r`
is a rest. Durations are milliseconds. The only valid notation is `note:duration` pairs separated by
spaces:

```berry
buzzer.tone(2000, 500)
buzzer.play("a4:1000 c6:500 a4:500")
buzzer.off()
```

The player is non-blocking: `tone` and `play` start a schedule and return, and the firmware advances
it in the loop. A note outside the buzzer's range makes `play` return false rather than clamp. A
non-positive `hz` means silence. A routine that starts a tone or sequence owns stopping it before it
leaves the page; a sequence stops itself at the end.

The notation is strict: a comma, a missing colon, or a space between a note and its duration makes
the whole sequence malformed and `play` returns false.

## `routines` — the registry

| Call | Returns | Notes |
| --- | --- | --- |
| `routines.register(name, title, routine)` | `bool` | bind a routine to a name and a title. Called from a file's top level |
| `routines.set_active(name)` | `bool` | switch pages. Called from inside a `poll` it applies when that call returns, so the VM is never re-entered |
| `routines.names()` | list of strings | every registered name, in registration order |
| `routines.get(name)` | the routine object, or `nil` | the registered instance, so a page can call a method such as `describe()` in its own frame |
| `routines.has(name, method)` | `bool` | whether that routine has a function of that name |

Registering the same name twice is one routine: the later file wins, and the replacement is logged.

There is no fixed number of routines: the registry grows as files register. What bounds it is the
heap the files are loaded into, not a table, so a boot that runs out of memory while loading aborts
and says so rather than dropping the file quietly. A name keeps the position it registered in, which
is what `routines.names()` reports and what a page can use as an index.

`routines.get()` and `routines.has()` look through the same registry. The shared settings editor
uses them to call another page's `describe()` without the firmware re-entering the interpreter from
a native call.

## `describe()` — the settings a routine describes

A routine that wants editable values exposes `describe()`. It returns a list of `setting` records;
the editor calls it when the settings view opens, so the list can reflect the machine as it is now:

```berry
def describe()
  return [
    setting("target", "Target", "pressure", 1, nil, nil, nil),
    setting("hold", "Hold", "duration", 1, 0, 7200, nil),
    setting("mode", "Mode", "choice", 0, nil, nil, ["hold", "cycle"]),
    setting("enabled", "Enabled", "bool", 0, nil, nil, nil),
    setting("label", "Label", "text", 0, nil, nil, nil)
  ]
end
```

`setting(key, label, kind, step, min, max, values)` builds one record:

| Field | Type | Meaning |
| --- | --- | --- |
| `key` | string | the key in the routine's `store` file |
| `label` | string | the line the editor draws beside the value |
| `kind` | string | `real`, `int`, `bool`, `choice`, `duration` or `pressure`; other kinds are shown read-only |
| `step` | real | how far Up/Down moves a numeric value; for `duration`, the seconds increment in the seconds stage; for `pressure`, whole steps in the panel's unit; ignored by `bool` and `choice` |
| `min` / `max` | real or nil | optional bounds for a numeric value; for `duration` both are total seconds and the value never goes below 0; ignored by `pressure`, whose bounds are the machine's |
| `values` | list or nil | the allowed values for `choice`, in cycle order |

The editor reads the current value through `store`, with a fallback whose type matches the kind:
`0` for `int` and `duration`, `0.0` for `real` and `pressure`, `false` for `bool`, and `values[0]`
for `choice`. A `choice` needs at least one value. Up/Down steps `real`, `int` and `pressure`,
toggles `bool`, and cycles `choice`; OK writes the value back with `store.set` and Back leaves
without writing.

A `duration` is stored as whole seconds and shown as `2m30s`, `45s` under a minute or `1m` on a
whole minute. OK opens it in the **minutes** stage, Up/Down moves that by a minute, OK moves on to
the **seconds** stage, and Up/Down moves that by `step` seconds (default 1); OK then writes. Back
cancels from either stage. The value is held between `min` and `max`, and never below zero.

A `pressure` is stored in kPa, the machine's unit, and shown and stepped in the unit on the glass:
`12.5 psi`, `0.12 inHg` or `40 mmHg`, suffix included. Up/Down moves it by `step` whole display
steps — one psi, one inHg, ten mmHg — and every value goes through `vacuum.clamp()`, whether it was
read, stepped or restored, so the editor cannot produce a target the machine refuses. An unset
pressure reads as `0.0` and therefore clamps to the shallowest legal target. `min` and `max` are
ignored: the machine's `min` … `floor` is the range.

`describe()` is an ordinary method: it can read `vacuum.state()` and the panel unit, and it must
return a list. A routine without `describe()`, or one whose method raises, leaves the settings view
closed — on the browser, the routine list stays as it was — and writes one `vacuum.log` line.

## `store` — the routine's own values

| Call | Returns | Notes |
| --- | --- | --- |
| `store.get(key, fallback)` | the fallback's type | the active routine's value, or `fallback` when the key was never set |
| `store.set(key, value)` | `bool` | false when the value could not be written |
| `store.get(name, key, fallback)` | the fallback's type | a registered routine's value |
| `store.set(name, key, value)` | `bool` | write a registered routine's value |

A routine's values live in `/routines/<name>.ini`, named after the routine, and a data-image upload
can ship initial ones beside its `.be`. A routine writes its own; the named forms in the table let
a page such as `browser` edit them.

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

The one- and two-argument forms belong to the routine that is **active**. Use them from `activate()`
or `poll()`; a file's top level runs before any routine is active, when there is no file to address.
The named forms are how a page such as `browser` reaches another routine's values. They cannot walk
outside `/routines/`: a name that is not registered is refused.

## A complete example

Three routines ship. `/routines/default.be` shows the whole surface in use: the readout, a target
editor stepped in whole display units, OK to start or commit, Back to vent, and the target kept
across a boot by `store`. A **double Up** on that page opens the second one.

`/routines/browser.be` is the list of registered routines: Up and Down walk it and wrap, a six-row
window follows the selection, OK switches to the highlighted page and Back goes home. A **double OK**
opens the highlighted routine's settings when it has `describe()`: Up and Down walk the settings, OK
opens one, Up/Down changes it, OK saves and Back moves up a level. The editor itself is
`/modules/settings.be`; the browser hands it the panel and the keys while it is open. The browser
reads `routines.names()`, so a routine that is added to the data image appears in it with no change
to either file. It touches nothing of the machine — a run keeps going behind it — and it writes only
through `store.set` when a setting is saved.

`/routines/static.be` is the cycle: OK starts a pull to `target_pressure`, `HOLD` starts the work
timer, `vacuum.stop()` vents, `IDLE` starts the rest timer, and `resume_behavior` decides whether the
next cycle starts by itself or on OK. It is also the worked example of a page whose state is a phase
machine: `poll` advances a phase and never waits, and a release it did not ask for (the Back hold)
sends it back to `ready`.

Between them the three pages cover what a page can do; look at them rather than at a second
description here.
