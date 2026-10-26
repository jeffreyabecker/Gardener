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
global `routines` instance in `/system/registry.be` — so the only native addition over the older
page model is the `device` module's document call beside the panel's own modules.

## Where a routine lives and what it must look like

Every file under `/routines/` ending in `.be` is loaded at boot, in name order. A file's top level
runs once; it registers an object and is done:

```berry
import string

class MyPage : Scene
  var routine_key

  def init(activation)
    super(self).init()
    self.routine_key = activation.routine_key
    # activation.settings_values is the map the registry made clean: one value per setting
  end
end

routines.register(MySettings("mypage", "My page", MyIcon(), MyPage))
return MyPage
```

A routine carries its own **routine key** in the `routine_key` field its `init()` takes from the
activation: the key is its identity, it is what the store file is called, and it is what its settings
are read and written under. The registry passes the key the routine was launched under, so a page
that extends another inherits its `init()` and is still launched under its own key.

The **title** is the label the browser draws for its row. It is not the identity — the routine key
still is — so it may contain spaces and capital letters.

The **icon** is an instance of a class extending `Icon` from `/system/components/icon.be`, which
draws itself with the `screen` primitives and has an intrinsic `w`. The browser draws it at the left
of the routine's row and shifts the title right by `icon.w + 2`. A routine that passes `nil` has a
text-only row.

The **factory** is the routine's class, not an instance: the registry builds a scene from it every
time the routine is pushed, so each visit starts from `init()` and nothing a previous visit left
behind survives. The scene that is showing is `ux.top()`. The class must be a scene: `Scene`
(`/system/scene.be`) provides `poll`, a gesture drain, the repaint cadence and the gesture dispatch;
a routine extends `Scene`, carries its own routine key in the `routine_key` field, and defines
`init(activation)` plus whatever `draw_page()` and `tick(now)` it needs. A scene that is missing a
method the stack calls raises when it is pushed or polled, which drops the stack and shows the mark.

The **settings declaration** is the record's own: the values the routine edits in `settings_cfgs`,
and any rule that binds them in `problem(values)`. The record is built where the file registers, so
the declaration is made once, at registration time (see below).

**Initializing the scene is the whole of its activation.** The constructor is handed a
`RoutineActivation`: the routine key and the map `load_settings` built — every setting the routine
registers, read from its own file, held to its kind, and repaired by the routine's rule, so the map
is complete and describes a set a run accepts. A routine with no settings is given an empty map. The
map belongs to that activation alone — the runtime neither keeps it nor writes it anywhere — and a
routine's `init(activation)` may rely on every non-nullable key being present, with a nullable key
that holds nothing reading `nil`.

A scene's `activate(args)` is the inherited no-op. A page that does its work in `init()` — every
shipped routine does — leaves it alone.

Every registration is listed: there is no hidden routine. The device's own scenes — the home scene
and the browser — are not registered at all: they are well-known scenes — `/modules/ui.be` and
`/modules/browser.be` — the system pushes directly.

The root scene — the home scene — is the `__rootScene` global `/modules/ui.be` sets. If nothing sets
one, the panel shows the device's mark and nothing runs.

A routine's key is also the name of the file its values are kept in (see `device.config` in
[bindings.md](./bindings.md)), so it has to be a plain word: a key containing `/` or `\` is
refused at load. The file's global section is the routine's own set; every named section is an
alternate set, an alias the registry lists beside the routine and that launches the same page from
that section's values.

Routines ship with the data image: put them in `firmware/gardener/data/routines/` and upload with
`python scripts/arduino.py --task data`.

A routine that needs more than one file states their order with a numeric prefix, because the folder
loads in name order and Berry resolves a global when it compiles a file: a file may name only what
an earlier one defined, inside method bodies as well as at the top level. A file that another file
names registers nothing, so it has no page, no store file and no entry in `routines.keys()`. The
interval cycle is the worked example: `0_ramped_interval_machine.be` is the phase machine both
pages hold, `1_ramped_interval_chrome.be` the chrome they draw, and `2_ramped_interval.be` and
`3_interval.be` the two pages, each with its glyph, its settings declaration and its registration.
The static profile follows the same numbering: `4_ramped_static_machine.be` is its phase machine and
`5_ramped_static.be` the page that holds it.

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
`class Ux` and the global `ux`, the frames, the callbacks, the one-beat deferral and `box()`, which
answers the box of the scene on top, with the firmware calling `reset`, `enter` and `poll` on it.
`/system/scene.be` is `Scene` — the box a scene draws in (`pos` and `dim`), the gesture drain, the
repaint cadence and the gesture dispatch a routine's scene extends.
`/system/registration.be` is `Registration`, the record a routine registers: its key, title, icon,
scene class, settings declaration and the alternate sets its document names, with the
`problem(values)` rule, the `repair(section, values)` that applies it, and the
`load_settings(section)` and `save_settings(section, values)` that read and write its values.
`RoutineAlias` is one alternate — the key its section gives it and the title its row shows.
`/system/registry.be` is the registry: `class RoutineRegistry` with the global `routines` instance —
the registrations in one list in registration order, the list the browser draws, the
`load_settings(routine_key)` and `save_settings(routine_key, settings_values)` that hand a routine's
set to its registration, and the `push(routine_key, callback, activation)` that opens a routine.
`/system/list.be` is `List`, a selectable menu: a selected index into rows walked with Up/Down and
wrapped at the ends, drawn with one highlighted row and a scroll window, feeding an optional
`Scrollbar`. A row is a `ListRow` — the icon drawn at its left, the title drawn beside it, and the
value the row stands for — so a caller keeps the thing a row means in the row rather than in a list
beside it. `list_rows()` holds a row written as a two-element `[icon, title]` pair, or as a bare
title, as a `ListRow` with no value. `set(rows)` normalizes whatever it is given and `selected_row()`
answers the row the index names. The list draws the icon and the title and never reads the value.
`/system/home_machine.be` is `HomeMachine`, the run the home scene holds: the phase it is in — `IDLE`
or `RUNNING`, read from the chamber every beat — the clock the page shows beside it, and the `start`
and `stop` commands that move it.

`/system/components/` is the drawing primitives the home scene composes, one class per file:
`component.be` is `Component` (the `Position`/`Size` box and the `inverted` flip), `icon.be` is
`Icon` (a glyph drawn in its box), `text.be` is `Text` (the face choice, plus `text_settings`),
`layout.be` is `AutoLayout` and the `bounds` a child declares (a layout resolves each child's
fraction of its own box and draws the children in order), `progress_bar.be` is
`ProgressBar`, `scrollbar.be` is `Scrollbar`, `triangle.be` is `Triangle`, `pressure_display.be` is
`PressureDisplay` and `AutoPressureDisplay`, and `duration.be` is `Duration`. A component takes no
box at construction: `set_box(pos, dim)` sets both, and the layout, or the caller for a component
that stands alone, calls it before the first draw. Setting `inverted` on a component flips its box
as the last thing its draw does — paper where its ink was — which is what a selected row, a cursor
or a focused readout looks like.

`/system/settings/` is the settings view. `settings.be` is the pair of duration helpers, the
`ChoiceItem` record a choice's entries are held as, `choice_items()` which normalizes a
declaration's list into them — giving a nullable setting's list the `--` entry it opens on unless
the declaration named one — and `ChoiceRing`, the entries a value editor steps through together
with the index of the one showing. `view.be` is `SettingsView`, the list's own geometry: the row a
setting is drawn on, the gap under its heading and the window a box holds.

`edit_scene.be` is `EditScene`, the modal editor: it owns the keys while it is on top, frames the
content a kind measures, and pops what OK holds or what Cancel was opened on. It draws nothing
itself, so every kind measures and draws the whole of its own content — label row, value and hint
row — and nothing about a kind's look is inherited. `edit_cfg.be` is `EditCfg`, the setting: the row
it draws, its default, and the rules its kind is read, held and printed by. Each kind is one file
holding both halves, its scene first and its cfg after it — `bool.be`, `choice.be`, `real.be`,
`int.be`, `pressure.be` and `duration.be` — and `settings_edit.be` is `SettingsEditScene`, the list,
handed its settings and the values keyed by setting key. **`up:double`** is the standard gesture for
opening the list; the page opens it itself, from its own declaration and the state it is running:

```berry
def edit()
  var settings_cfgs = routines.settings_cfgs(self.routine_key)
  ux.push(SettingsEditScene(settings_cfgs, self.serialize_to_settings(), nil), nil,
          / settings_values -> self.settings_closed(settings_values))
  return true
end
```

Nothing in the settings folder reads or writes anything on its own: `SettingsEditScene` edits the
values it was handed and pops them on `back:single`, and the caller writes them. A page has the
state already, so it opens the list on that state and reads no file; a caller with no state of its
own — the browser — reads the routine's document through `routines.load_settings(routine_key)`,
which mends it when the routine's rule refuses it, and passes the result in under the routine's
title. Writing is the registry's `save_settings(routine_key, settings_values)`, which hands the set
to the routine's registration: the rule is applied and the whole set written to the routine's
document in one `save()`, so a set that changes nothing costs no write. The browser then launches
the routine from the set the list settled on, so editing a routine's settings there is the way into
the routine rather than a return to the browser's rows.

`/cfg/jingles.ini` is data rather than code. It is not compiled or loaded as a `.be` file; the
device binding takes it at boot and answers its entries as `jingle` records from
`device.all_jingles()`, and `device.play_current_jingle()` sounds the selected one. Its format and
the record's fields are in [bindings.md](./bindings.md#the-jingle-library).

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

The device's own scenes ship in `/modules/`, one concern per file: `/modules/ui.be` is the home
scene, `/modules/settings_device.be` the settings list it opens, `/modules/jingles_edit.be` the
library face the jingle row in that list is drawn from, and `/modules/browser.be` the routine
browser, whose instance `browserScene` that file builds before `ui.be` compiles and names it. Loaded
in the alphabetical order the folder gives — `browser`, `jingles_edit`, `settings_device`, `ui` —
each is a global by the time the file that names it compiles. The browser file also defines
`RoutineActivation`, the record a launch hands a routine's class: the routine key and the clean
settings map.
None of them registered. The home scene holds a set of its own and opens the same list a page does,
over `DeviceSettings`: the target it works to, the unit it prints, the jingle it finishes with and
the radio as a build fact. Every one of those is a device setting rather than a document, so the
home scene fills the set in from `device.target()`, `screen.unit()`, `device.jingle()` and
`device.bluetooth()`, and writes it back through `device.set_target()`, `device.set_unit()` and
`device.set_jingle()` when the list commits. The rows only declare their kinds — the radio row's
`scene()` is `nil`, so Forward on it opens nothing — and the home scene is what reads and writes
them.

## When the firmware calls into a scene

| Moment | Call |
| --- | --- |
| boot, after the files load | the firmware pushes the root scene (`__rootScene`) |
| a scene is built and pushed | `init(activation)`, then `activate(args)` — `Scene`'s no-op unless the scene overrides it |
| every panel-task beat while the scene is on top | `poll(now)` |
| a scene is popped off the top | the pusher's callback runs with the returned value, or the scene below re-activates |

A scene built by the registry is a new instance every time, so its `init(activation)` runs once per
visit and nothing a previous visit left behind survives. `activate()` runs on every push and on
every return to a scene that was pushed without a callback; a routine page, which does all of its
work in `init()`, inherits it as a no-op. A re-entry does not repeat `init(activation)`, so a scene
whose values can change while it is not showing reads them again in the callback that brought it
back — the shipped pages keep the settled set their settings list pops in that callback, and do not
read the file back.

## Gestures

The top scene sees one gesture per poll as `ev` in `handle(ev)` — a whole string such as
`ok:single` or `cancel:double`. A scene answers the ones it knows and drops the rest, comparing the
whole string:

```berry
def handle(ev)
  if ev == "ok:single"
    return self.start()
  end
  if ev == "cancel:double"
    return self.leave()
  end
  return false
end
```

The match is whole or not at all: no `*`, no bare key, no list of presses, and no splitting on `:`
or `+`. `Scene.handle(ev)` answers nothing, so a scene that knows no keys writes nothing. The
standard gestures are in [gestures.md](../gestures.md); the shipped bindings are in
[scenes.md](./scenes.md).

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

A routine's settings are part of its registration: the file builds one `Registration` — extending it
when the routine declares values or a rule — and hands it to `routines.register`. The record holds
the values it edits in `settings_cfgs`, in the order the list shows them, and, when its values
constrain one another, the rule that has to hold in `problem(values)`. It is built where the file
registers, so the declaration is made once, at registration time:

```berry
class MySettings : Registration
  def init(routine_key, title, icon, factory)
    super(self).init(routine_key, title, icon, factory, [
      PressureEditCfg("target", 10.0, "Target", nil, 1, false),
      DurationEditCfg("hold", 30, "Hold", nil, 1, 0, 7200, false),
      ChoiceEditCfg("mode", "hold", "Mode", nil, ["hold", "cycle"], false),
      BoolEditCfg("enabled", true, "Enabled", nil, false)
    ])
  end

  def problem(values)
    if values["hold"] > 3600
      return "HOLD > 1H"
    end
    return nil
  end
end

routines.register(MySettings("mypage", "My page", MyIcon(), MyPage))
```

A routine with nothing to edit registers a bare `Registration` over an empty list. An instance that
is registered answers both: a routine with no rule of its own returns `nil` from `problem`.

The list itself is `SettingsEditScene(settings_cfgs, settings_values, title)`: the settings
declaration for the rows, a map of values keyed by setting key, and the title drawn above them, or
`nil` for none. It is built by whoever opens it — the page's `up:double`, or the browser's Forward
on the routine's row. The scene edits the values it was handed and hands a set back on the way out:
what it was edited to when **OK** kept it, and the set as it was opened when **Back** or **Cancel**
refused it. The caller applies the rule and writes what it is given, so a refused edit writes back
the values it opened with. The scene never touches the store, and nothing is written until the list
closes.

A list that fits in the page draws a centered block the size of its rows; a title, when there is one,
is the block's first line. A longer list fills the page and scrolls, keeping its heading — the title,
or `SETTINGS` when it has none.

### A setting

One class per kind in `/system/settings/`, its `<Kind>EditCfg` in the same file as its
`<Kind>EditScene`. Every constructor takes the same four arguments first, then the kind's own, and
`nullable` last:

| Setting | Arguments |
| --- | --- |
| `BoolEditCfg` | `key`, `default_value`, `label`, `icon`, `nullable` |
| `IntEditCfg` | `key`, `default_value`, `label`, `icon`, `step`, `min`, `max`, `nullable` |
| `RealEditCfg` | `key`, `default_value`, `label`, `icon`, `step`, `min`, `max`, `nullable` |
| `PressureEditCfg` | `key`, `default_value`, `label`, `icon`, `step`, `nullable` |
| `DurationEditCfg` | `key`, `default_value`, `label`, `icon`, `step`, `min`, `max`, `nullable` |
| `ChoiceEditCfg` | `key`, `default_value`, `label`, `icon`, `values`, `nullable` |

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

The setting normalizes whatever it is given into `ChoiceItem` records, and `cfg.values` is that list
of records: `key` is the value the setting stores, `title` the label the row and the editor print —
the key itself when the declaration gave none — and `icon` is a glyph to draw beside it. A
declaration may hand over records directly when it needs those. A nullable setting's list begins
with a record whose key is nil and whose title is `--`, unless the declaration named that entry
itself: it is the "no choice" the editor opens on, and the row prints it too.

`extra_actions` is the other thing a record carries: a dict of gesture to callback for the gestures
that entry answers itself. The value editor keeps its own four gestures, and any other is offered to
the choice that is showing, with that choice handed to the callback — so an entry can offer an action
of its own without the editor having to know about it.

### What a setting does

The setting is the whole of what its value is: the rules its kind needs, and the edit scene that
edits it. It answers three questions:

- `value(raw)` — a raw value held to the kind's rules, which is what a parsed value from a file or a
  value carried in a map ends up as.
- `text(value)` — the value as the list shows it.
- `scene(value)` — the edit scene for it.

The store hands a value back as the text the file holds, so the kind parses it — `parse(text)`, one
per kind: an int or a duration through `int()`, a real or a pressure through `real()`, a bool from
`true`/`1`, a choice from its own list — and `value` holds the result. The registry does the
reading: `load_settings` opens the routine's document once with `device.config` and asks it for each
declared key, so a setting names no file and opens none. Which file a value belongs to is the
caller's, and the caller writes what the list settles on.

A value the kind cannot accept is brought to what it can: an int or a real past its bounds is
clamped to the bound, a duration is held between `min` and `max`, a pressure goes through
`vacuum.clamp()`, and a choice that is not one of its values reads as the default. A kind that
cannot work at all — a choice with no values, bounds the wrong way round — says so where it is
built, logs one line, and leaves its row out of the list. Up/Down steps `real`, `int` and
`pressure`, and cycles `bool` and `choice`; `ok:single` keeps the value the editor holds and
`cancel:single` hands back the value it opened with, which leaves the row as it stands.

A `nullable` setting whose key is unset — or holds an empty value — reads as `nil` whatever its
default says, and shows `--` in the list. Opening its row hands the scene the stored value, or the
setting's default when it holds nothing — so a nullable setting with no default opens at `--`.
On a kind with a ladder of values — `int`, `real`, `pressure` and a `duration` — `nil` sits one
step below the lowest of them: Down at the lowest value clears back to `--`, Up from `--` enters the
lowest value, and Down at `--` stays. A `bool` and a `choice` are rings instead, and `--` is an
entry of the ring: Up past the last entry wraps round to `--` and Down at `--` wraps to the last
entry, so stepping never stops. Every one of these hands back nothing when OK is pressed on `--`,
which the caller writes as an empty value and reads back as `nil`.

A `duration` is stored as whole seconds and shown as `2m30s`, `45s` under a minute or `1m` on a
whole minute. `ok:single` opens it in the **minutes** stage, Up/Down moves that by a minute,
`forward:single` moves on to the **seconds** stage, and Up/Down moves that by `step` seconds
(default 1); `ok:single` then keeps it, and `back:single` steps back to the minutes stage.
`cancel:single` hands back the value the editor opened with, from either stage. The value is held
between `min` and `max`, and never
below zero. A nullable duration cleared from the minutes stage advances to the seconds stage still
`nil`, and the second OK keeps the clear.

A `pressure` is stored in kPa, the machine's unit, and shown and stepped in the unit on the glass:
`12.5` in psi, `0.12` in inHg or `40` in mmHg. The unit itself is the band's, printed in the
bottom band rather than beside any number, so a page or the value scene draws the value alone.
Up/Down moves it by `step` whole display steps — one psi, one inHg, ten mmHg — and every value goes
through `vacuum.clamp()`, whether it was read, stepped or restored, so the scene cannot produce a
target the machine refuses. An unset pressure reads as `0.0` and therefore clamps to the shallowest
legal target; a nullable one reads as `nil`, and its lowest value is that same shallowest target.
The machine's own `min` … `floor` is the range, so a pressure setting takes no `min` or `max`.

### A rule across settings

`problem(values)` is asked with every value by key when the list opens and again when it closes, and
when the registration builds the clean map a routine is initialized from. While the rule refuses the
set, the settings that differ from their default are put back to them in the order the list shows
them, so what is written is always a set the rule accepts. The reason that was applied is logged, not
drawn: the list's heading is the routine's title. So a file edited by hand is mended the moment its
settings are opened and written mended when the list closes; an edit that would break a rule is put
back when the list closes, and a page is never initialized with a set the rule refuses.

### Reading and writing them

A page does not read its file at all: the registry's `load_settings(routine_key)` asks the routine's
registration for the set, which reads every setting it declares, holds each to its kind and repairs
it, and the page's `init(activation)` is handed the result as `activation.settings_values`, a map
keyed by setting key. The root config lives in the
document's global section; every named section is an alias — a second key that launches the same
page from that section's set, and whose map also carries its `routine_title` so the page can show
its own name. A page takes what it needs
from that map in
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
