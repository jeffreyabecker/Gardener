# Routines

The panel page is a Berry program. This is the contract a routine must satisfy: where it
lives, what it must look like, and when the firmware calls it. What a routine can call — the native
modules and the records they answer with — is [bindings.md](./bindings.md).

This file is the contract for **any** routine, not a description of the ones that ship. A page's
phases and its settings keys belong in that page's own `.be` file, its gestures in
[gestures.md](../gestures.md), and `firmware/gardener/data/routines/` holds the pages this build
ships when you want to read one. Nothing here should need to change when one of them changes.

The panel UI is a stack of **scenes**. A routine file registers the class its scene is built from;
the firmware polls the top scene and only the top scene, and a scene pushes or pops scenes with `ux`,
the stack `/system/ux.be` defines. The registry the routine browser draws is a Berry class too — the
global `routines` instance in `/system/registry.be` — so the only native additions over the older
page model are the `store.*` forms and the panel's own modules.

## Where a routine lives and what it must look like

Every file under `/routines/` ending in `.be` is loaded at boot. A file's top level runs once; it
registers an object and is done:

```berry
import string

class MyPage : Scene
  var routine_key

  def init(settings)
    super(self).init()
    self.routine_key = "mypage"
    # settings is the map the registry made clean: one value per setting the routine registers
  end
end

routines.register("mypage", "My page", MyIcon(), MyPage, MySettings)
return MyPage
```

A routine carries its own **routine key** in the `routine_key` field it sets in `init()`: the key is
its identity, it is what the store file is called, and it is what its settings are read and written
under, so the class that owns the key writes it once.

The second argument is the routine's **title**: the label the browser draws for its row. It is not
the identity — the routine key still is — so it may contain spaces and capital letters.

The third argument is the routine's **icon**: an instance of a class extending `Icon` from
`/system/components/icon.be`, which draws itself with the `screen` primitives and has an intrinsic
`w`. The browser draws it at the left of the routine's row and shifts the title right by
`icon.w + 2`. A routine that passes `nil` has a text-only row.

The fourth argument is the routine's **class**, not an instance: the registry builds a scene from it
every time the routine is pushed, so each visit starts from `init()` and nothing a previous visit
left behind survives. The scene that is showing is `ux.top()`. The class must be a scene:
`Scene` (`/system/scene.be`) provides `poll`, a gesture drain, the repaint cadence and the gesture
dispatch; a routine extends `Scene`, carries its own routine key in the `routine_key` field, and
defines `init(settings)` plus whatever `draw_page()` and `tick(now)` it needs. A scene that is
missing a method the stack calls raises when it is pushed or polled, which drops the stack and shows
the mark.

The fifth argument is the routine's **settings declaration** — the class that describes the values it
edits and any rule that binds them, or `nil` when it has none. It is asked through the class and
never built, so both of its answers are static (see below).

**Initializing the scene is the whole of its activation.** The registry builds the map the
constructor is given: every setting the routine registers, read from its own file, held to its kind,
and repaired by the routine's rule, so the map is complete and describes a set a run accepts. A
routine with no settings is given an empty map. The map belongs to that activation alone — the
runtime neither keeps it nor writes it anywhere — and a routine's `init(settings)` may rely on every
non-nullable key being present, with a nullable key that holds nothing reading `nil`.

A scene's `activate(args)` is the inherited no-op. A page that does its work in `init()` — every
shipped routine does — leaves it alone.

Every registration is listed: there is no hidden routine. The device's own scenes — the readout, the
system settings and the browser — are not registered at all: they are well-known scenes in
`/modules/ui.be` the system pushes directly.

The root scene — the readout — is the `__rootScene` global `/modules/ui.be` sets. If nothing sets
one, the panel shows the device's mark and nothing runs.

A routine's key is also the name of the file its values are kept in (see `store` in
[bindings.md](./bindings.md)), so it has to be a plain word: a key containing `/` or `\` is
refused at load.

Routines ship with the data image: put them in `firmware/gardener/data/routines/` and upload with
`python scripts/arduino.py --task data`.

## The system in `/system`

`/system` is what the device itself provides before a page exists: the records a native call
answers with, the components and the scene a page is built from, and the registry the browser
draws. It loads before `/modules` and `/routines`, and its order is stated rather than sorted.

`/system/order` is that order: one path per line, relative to `/system`, with `#` for a comment.
`components/text.be` and `scene.be` are the same kind of entry, so a system file may sit in a
subfolder. A `.be` file the order does not name does not load, and one found beside the named files
is reported as an error. **A new system file is an edit to `/system/order`**, at the point its
dependencies put it.

`/system/records.be` declares `reading`, `unit` and `bounds` — the classes the native calls answer
with. They are written in Berry because only the parser can declare an instance variable on a class.
The same file defines the
global `is_nil(a, b, ...)`: the first argument that is not nil, or nil when every one of them is,
so a stored-or-default choice reads `is_nil(stored, fallback)`. `/system/ux.be` is the scene stack —
`class Ux` and the global `ux`, the frames, the callbacks and the one-beat deferral, with the
firmware calling `reset`, `enter` and `poll` on it. `/system/scene.be` is
`Scene` — the gesture drain, the repaint cadence and the gesture dispatch a routine's scene extends.
`/system/registry.be` is the registry: `class RoutineRegistration`, the record a registration
builds — routine key, title, icon, factory and settings class — and `class RoutineRegistry` with the
global `routines` instance — the list the browser draws, the `load(routine_key)` that builds a
routine's clean settings map, and the `push(routine_key, callback)` that opens a routine.
`/system/list.be` is `List`, a selectable menu.

`/system/components/` is the drawing primitives a readout composes, one class per file:
`component.be` is `Component` (the `Position`/`Size` box and the `inverted` flip), `icon.be` is
`Icon` (a glyph drawn in its box), `text.be` is `Text` (the face choice, plus `text_settings`),
`layout.be` is `Layout` (a flat list of children drawn in order), `bubble.be` is `Bubble`,
`progress_bubbles.be` is `ProgressBubbles`, `progress_bar.be` is `ProgressBar`, `scrollbar.be` is
`Scrollbar`, `triangle.be` is `Triangle`, `pressure_display.be` is `PressureDisplay` and
`AutoPressureDisplay`, and `duration.be` is `Duration`. Every component is given its box at
construction (`pos` and `dim`) and keeps it; setting `inverted` on it flips that box as the last
thing its draw does — paper where its ink was — which is what a selected row, a cursor or a focused
readout looks like.

`/system/settings/` is the settings view. `settings.be` is the pair of duration helpers.
`scenes/` holds one scene per file: `value_scene.be` is `ValueScene` (the modal editor that owns the
keys while it is on top) and the value it pops on Cancel, `bool_scene.be`, `choice_scene.be`,
`real_scene.be`, `int_scene.be`, `pressure_scene.be` and `duration_scene.be` are the kinds,
`settings_scene.be` is `SettingsScene` (the list, handed its settings and the values keyed by setting
key).
`configs/` holds one setting per kind: `setting_cfg.be` is `SettingCfg` and
`bool_setting_cfg.be`, `choice_setting_cfg.be`, `int_setting_cfg.be`, `real_setting_cfg.be`,
`pressure_setting_cfg.be` and `duration_setting_cfg.be` are the kinds, each opening the value scene
of the same name. **`up:double`** is the standard gesture for opening the list; the page opens it
itself, from its own declaration and the state it is running:

```berry
def edit()
  ux.push(SettingsScene(RampedStaticSettings.settings(), self.serialize_to_settings(), nil), nil,
          / settings_values -> self.settings_closed(settings_values))
  return true
end
```

Nothing in the settings folder reads or writes anything on its own: `SettingsScene` edits the values
it was handed and pops them on `back:single`, and the caller writes them. A page has the state
already, so it opens the list on that state and reads no file; a caller with no state of its own —
the browser — reads the routine's document through the declaration, mends it when the routine's rule
refuses it, and passes the result in. Writing is the registry's
`save_settings(routine_key, settings_values[, settings_cfgs])`, which applies the rule and writes
every value.

`/system/jingles.txt` is data rather than code. It is not compiled or loaded as a `.be` file; the
native `jingles` binding reads it at boot and exposes its entries as `jingles.ids()`,
`jingles.title()`, `jingles.label()`, `jingles.notes()`, `jingles.current()` and
`jingles.play_current()`. Its format and the
call list are in [bindings.md](./bindings.md#jingles--the-note-library).

## Shared code in `/modules`

Every file under `/modules/` ending in `.be` is loaded after the system and before any file under
`/routines/`. A module file defines classes or functions and registers nothing: it has no page, no
store file and no entry in `routines.keys()`. It is not an `import` target either — `import`
reaches only the native modules such as `string` — its classes are simply globals by the time the
pages run.

Ship a module the same way as a routine: put it in `firmware/gardener/data/modules/` and upload the
data image.

Files within `/modules/` load in directory order, which is the alphabetical order the image is
packed in, so a module may only name a class a module that loads before it defines. That rule
reaches inside methods as well as the top level: Berry compiles a name when it reads the file, so a
reference to a later module's class is refused at load. Keep the dependency pointing at the earlier
file, or move the class into the file that uses it. Define classes and functions there; leave
constructing them to the page that uses them.

The device's own scenes ship in `/modules/ui.be` — the readout and the system settings — and
`/modules/browser.be` — the routine browser, whose instance `browserScene` that file builds before
`ui.be` compiles and names it. None of them registered. The system menu is itself a `SettingsScene`, over three settings that
read and write the device through `screen.unit()` / `device.set_unit()`, `device.bluetooth()` and
`device.jingle()` / `device.set_jingle()` rather than through a routine's file: a row there carries
its own `current()` and `write(value)`, the radio row's `write` is empty and its `scene()` is `nil`
so Forward has nothing to open, and the set it settled on is written when the menu leaves.

## When the firmware calls into a scene

| Moment | Call |
| --- | --- |
| boot, after the files load | the firmware pushes the root scene (`__rootScene`) |
| a scene is built and pushed | `init(settings)`, then `activate(args)` — `Scene`'s no-op unless the scene overrides it |
| every panel-task beat while the scene is on top | `poll(now)` |
| a scene is popped off the top | the pusher's callback runs with the returned value, or the scene below re-activates |

A scene built by the registry is a new instance every time, so its `init(settings)` runs once per
visit and nothing a previous visit left behind survives. `activate()` runs on every push and on
every return to a scene that was pushed without a callback; a routine page, which does all of its
work in `init()`, inherits it as a no-op. A re-entry does not repeat `init(settings)`, so a scene
whose values can change while it is not showing reads them again in the callback that brought it
back — the shipped pages call `routines.load()` from their settings callback for exactly that.

## Gestures

The top scene sees one gesture per poll as `ev` in `handle(ev)` — a whole string such as
`ok:single` or `cancel:double`. A scene registers its gestures declaratively in `init()`:

```berry
def init()
  super(self).init()
  self.on("ok:single", / -> self.start())
  self.on("cancel:double", / -> self.leave())
end
```

`on(gesture, fn)` appends the pair; the default `handle(ev)` compares each registered gesture whole
and runs the first action that returns true. A scene may instead override `handle_page(ev)` and
compare whole strings itself. Either way the match is whole or not at all: no `*`, no bare key, no
list of presses, and no splitting on `:` or `+`. The standard gestures are in
[gestures.md](../gestures.md); the shipped bindings are in [scenes.md](./scenes.md).

## The stack

The stack is `/system/ux.be`: `class Ux` and the global instance `ux`, in Berry. `ux.push(scene
[, args [, callback]])` puts a scene on top and makes it the one that is polled. `ux.pop([value])`
leaves it and hands `value` to the pusher's callback, or re-activates the scene below when it was
pushed without one. `ux.pop_to_root([value])` pops every scene above the root in one move — a
finished run's way home. The root is the `__rootScene` global `/modules/ui.be` sets; if nothing sets
it, the panel shows the device's mark. Eight scenes is the deepest the stack goes; a ninth push is
refused and logged.

Only the top scene polls and draws, so a scene below the top is suspended — it keeps its state and
resumes through its callback when the scene above pops. A push or pop asked for from inside a
scene's own `poll` is deferred until that call returns, so a scene may pop itself without the rest
of its `poll` — its draw — running over the page that came back. One held request is applied per
beat.

## One entry per beat

The runtime is the only thing that enters the interpreter, and it does so from its own frame — one
`be_pcall` per beat into `ux.poll(now)`, never from inside a Berry call. That call and the scene
calls it makes are the beat; the frames, the callbacks and the held push or pop are Berry's. It runs
on the panel task, and the one-writer rule is what keeps it lock-free: the machine is written only
through the command queue, and read dirty by any task. Nothing is suspended, and there are no locks
and no callbacks.

What that costs, stated plainly:

- **A `poll` that never returns blocks the panel loop.** The control loop and the wire run on their
  own tasks, so a slow routine cannot starve them. There is no instruction budget in this build, so
  returning is a convention the author keeps. Keep `poll` short and let it return.
- **`print` writes nowhere.** Use `vacuum.log(text)`; it reaches the serial line as a `[I]` record
  when the device's `log` set includes `I`.
- A routine that raises is **dropped**: one `[E]` line names the routine, the method and the
  message, and the page falls back to the mark until the next boot.

## The settings a routine registers

A routine's settings are a declaration of its own: a class registered as the fifth argument, holding
the values it edits in the order the list shows them and, when its values constrain one another, the
rule that has to hold. Both answers are static, because the class is asked and never built:

```berry
class MySettings
  static def settings()
    return [
      PressureSettingCfg("target", 10.0, "Target", nil, 1, false),
      DurationSettingCfg("hold", 30, "Hold", nil, 1, 0, 7200, false),
      ChoiceSettingCfg("mode", "hold", "Mode", nil, ["hold", "cycle"], false),
      BoolSettingCfg("enabled", true, "Enabled", nil, false)
    ]
  end

  static def problem(values)
    if values["hold"] > 3600
      return "HOLD > 1H"
    end
    return nil
  end
end

routines.register("mypage", "My page", MyIcon(), MyPage, MySettings)
```

A routine with nothing to edit registers `nil`. A class that is registered answers both: a routine
with no rule of its own returns `nil` from `problem`.

The list itself is `SettingsScene(settings_cfgs, settings_values, message)`: the settings declaration
for the rows and a map of values keyed by setting key, built by whoever opens it — the page's
`up:double`, or the browser's Forward on the routine's row. The scene edits the values it was handed
and pops them when Back is pressed; the caller applies the rule and writes every value. The scene
never touches the store, and nothing is written until the list closes.

### A setting

One class per kind in `/system/settings/configs/`. Every constructor takes the same four arguments
first, then the kind's own, and `nullable` last:

| Setting | Arguments |
| --- | --- |
| `BoolSettingCfg` | `key`, `default_value`, `label`, `icon`, `nullable` |
| `IntSettingCfg` | `key`, `default_value`, `label`, `icon`, `step`, `min`, `max`, `nullable` |
| `RealSettingCfg` | `key`, `default_value`, `label`, `icon`, `step`, `min`, `max`, `nullable` |
| `PressureSettingCfg` | `key`, `default_value`, `label`, `icon`, `step`, `nullable` |
| `DurationSettingCfg` | `key`, `default_value`, `label`, `icon`, `step`, `min`, `max`, `nullable` |
| `ChoiceSettingCfg` | `key`, `default_value`, `label`, `icon`, `values`, `nullable` |

| Field | Type | Meaning |
| --- | --- | --- |
| `key` | string | the key in the routine's `store` file |
| `default_value` | the kind's type, or `nil` | what the setting is when nothing has been stored; `nil` for a nullable one |
| `label` | string | the line the value scene draws beside the value |
| `icon` | an `Icon`, or `nil` | drawn at the left of the row; an argument left off is a text-only row |
| `nullable` | `bool` | whether the setting can hold nothing |

A choice's `values` is a list of the values the setting accepts — `["hold", "cycle"]` — or of
`[value, title]` pairs, where the value is what the file stores and the title is what the row and
the editor print: `[["poacher", "Poacher"], ["alarm", "Alarm"]]`. A pair lets a setting hold a short
identifier and show a name; a plain value is its own title.

### What a setting does

The setting is the whole of what its value is: the rules its kind needs, and the value scene that
edits it. It answers four questions:

- `read(routine_key)` — the stored value from the file it is given, held to the kind, or the default
  when nothing is stored.
- `value(raw)` — a raw value held to the same rules, which is what a value read from a file or
  carried in a map ends up as.
- `text(value)` — the value as the list shows it.
- `scene(value)` — the value scene that edits it.

The store hands a value back as the text the file holds, so the kind parses it: an int or a duration
through `int()`, a real or a pressure through `real()`, a bool from `true`/`1`, a choice from its own
list.

Which file a value belongs to is the caller's: the setting reads the routine key it is handed, and
the caller writes what the list settles on.

A value the kind cannot accept is brought to what it can: an int or a real past its bounds is
clamped to the bound, a duration is held between `min` and `max`, a pressure goes through
`vacuum.clamp()`, and a choice that is not one of its values reads as the default. A kind that
cannot work at all — a choice with no values, bounds the wrong way round — says so where it is
built, logs one line, and leaves its row out of the list. Up/Down steps `real`, `int` and
`pressure`, toggles `bool`, and cycles `choice`; `ok:single` keeps the value and `cancel:single`
discards it.

A `nullable` setting whose key is unset — or holds an empty value — reads as `nil` whatever its
default says, and shows `--` in the list. Opening its row hands the scene the stored value, or the
setting's default when it holds nothing — so a nullable setting with no default opens at `--`.
`nil` sits one step below the kind's lowest real value: Down at the lowest value clears back to
`--`, Up from `--` enters the lowest value, and Down at `--` stays. OK on `--` hands back nothing,
which the caller writes as an empty value and reads back as `nil`.

A `duration` is stored as whole seconds and shown as `2m30s`, `45s` under a minute or `1m` on a
whole minute. `ok:single` opens it in the **minutes** stage, Up/Down moves that by a minute,
`forward:single` moves on to the **seconds** stage, and Up/Down moves that by `step` seconds
(default 1); `ok:single` then keeps it, and `back:single` steps back to the minutes stage.
`cancel:single` discards from either stage. The value is held between `min` and `max`, and never
below zero. A nullable duration cleared from the minutes stage advances to the seconds stage still
`nil`, and the second OK keeps the clear.

A `pressure` is stored in kPa, the machine's unit, and shown and stepped in the unit on the glass:
`12.5` in psi, `0.12` in inHg or `40` in mmHg. The unit itself is the overlay's, printed in its
bottom band rather than beside any number, so a page or the value scene draws the value alone.
Up/Down moves it by `step` whole display steps — one psi, one inHg, ten mmHg — and every value goes
through `vacuum.clamp()`, whether it was read, stepped or restored, so the scene cannot produce a
target the machine refuses. An unset pressure reads as `0.0` and therefore clamps to the shallowest
legal target; a nullable one reads as `nil`, and its lowest value is that same shallowest target.
The machine's own `min` … `floor` is the range, so a pressure setting takes no `min` or `max`.

### A rule across settings

`problem(values)` is asked with every value by key when the list opens and again when it closes, and
when the registry builds the clean map a routine is initialized from. While the rule refuses the set,
the settings that differ from their default are put back to them in the order the list shows them,
so what is written is always a set the rule accepts. The reason that was applied is drawn in place
of the list's title until the first value is committed. So a file edited by hand is shown mended the
moment its settings are opened and written mended when the list closes; an edit that would break a
rule is put back when the list closes, and a page is never initialized with a set the rule refuses.

### Reading and writing them

A page does not read its file at all: the registry's `load(routine_key)` reads every setting it
registers, holds each to its kind and repairs the set, and the page's `init(settings)` is handed the
result as a map keyed by setting key. A page takes what it needs from that map in
`init_from_settings(settings_values)`, and a value is read from it directly:

```berry
def init_from_settings(settings_values)
  self.count = settings_values["set_count"]
  self.target = settings_values["target_pressure"]
  self.rest = settings_values["rest_duration"]
  # a nullable setting that holds nothing arrives as nil
  self.floor = vacuum.clamp(0.0)
  if settings_values["floor_pressure"] != nil
    self.floor = settings_values["floor_pressure"]
  end
end
```

`serialize_to_settings()` is the other direction: the values the page is holding, as a map of the
same shape and keyed the same way, so a set can be written back the way it was read — and so the
settings list can be opened on it as it stands. It runs no rule and writes nothing: a page's state is
one it is already running, so there is nothing to mend. A page that holds a duration in milliseconds
writes it back in whole seconds, which is what the store holds, so the two halves of one key agree.
