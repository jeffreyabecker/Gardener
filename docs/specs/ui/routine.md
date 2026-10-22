# Routines

The disconnected page is a Berry program. This is the contract a routine must satisfy: where it
lives, what it must look like, and when the firmware calls it. What a routine can call — the native
modules and the records they answer with — is [bindings.md](./bindings.md).

This file is the contract for **any** routine, not a description of the ones that ship. A page's
phases and its settings keys belong in that page's own `.be` file, its gestures in
[gestures.md](../gestures.md), and `firmware/gardener/data/routines/` holds the pages this build
ships when you want to read one. Nothing here should need to change when one of them changes.

The panel UI is a stack of **scenes**. A routine file registers a scene instance; the firmware polls
the top scene and only the top scene, and a scene pushes or pops scenes with the `ux` module. The
registry the routine browser draws is a Berry class — the global `routines` instance in
`/modules/registry.be` — and the only native additions over the older page model are `ux.*` (the
stack) and the now name-only `store.*` forms.

## Where a routine lives and what it must look like

Every file under `/routines/` ending in `.be` is loaded at boot. A file's top level runs once; it
registers an object and is done:

```berry
import string

class MyPage : Scene
  def activate()
    # the scene became the top scene; draw the first frame and re-read anything cached
  end

  def poll(now)
    # called once per loop pass while this scene is showing; `now` is millis()
  end
end

routines.register("mypage", "My page", MyPage())
return MyPage
```

The object must be an **instance** with both `activate()` and `poll(now)`. `Scene`
(`/modules/scene.be`) provides `poll`, a press drain, the repaint cadence and the gesture
dispatch; a routine extends it and defines `activate()` plus whatever `draw_page()` and `tick(now)`
it needs. The stack refuses to push a scene that lacks either method.

`activate()` may take one argument: the settings map a switch carried, or `nil` when it carried
none. The map belongs to that activation alone — the runtime neither keeps it nor writes it
anywhere. A routine that wants values across a re-entry reads them through `store`; a routine that
accepts a map decides what a missing key means (the shipped `interval` uses its fallback, not the
file's value).

An optional `describe()` gives the settings editor a list of values to edit (see below). A routine
without one simply has no settings.

A routine can register itself **hidden** with the fourth argument, for example
`routines.register("mypage", "My page", MyPage(), true)`. It is registered and can be pushed like
any other, but it is left out of `routines.names()`, which is the list the routine browser draws.
The device's own scenes — the readout, the system settings and the browser — are not registered at
all: they are well-known scenes in `/modules/ui.be` the system pushes directly.

The fifth argument is the routine's **icon**: an instance of a class extending `Icon` from
`/modules/components.be`, which draws itself with the `screen` primitives and has an intrinsic `w`.
The browser draws it at the left of the routine's row and shifts the name right by `icon.w + 2`. A
routine that registers no icon, or passes `nil`, has a text-only row:
`routines.register("mypage", "My page", MyPage(), false, MyIcon())`.

The root scene — the readout — is the `__rootScene` global `/modules/ui.be` sets. If nothing sets
one, the panel shows the device's mark and nothing runs.

A routine's name is also the name of the file its values are kept in (see `store` in
[bindings.md](./bindings.md)), so it has to be a plain word: a name containing `/` or `\` is
refused at load.

Routines ship with the data image: put them in `firmware/gardener/data/routines/` and upload with
`python scripts/arduino.py --task data`.

## Shared code in `/modules`

Every file under `/modules/` ending in `.be` is loaded before any file under `/routines/`. A module
file defines classes or functions and registers nothing: it has no page, no store file and no entry
in `routines.names()`. It is not an `import` target either — `import` reaches only the native
modules such as `string` — its classes are simply globals by the time the pages run.

Ship a module the same way as a routine: put it in `firmware/gardener/data/modules/` and upload the
data image.

`/modules/jingles.txt` is data rather than a module. It is not compiled or loaded as a `.be` file;
the native `jingles` binding reads it at boot and exposes its entries as `jingles.ids()`,
`jingles.title()`, `jingles.label()`, `jingles.notes()`, `jingles.current()` and
`jingles.play_current()`. Its format and the
call list are in [bindings.md](./bindings.md#jingles--the-note-library).

Files within `/modules/` load in directory order, which is the alphabetical order the image is
packed in, so a module may only name a class a module that loads before it defines. That rule
reaches inside methods as well as the top level: Berry compiles a name when it reads the file, so a
reference to a later module's class is refused at load. Keep the dependency pointing at the earlier
file, or move the class into the file that uses it. Define classes and functions there; leave
constructing them to the page that uses them.

The page framework ships the same way. `/modules/scene.be` is `Scene` — the button drain, the
repaint cadence and the gesture dispatch a routine's scene extends. `/modules/registry.be` is the
registry: `class RoutineRegistry` and the global `routines` instance — the list the browser draws,
and the `push(name, callback)` that opens a routine. `/modules/components.be` is `Component` and the
drawing primitives a readout composes (`FixedText`, `DynamicText`, `PressureDisplay`,
`ProgressBar`, `ProgressBubbles`, `Scrollbar`, `Triangle`, and the `Icon` glyph).
`/modules/list.be` is `List`, a selectable menu. `/modules/ui.be` is the device's own scenes — the
readout, the system settings and the browser — none of them registered.

The settings view ships as `/modules/settings.be`: `SettingsScene` (the list) and one value scene
per kind (`BoolScene`, `ChoiceScene`, `RealScene`, `IntScene`, `PressureScene`, `DurationScene`).
**Up+Down** is the standard gesture for opening it. A routine pushes `SettingsScene` with a
callback that writes the committed values; the value scenes pop with the value they settled on:

```berry
def edit()
  ux.push(SettingsScene("mypage", / key, value -> self.on_setting(key, value)), nil, / v -> self.draw())
end

def on_setting(key, value)
  store.set("mypage", key, value == nil ? "" : value)
end
```

`SettingsScene` reads the routine's `describe()` and each value from its store file; nothing in
`settings.be` writes. A committed value is handed to the caller's callback, which writes where
appropriate. Back at the top of the list pops the settings scene with nil. The routine still
implements `describe()`, which remains the one description of what is editable.

## When the firmware calls into a scene

| Moment | Call |
| --- | --- |
| boot, after the files load | the firmware pushes the root scene (`__rootScene`) |
| a scene becomes the top of the stack | `activate(args)` — the push's args, or `nil` |
| every panel-task beat while the scene is on top | `poll(now)` |
| a scene is popped off the top | the pusher's callback runs with the returned value, or the scene below re-activates |
| a controller acquires control | nothing; the top scene stops being polled |
| the controller releases control, or the device resets | `activate(nil)` on the top scene |

`activate()` runs again on every push and on every return to a scene that was pushed without a
callback: the display unit may have changed while the scene was not showing, and a scene should not
trust what it cached. A push's args are not carried across a re-entry; a re-entry without them reads
the file.

## The stack

`ux.push(scene [, args [, callback]])` puts a scene on top and makes it the one that is polled.
`ux.pop([value])` leaves it and hands `value` to the pusher's callback, or re-activates the scene
below when it was pushed without one. `ux.pop_to_root([value])` pops every scene above the root in
one move — a finished run's way home. The root is the `__rootScene` global `/modules/ui.be` sets;
if nothing sets it, the panel shows the device's mark.

Only the top scene polls and draws, so a scene below the top is suspended — it keeps its state and
resumes through its callback when the scene above pops. A pop asked for from inside a scene's own
`poll` is deferred until that call returns, so a scene may pop itself.

## One entry per beat

The runtime is the only thing that enters the interpreter, and it does so from its own frame — one
`be_pcall` per `poll`, never from inside a Berry call. It runs on the panel task, and the one-writer
rule is what keeps it lock-free: the machine is written only through the command queue, and read
dirty by any task. Nothing is suspended, and there are no locks and no callbacks.

What that costs, stated plainly:

- **A `poll` that never returns blocks the panel loop.** The control loop and the wire run on their
  own tasks, so a slow routine cannot starve them. There is no instruction budget in this build, so
  returning is a convention the author keeps. Keep `poll` short and let it return.
- **`print` writes nowhere.** Use `vacuum.log(text)`; it reaches the serial line as a `[I]` record
  when the device's `log` set includes `I`.
- A routine that raises is **dropped**: one `[E]` line names the routine, the method and the
  message, and the page falls back to the mark until the next boot.

## `describe()` — the settings a routine describes

A routine that wants editable values exposes `describe()`. It returns a list of `setting` records;
the editor calls it when the settings view opens, so the list can reflect the machine as it is now:

```berry
def describe()
  return [
    setting("target", "Target", "pressure", 1, nil, nil, nil, false),
    setting("hold", "Hold", "duration", 1, 0, 7200, nil, false),
    setting("mode", "Mode", "choice", 0, nil, nil, ["hold", "cycle"], false),
    setting("enabled", "Enabled", "bool", 0, nil, nil, nil, true),
    setting("label", "Label", "text", 0, nil, nil, nil, false)
  ]
end
```

`setting(key, label, kind, step, min, max, values, nullable)` builds one record:

| Field | Type | Meaning |
| --- | --- | --- |
| `key` | string | the key in the routine's `store` file |
| `label` | string | the line the editor draws beside the value |
| `kind` | string | `real`, `int`, `bool`, `choice`, `duration` or `pressure`; other kinds are shown read-only |
| `step` | real | how far Up/Down moves a numeric value; for `duration`, the seconds increment in the seconds stage; for `pressure`, whole steps in the panel's unit; ignored by `bool` and `choice` |
| `min` / `max` | real or nil | optional bounds for a numeric value; for `duration` both are total seconds and the value never goes below 0; ignored by `pressure`, whose bounds are the machine's |
| `values` | list or nil | the allowed values for `choice`, in cycle order |
| `nullable` | bool | whether the value may be nothing; see below |

The editor reads the current value through `store`, with a fallback whose type matches the kind:
`0` for `int` and `duration`, `0.0` for `real` and `pressure`, `false` for `bool`, and `values[0]`
for `choice`. A `choice` needs at least one value. Up/Down steps `real`, `int` and `pressure`,
toggles `bool`, and cycles `choice`; OK writes the value back with `store.set` and Back leaves
without writing.

A `nullable` setting whose key is unset — or holds an empty value — reads as `nil` and shows `--`
in the list and the editor. It opens at the kind's lowest real value (the value a non-nullable
setting starts from), and `nil` sits one step below that: Down at the lowest value clears back to
`--`, Up from `--` enters the lowest value, and Down at `--` stays. OK on `--` writes an empty
value, which reads as `nil` again.

A `duration` is stored as whole seconds and shown as `2m30s`, `45s` under a minute or `1m` on a
whole minute. OK opens it in the **minutes** stage, Up/Down moves that by a minute, OK moves on to
the **seconds** stage, and Up/Down moves that by `step` seconds (default 1); OK then writes. Back
cancels from either stage. The value is held between `min` and `max`, and never below zero. A
nullable duration cleared from the minutes stage advances to the seconds stage still `nil`, and the
second OK writes the clear.

A `pressure` is stored in kPa, the machine's unit, and shown and stepped in the unit on the glass:
`12.5` in psi, `0.12` in inHg or `40` in mmHg. The unit itself is the overlay's, printed in its
bottom band rather than beside any number, so a page or the editor draws the value alone. Up/Down
moves it by `step` whole display steps — one psi, one inHg, ten mmHg — and every value goes through
`vacuum.clamp()`, whether it was read, stepped or restored, so the editor cannot produce a target
the machine refuses. An unset pressure reads as `0.0` and therefore clamps to the shallowest legal
target; a nullable one reads as `nil`, and its lowest value is that same shallowest target. `min`
and `max` are ignored: the machine's `min` … `floor` is the range.

`describe()` is an ordinary method: it can read `vacuum.state()` and the panel unit, and it must
return a list. A routine without `describe()`, or one whose method raises, leaves the settings view
closed and writes one `vacuum.log` line.
