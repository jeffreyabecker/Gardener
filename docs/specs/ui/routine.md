# Routines

The disconnected page is a Berry program. This is the contract a routine must satisfy: where it
lives, what it must look like, and when the firmware calls it. What a routine can call — the native
modules and the records they answer with — is [bindings.md](./bindings.md).

This file is the contract for **any** routine, not a description of the ones that ship. A page's
phases and its settings keys belong in that page's own `.be` file, its gestures in
[gestures.md](../gestures.md), and `firmware/gardener/data/routines/` holds the pages this build
ships when you want to read one. Nothing here should need to change when one of them changes.

The panel UI is a stack of **scenes**. A routine file registers the class its scene is built from;
the firmware polls the top scene and only the top scene, and a scene pushes or pops scenes with the
`ux` module. The registry the routine browser draws is a Berry class — the global `routines`
instance in `/modules/registry.be` — and the only native additions over the older page model are
`ux.*` (the stack) and the now name-only `store.*` forms.

## Where a routine lives and what it must look like

Every file under `/routines/` ending in `.be` is loaded at boot. A file's top level runs once; it
registers an object and is done:

```berry
import string

class MyPage : Page
  def init()
    super(self).init("mypage")
  end

  def activate()
    # the scene became the top scene; draw the first frame and re-read anything cached
  end
end

routines.register("mypage", MyIcon(), MyPage, MySettings)
return MyPage
```

`Page` (`/modules/settings_page.be`) is `Scene` plus the routine's own name: it is what the store
file is called, and it is the class that opens the settings list, so a page writes the name once.

The second argument is the routine's **icon**: an instance of a class extending `Icon` from
`/modules/components.be`, which draws itself with the `screen` primitives and has an intrinsic `w`.
The browser draws it at the left of the routine's row and shifts the name right by `icon.w + 2`. A
routine that passes `nil` has a text-only row.

The third argument is the routine's **class**, not an instance: the registry builds a scene from it
every time the routine is pushed, so each visit starts from `activate()` and nothing a previous
visit left behind survives. The scene that is showing is `ux.top()`. The class must be a scene:
`Scene` (`/modules/scene.be`) provides `poll`, a press drain, the repaint cadence and the gesture
dispatch, and `Page` adds the routine's name and its settings view; a routine extends `Page` and
defines `activate()` plus whatever `draw_page()` and `tick(now)` it needs. The stack refuses to push
a scene that lacks either `activate()` or `poll`.

The fourth argument is the routine's **settings scene** — the class that edits its values, or `nil`
when it has none. It is a `SettingsScene` subclass, and it is where the list and any rule that binds
one setting to another live, so the page never names the keys itself (see below).

`activate()` may take one argument: the settings map a switch carried, or `nil` when it carried
none. The map belongs to that activation alone — the runtime neither keeps it nor writes it
anywhere. A routine that wants values across a re-entry reads them through `store`; a routine that
accepts a map decides what a missing key means (the shipped `interval` uses its fallback, not the
file's value).

Every registration is listed: there is no hidden routine. The device's own scenes — the readout, the
system settings and the browser — are not registered at all: they are well-known scenes in
`/modules/ui.be` the system pushes directly.

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
`ProgressBar`, `ProgressBubbles`, `Scrollbar`, `Triangle`, and the `Icon` glyph). Every one of them
has a box, and setting `inverted` on it flips that box as the last thing its draw does — paper
where its ink was — which is what a selected row, a cursor or a focused readout looks like.
`/modules/list.be` is `List`, a selectable menu. `/modules/ui.be` is the device's own scenes — the
readout, the system settings and the browser — none of them registered.

The settings view ships in four files under `/modules/`. `settings.be` is the pair of duration
helpers. `settings_edit.be` is the editing screens: one value scene per kind (`BoolScene`,
`ChoiceScene`, `RealScene`, `IntScene`, `PressureScene`, `DurationScene`) over a shared
`ValueScene`. `settings_editors.be` is the editor each kind's setting carries. `settings_page.be`
is `SettingsScene` (the list), `Activation` (one activation's values) and `Page` (the scene a
routine extends). **Up+Down** is the standard gesture for opening the list; the page's own gesture
calls `open_settings()`, which pushes the list under the page's name and writes each committed value
to the page's file:

```berry
def edit()
  self.open_settings()
  return true
end
```

`SettingsScene` reads the list the routine registered and each value from its store file through the
setting's editor; nothing in the four settings modules writes. A committed value is handed to the
caller's callback, which writes where appropriate. Back at the top of the list pops the settings
scene with nil.

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

## The settings a routine registers

A routine's settings are a scene of their own: a class extending `SettingsScene` from
`/modules/settings_page.be`, registered as the fourth argument. It answers `settings()` with the
values it edits, in the order the list shows them, and — when its values constrain one another —
`problem()` with the rule that has to hold:

```berry
class MySettings : SettingsScene
  def settings()
    return [
      setting("target", "Target", PressureEditor(1, false), 10.0),
      setting("hold", "Hold", DurationEditor(1, 0, 7200, false), 30),
      setting("mode", "Mode", ChoiceEditor(["hold", "cycle"], false), "hold"),
      setting("enabled", "Enabled", BoolEditor(true), true)
    ]
  end

  def problem(values)
    if values["hold"] > 3600
      return "HOLD > 1H"
    end
    return nil
  end
end

routines.register("mypage", MyIcon(), MyPage, MySettings)
```

A routine with nothing to edit registers `nil`. A subclass that defines only `settings()` inherits
the rows, the value scenes, the writes and the repair.

### A setting

`setting(key, label, editor, default[, icon])` builds one record:

| Field | Type | Meaning |
| --- | --- | --- |
| `key` | string | the key in the routine's `store` file |
| `label` | string | the line the editor draws beside the value |
| `editor` | an editor | what the value is, and how it is read, printed and edited |
| `default` | the kind's type, or `nil` | what the setting is when nothing has been stored; `nil` for a nullable one |
| `icon` | an `Icon`, or `nil` | drawn at the left of the row; an argument left off is a text-only row |

### Editors

The editor is the whole of what a value is. There is one class per kind in
`/modules/settings_editors.be`, and each keeps the rules its kind needs:

| Editor | `kind()` | Arguments |
| --- | --- | --- |
| `BoolEditor` | `bool` | `nullable` |
| `ChoiceEditor` | `choice` | `values`, `nullable` |
| `IntEditor` | `int` | `step`, `min`, `max`, `nullable` |
| `RealEditor` | `real` | `step`, `min`, `max`, `nullable` |
| `PressureEditor` | `pressure` | `step`, `nullable` |
| `DurationEditor` | `duration` | `step`, `min`, `max`, `nullable` |

An editor answers four questions: `read(name, key, default)` gives the stored value in the kind's
own type, or the default when nothing is stored; `value(raw, default)` holds a value that came from
a switch's map to the same rules; `text(value)` prints it as the list shows it; `lowest()` is the
value a null setting opens at; and `scene(label, value)` builds the value scene that edits it.
`kind()` names the kind for whatever has to know, which today is only the host harness.

A value the kind cannot accept is brought to what it can: an int or a real past its bounds is
clamped to the bound, a duration is held between `min` and `max`, a pressure goes through
`vacuum.clamp()`, and a choice that is not one of its values reads as the default. A kind that
cannot work at all — a choice with no values, bounds the wrong way round — says so where it is
built, logs one line, and leaves its row out of the list. Up/Down steps `real`, `int` and
`pressure`, toggles `bool`, and cycles `choice`; OK writes the value back with `store.set` and Back
leaves without writing.

A `nullable` setting whose key is unset — or holds an empty value — reads as `nil` whatever its
default says, and shows `--` in the list and the editor. It opens at the kind's lowest real value
(the value a non-nullable setting starts from), and `nil` sits one step below that: Down at the
lowest value clears back to `--`, Up from `--` enters the lowest value, and Down at `--` stays. OK
on `--` writes an empty value, which reads as `nil` again.

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
target; a nullable one reads as `nil`, and its lowest value is that same shallowest target. The
machine's own `min` … `floor` is the range, so a pressure editor takes no `min` or `max`.

### A rule across settings

`problem(values)` is asked with every value by key whenever the list opens and after every commit.
The scene never hands out a set the rule refuses: while it fails, the settings that differ from
their default are put back to them — the one just changed first, then the rest in the order the
list shows them — and each repair is written through the page's callback. The rule that was applied
is drawn in place of the list's title until the next press. So a file edited by hand is mended the
moment its settings are opened, and an edit that would break a rule lands on the default instead.

A page starting a run from a file nobody has opened settings on still refuses: `begin()` asks the
same rule of the values it read.

### Reading them

A page reads its values through `self.activation(settings)`: `value(key)` for a setting held to its
kind — from the switch's map when one was carried, from the routine's own file otherwise —
`raw(key)` for a rule the kind does not own, and `unset(key)` for a nullable setting that holds
nothing.

```berry
def read_settings(settings)
  var s = self.activation(settings)
  self.count = s.value("set_count")
  self.target = s.value("target_pressure")
  self.rest = s.value("rest_duration")
end
```
