# Bindings

The native surface a routine calls: two modules — `vacuum` and `device` — the records those calls
answer with, and the `Config` document instance `device.config` hands back. This is the reference
for what a routine can call; for what a routine must be, see [routine.md](./routine.md).

Nothing here draws. A page is a scene the stack paints, so the panel has no verbs of its own and a
page that needs to draw does it in C++, where the canvas is.

Three of the names a scene uses are Berry classes rather than native modules: the registry the
routine browser draws (`/system/registry.be`), the record it registers (`/system/registration.be`)
and the scene stack it pushes onto (`/system/ux.be`). They are documented here too, because a
routine calls them the same way.

## Values

Berry's numbers here are 32-bit integers and single-precision reals.

Several of the calls answer a **record**: a small object whose fields are read with a dot. Two of
them answer a list of records.

```berry
var u = device.unit()
var suffix = u.suffix                # not u["suffix"]
```

A record is a value as it stood when the call returned, not a live view: writing a field
(`u.suffix = "kPa"`) changes the routine's copy and nothing else. Reading a field the record does not
have raises `attribute_error`, and the fields are the ones listed under each call. The classes are
declared in `/system/records.be`, which the image loads before any page.

A routine's settings are not native records: each one is a setting config the routine builds from
`/system/settings/`, so it carries the key, the default and the kind's own rules rather
than being answered by a call. The classes and their arguments are listed under the settings a
routine registers in [routine.md](./routine.md).

## `vacuum` — the machine

| Call | Returns | Notes |
| --- | --- | --- |
| `vacuum.set_target_pressure(pressure_kpa)` | — | sets the depth to pull to, in kPa below ambient; `0` is the ambient target and releases the chamber. A value outside `0` … `get_deepest_pressure()` is ignored by the machine. A shallower target while pulling or holding vents to it; a deeper one pulls. Both return to the run |
| `vacuum.get_target_pressure()` | `real` | the target the machine holds; `0` is the ambient target, which keeps the chamber released |
| `vacuum.get_current_pressure()` | `real` | the depth below ambient as the sensor reports it, `0` while there is no valid reading |
| `vacuum.get_deepest_pressure()` | `real` | the deepest target the machine accepts |
| `vacuum.get_shallowest_pressure()` | `real` | the shallowest non-zero target the machine accepts |
| `vacuum.clamp(pressure_kpa)` | `real` | the same number held to those two bounds. See below |
| `vacuum.start()` | `bool` | false when the machine refuses: no valid reading, or a latched fault. Valid at any target, `0` included |
| `vacuum.stop()` | — | stops the pump and vents. The target is kept |
| `vacuum.reset()` | — | clears a latched fault, then vents |
| `vacuum.state()` | string | `IDLE`, `PULL`, `HOLD`, `VENT` or `FAULT`. A `VENT` from `stop()` ends at `IDLE`; a target vent returns to `PULL`/`HOLD` |

Every pressure is kPa whatever the panel is printing. `device.unit()` says what the glass shows.

A command joins the control task's queue and is applied on its next beat, so
`set_target_pressure()` followed by `get_target_pressure()` answers the target from before the call.
`start()` is the one that answers a fact about the chamber: its refusal is read before anything is
queued. A latched fault reads as `FAULT` from `state()` until `reset()` clears it.

`vacuum.clamp(pressure_kpa)` is the bounds rule in one place: below the shallowest it answers the
shallowest, above the deepest it answers the deepest, otherwise the number it was given. A target
editor steps through it, a stored target is restored through it, and a page that reads a pressure to
start from uses it too — so no routine carries its own copy of the two bounds:

```berry
var start = vacuum.clamp(vacuum.get_current_pressure())   # a safe starting point for the editor
vacuum.set_target_pressure(vacuum.clamp(stored))          # and a stored one it cannot trust yet
```

The shallowest is a non-zero target: `vacuum.set_target_pressure(shallowest)` is taken, anything
between 0 and it is not, and `0` is the ambient target — the chamber released — and is always taken.

## `screen` — the drawing a page describes

A call does not draw. It answers a `draw` record — one node of a tree that says what the page looks
like — and the scene renders that tree: a page answers its root from `draw_page()`, and the node is
drawn on the page's canvas, the panel less the band's rows. Every node carries `kind` and the fields
its kind uses; the rest are nil, and a node the drawing does not know draws nothing.

| Call | Node |
| --- | --- |
| `screen.text(x, y, w, h, value[, font[, scale]])` | `text`: the box, `value`, `font`, `scale` |
| `screen.centred(x, y, w, h, value[, font[, scale]])` | `centred`: the same fields, drawn across and down the box |
| `screen.icon(x, y, w, h, glyph)` | `icon`: the box, and the `glyph` node drawn inside it |
| `screen.bar(x, y, w, h, percent)` | `bar`: the box and `percent` |
| `screen.scrollbar(x, y, w, h, index, count, visible[, hollow])` | `scrollbar`: the box and the four numbers |
| `screen.duration(x, y, w, h, format, seconds[, font[, known]])` | `duration`: the box, `format`, `seconds`, `font` and `known` |
| `screen.pressure_readout(x, y, w, h[, size_hint[, font[, scale[, secondary_scale]]]])` | `pressure_readout`: the box, `size_hint`, `font`, `scale` and `secondary_scale` |
| `screen.layout(w, h, children[, alignment])` | `layout`: the cell it takes and the children it divides |
| `screen.row(h, children[, alignment])` | `row`: the height it takes, children stacked across |
| `screen.column(w, children[, alignment])` | `column`: the width it takes, children stacked down |

A leaf's `x` and `y` are pixels from the corner of the canvas it is drawn in, and a side it leaves
nil is the whole of that canvas — so a node inside a layout fills the cell it was given without the
page computing a number, and `screen.centred(nil, nil, nil, nil, "Gardener")` is a string centred in
whatever box it finds itself in. A layout's, a row's and a column's `w` and `h` are the share they
declare of what holds them, nil for the whole side, so a row declares `h` and a column `w`.
`children` is a list of `[bounds, draw]` pairs — the cell a child declares and the child itself —
and `alignment` is the record above, `alignment("center", "center")`, or nil for the left and the
top.

`font` is a face name, or nil for the built-in 5x7, and `scale` is 1 unless it is given. `format` is
`minutes`, `seconds` or `hours`, `seconds` a count of them, and `known` is true unless it is given —
a count that is not known draws `--`. A face, a format or a scale the drawing does not know raises at
the call that made it, so a tree built wrong fails where it was written rather than where it is
drawn.

The readout is the machine's own: it prints the depth in force as the reading and the target as a
second one, both in the panel's unit, so the node says where the readout goes rather than what it
shows. `scale` is 2 and `secondary_scale` 1 unless they are given, and `size_hint` is 0.5: below 1
the second reading sits small in the bottom right of the box, and at 1 or more the two sit side by
side at the main scale, the second on the right.

The calls that answer a value rather than a node are the arithmetic and the text:

| Call | Returns | Notes |
| --- | --- | --- |
| `screen.duration_text(format, seconds[, known])` | string | the text a `duration` node would draw |
| `screen.measure(value[, font[, scale]])` | a `bounds` record | the ink the string takes, which is what a page places a second string by |
| `screen.cell(total, declared)` | int | the pixels a declared share takes of a total: nil for the whole, a share rounded to the nearest pixel, or a count clamped to the total |
| `screen.align(leftover, where)` | int | the offset an alignment takes of the room left over; `where` is `left`, `center`, `right`, `top` or `bottom` |
| `screen.width()`, `screen.height()` | int | the panel's size in pixels |

The faces a `font` may name:

| Name | Face |
| --- | --- |
| `sans9`, `sans12`, `sans18`, `sans24` | FreeSans at 9, 12, 18 and 24pt |
| `sansbold9`, `sansbold12`, `sansbold18`, `sansbold24` | FreeSansBold at the same sizes |
| `mono9`, `mono12`, `mono18`, `mono24` | FreeMono at the same sizes |
| `monobold9`, `monobold12`, `monobold18`, `monobold24` | FreeMonoBold at the same sizes |
| `tomthumb`, `picopixel` | the 3x5 and 3x6 bitmap faces |

`nil`, `""` and `"builtin"` are the built-in 5x7, the face that carries the arrow glyphs a hint row
prints.

`draw_page()` answers the root of the tree and the scene draws it in the page, so a page that draws
nothing answers `nil` — the base's empty return is already that. Everything in the tree is drawn in
the one call, which is why a string is safe to hand over: the record holds it for as long as the
drawing runs.

## `device` — the device around the machine

The unit the panel prints, the target the default program restores, the selected jingle, the piezo,
the clock, the log and the presses a page has been given. The first three belong to the device rather
than to a routine's own document, and all are keys of `/cfg/settings.ini`'s `[device]` section:
`unit`, `target` and `jingle`. The unit is the same switch the serial `device` domain carries; the
diagnostics switch is not here, because it belongs to the serial link rather than to the glass. The
radio is not a setting: `device.bluetooth()` reports whether the image was built with it.

| Call | Returns | Notes |
| --- | --- | --- |
| `device.unit()` | a `unit` record | the unit in force, out of the same table `units()` lists |
| `device.units()` | list of `unit` records | every unit `device set unit` accepts, in `panel/Units.h` order, each carrying the fields `device.unit()` answers |
| `device.set_unit(token)` | `bool` | applies the unit and writes it to `/cfg/settings.ini` under `[device]`; `token` is a unit's `name`, and false for one that is not in `units()` |
| `device.target()` | `real` | the default program's target in kPa, or **0 when none is set** |
| `device.set_target(pressure_kpa)` | — | stores a new target in `/cfg/settings.ini` under `[device]`; clamp it with `vacuum.clamp()` first |
| `device.bluetooth()` | `bool` | whether this build has the radio; `true` unless the image was compiled without it |
| `device.links()` | a `links` record | the two ways in, `serial` and `bluetooth`, each `true` while its link is up |
| `device.jingle()` | string | the selected jingle identifier, or `""` when none has been chosen |
| `device.set_jingle(id)` | — | stores the identifier as `[device] jingle` in `/cfg/settings.ini` |
| `device.all_jingles()` | list of `jingle` records | every jingle this build can play, in file order |
| `device.play_current_jingle()` | `bool` | plays the jingle `[device] jingle` selects, falling back to the first entry; `false` when the library is empty |
| `device.play_sequence(notes)` | `bool` | a sequence of note/duration pairs; `false` when malformed |
| `device.tone(hz, duration_ms)` | — | a square wave for that many milliseconds; `0` means until `device.silence()`; `hz` is clamped to `BuzzerMinHz` … `BuzzerMaxHz` |
| `device.silence()` | — | silence and stop any sequence |
| `device.take_gesture()` | a gesture string, or `nil` when the queue is empty | one whole gesture, from the panel's own queue |
| `device.log(text)` | — | one `[I]` diagnostic record, written when the device's `log` set includes `I` |
| `device.now_ms()` | `int` | `millis()` |
| `device.config(path)` | a `Config` instance | the document at `/cfg/` followed by `path`, read once and held; see below |

`device.unit()` answers the unit in force, so a page does not need `device.units()` to read it —
only to cycle it, or to label the units it cycles. `device.target()` is the stored setting, not the
machine's live target: a page
hands it to the machine when it becomes visible. A page that edits these values writes them through
`device` rather than through a document of its own.

```berry
var pressure_kpa = device.target()
if pressure_kpa > 0.0
  vacuum.set_target_pressure(vacuum.clamp(pressure_kpa))
end
```

### The unit record

`device.unit()` answers a `unit` record for the unit in force, and `device.units()` answers one for
every unit; read with a dot:

| Field | Type | Meaning |
| --- | --- | --- |
| `name` | string | `kpa`, `psi`, `inhg` or `mmhg` — the same token `device set unit` takes |
| `suffix` | string | what the band prints as the glass's unit, at the left of the bottom band |
| `per_kpa` | real | multiply a depth in kPa by this to get the panel's number |
| `whole_step` | real | one "whole" step in the panel's unit, for an editor |
| `decimals` | int | digits the unit's fine step can express |

The same numbers live in `panel/Units.h`, so a routine and the panel's own formatting cannot
disagree about what a psi is. `suffix` is read to name a unit being chosen, not to append to a
reading: the band prints the glass's unit itself.

### The document instance

`device.config(path)` answers a `Config` over `/cfg/` followed by `path`, read once and held, so a
page can keep a document and read or write it with the class's own methods rather than a call per
value. A routine's values live at `/cfg/routines/<name>.ini`, named after the routine, and a
data-image upload can ship initial ones beside its `.be`. The path is relative to `/cfg`: an empty
path, one beginning with `/`, or one holding a `..` segment is refused with `value_error`, and
anything that is not a string with `type_error`, so a page cannot name a file outside `/cfg`. A
document that is not there is not an error: the instance holds nothing, every `get` answers its
fallback, and the first `save` creates the file.

The instance's methods are the native `Config` class's:

| Call | Returns | Notes |
| --- | --- | --- |
| `config.get(section, key, fallback)` | the fallback's type | the value for a section and key, or the fallback when the document holds none; a flat document's keys are in the empty section |
| `config.set(section, key, value[, decimals])` | — | put the value into the document in memory; `decimals` is the places a real is written with, two by default. Nothing is written until `save()` |
| `config.save()` | `bool` | write the whole document as it stands, once: `true` when it is written or nothing had changed since the document was read, `false` when the library refused it. A document that is not there is created |
| `config.section_keys()` | list of strings | the section names holding at least one key, in document order |

```berry
var doc = device.config("routines/interval.ini")
var target = doc.get("", "target", 0.0)   # the flat file's key, in the empty section
doc.set("", "target", 12.5)               # changes the document in memory
doc.save()                                # writes /cfg/routines/interval.ini
```

The file is a flat `key=value` document, which is what makes it readable off the device and
editable in the data image:

```
target=12.5
count=3
```

A routine's document may also hold named `[section]` sets: the flat part is the root config, and
every named section is an alternate set the registry reads as an alias of the routine. An alias
section may hold a `routine_title` key, which is the title its browser row shows; absent, the
section name is. The root config never carries `routine_title`.

```
target=12.5
count=3

[gentle]
routine_title=Gentle
target=6.0
count=1
```

The class's own `Config():init()` still opens the device's own `/cfg/settings.ini`; calling
`init()` on an instance from `device.config` replaces its document with that one.

An instance writes its **whole document** on every `save()` that follows a change — the sections and
keys as they stand, in the order they were read — so a comment in the file does not survive a write
through an instance, and a write from another route after the instance was made is overwritten by
its next `save`. Hold an instance to read a document repeatedly.

There is **no limit** on how many keys a document holds: a `set` adds a new key whatever the count,
and a `save` answers `false` only when the document could not be written. The one cost is flash
wear: a `save` writes only when a value changed since the document was read — setting the same value
on every pass costs nothing, while storing a *changing* value on every pass wears the partition.
Write on the event, not on the frame. One `save` after any number of `set`s is one write.

### The piezo

`device.play_sequence` takes note names with an optional `#` or `b`, then one octave digit (`a4`,
`c#5`, `bb3`); `r` is a rest. Durations are milliseconds. The only valid notation is `note:duration`
pairs separated by spaces:

```berry
device.tone(2000, 500)
device.play_sequence("a4:1000 c6:500 a4:500")
device.silence()
```

The player is non-blocking: `tone` and `play_sequence` start a schedule and return, and the firmware
advances it in the loop. A note outside the buzzer's range makes `play_sequence` return false rather
than clamp. A non-positive `hz` means silence. A routine that starts a tone or sequence owns stopping
it before it leaves the page; a sequence stops itself at the end.

### The presses

`device.take_gesture()` answers the classifier's whole gesture, or `nil` when the queue is empty:
one or more keys, each with the form of its press, joined by `+` in panel order. A key is `up`,
`down`, `forward`, `back`, `ok` or `cancel`; its form is `single`, `double`, or whole seconds from
`2` up once the key passes `ExtendedHoldMs`. `ok:single`, `cancel:double` and `ok:5+cancel:5` are
all gestures.

A gesture is queued only while this page is showing. The queue holds eight; the oldest is dropped
if a routine falls behind, and that is logged once.

Draining the queue is the routine's job, usually at the top of `poll`:

```berry
def poll(now)
  while true
    var ev = device.take_gesture()
    if ev == nil
      break
    end
    # ev is one whole gesture, e.g. "ok:single"
  end
end
```

Holding **Back and OK together for 5 s** is the reboot: the same safe reset the wire serves as
`device reboot`, which stops, vents up to 2 s and restarts. It works from every page and link state,
and a routine cannot suppress it. The panel task acts on it before a page sees it.

The standard gestures a page should follow are in [gestures.md](../gestures.md).

The notation is strict: a comma, a missing colon, or a space between a note and its duration makes
the whole sequence malformed and `play` returns false.

## The jingle library

The device ships named note sequences in `/cfg/jingles.ini`, and `device.all_jingles()` answers
them as `jingle` records. The file is read once at boot, like the `.be` files, so a change takes
effect at the next reset. It is an INI document with one section per jingle, three keys each; which
one is selected is the device's own `[device] jingle` key in `/cfg/settings.ini`.

```ini
[poacher]
full_title=Lincolnshire Poacher
short_title=Poacher
notes=E5:200 G5:200 E6:200 ...
```

The section name is the jingle's **identifier**: lowercase, stable, and the value `[device] jingle`
stores.
`full_title` is the display name, `short_title` the label that fits on the panel, and `notes` the
sequence in `device.play_sequence()` notation.

A line beginning with `;` is a comment; anywhere else `#` is a note, so a sharp like `F#5` needs no
escaping. A section missing one of the three keys is reported on the diagnostics stream at boot and
left out of the library, and so is one whose sequence `device.play_sequence()` would refuse. A missing or
malformed file leaves the library empty.

Each entry in the list is a `jingle` record, read with a dot:

| Field | Type | Meaning |
| --- | --- | --- |
| `key` | string | the identifier `device.set_jingle()` writes |
| `full_title` | string | the display name |
| `short_title` | string | the short label the System page draws |
| `notes` | string | the note sequence, ready for `device.play_sequence()` |

The list is the whole of what a routine knows about the library, so one entry's name or sequence is
found by scanning it.

```berry
for jingle : device.all_jingles()
  device.log(jingle.key + "=" + jingle.full_title)
end

device.play_current_jingle()                          # the jingle chosen on the System page
device.play_sequence(device.all_jingles()[0].notes)            # the first one by name
```

An identifier the list does not hold is not in it, and `device.play_current_jingle()` falls back to
the first entry. When the library is empty that call answers `false`, the System page shows
`JINGLE=--`, and the rest of the device keeps working.

## `ux` — the scene stack (a Berry class)

The stack is not a native module. `/system/ux.be` defines `class Ux` and the global instance `ux`;
the firmware calls `reset`, `enter` and `poll` on it once a beat, and the pages call the rest.

| Call | Returns | Notes |
| --- | --- | --- |
| `ux.push(scene [, args [, callback]])` | `bool` | put a scene on top and make it the one that is polled. `args` (a map or nil) is that activation's `activate(args)`; `callback` receives the value the scene returns when it pops. Refused once eight scenes are on the stack |
| `ux.pop([value])` | `bool` | leave the top scene, handing `value` to the pusher's callback, or re-activating the scene below when it was pushed without one. Refused when only the root remains |
| `ux.pop_to_root([value])` | `bool` | pop every scene above the root; the root re-activates |
| `ux.top()` | the top scene, or `nil` | what the stack is showing |
| `ux.depth()` | `int` | how many scenes are on the stack |

A push or pop asked for from inside a scene's own `poll` is applied when that call returns. That is
about drawing rather than re-entry: a scene's `poll` ends by painting, so a pop applied mid-poll
would paint the page that just left over the one that came back. One request is applied per beat.
Only the top scene is activated and polled; a scene below the top keeps its state and resumes
through its callback when the scene above pops. A scene whose `activate()` or `poll()` raises is
dropped with the whole stack: the panel shows the device's mark.

The root scene is the `__rootScene` global `/system/home.be` sets, pushed at boot by the firmware. It
is not registered anywhere; it is what the device's go-home gesture returns to.

## `Registration` — the record a routine registers (a Berry class)

`/system/registration.be` defines `class Registration`, which a routine file builds and hands to
`routines.register`, and `class RoutineAlias`, which the registry makes for each alternate the
routine's document names. A routine that declares settings extends `Registration`, so the record is
its settings declaration too.

| Call | Returns | Notes |
| --- | --- | --- |
| `Registration(routine_key, title, icon, factory, settings_cfgs)` | an instance | the record: the key the routine is found by and whose document its values live in, the row label, an `Icon` instance or `nil`, the scene class the registry builds on every push, and the list of setting configs it edits, in the order the list shows them. A kind that cannot work is left out here, with one line saying why |
| `registration.problem(settings_values)` | string, or `nil` | the rule that binds the values to one another, or `nil` when every set is one a run can use; the base class answers `nil` |
| `registration.repair(section, settings_values)` | string, or `nil` | apply the rule to the map, keyed by setting key, putting refused settings back to their defaults in place, and answer the reason it applied or `nil` when the set was valid. Nothing is written |
| `registration.load_settings(section)` | map | the values the section holds: every setting the record declares, read from the routine's document, held to its kind and repaired by the rule, keyed by setting key. An alternate's section also carries its `routine_title` on the map |
| `registration.save_settings(section, settings_values)` | `bool` | repair the map and write the whole set to the routine's document in one `save()`; a set that changes nothing writes nothing, and a record that declares no settings answers `false`, writing nothing |
| `registration.aliases` | list of `RoutineAlias` | the alternates the routine's document named, in the order its sections were scanned; the registry fills it when the routine registers |

A section is `""` for the routine's own set, which lives in the document's global section, and the
alternate's key for one of its sets, which lives in that named section. A routine file calls none of
these itself: `routines.register` takes the record, and the registry's settings calls delegate here.

## `routines` — the registry (a Berry class)

The registry is not a native module. `/system/registry.be` defines `class RoutineRegistry` and the
global instance `routines`; a routine file's top level calls its methods and the routine browser
draws its list.

| Call | Returns | Notes |
| --- | --- | --- |
| `routines.register(registration)` | `bool` | bind one `Registration` to the routine it names. The key is its identity and the name of the document its values live in, so it has to be a plain word; a missing title shows the key; a record that is not a `Registration`, or one without a scene class, is refused with one log line. The scene is built from `factory` on every push. Registering also reads the routine's document: every named section becomes an alias, a second key that shares this routine's factory, settings declaration and icon but carries its own title and settings set |
| `routines.keys()` | list of strings | every registered key, in registration order, each alias following its routine |
| `routines.title(routine_key)` | string, or `nil` | the label the browser draws for the routine; an alias's is its `routine_title`, or the alias key when its section holds none |
| `routines.settings_cfgs(routine_key)` | list of setting configs, or `nil` | what the registration declares, in the order the list shows them, a kind that cannot work left out; `nil` when the key is not registered. An alias shares its parent's |
| `routines.icon(routine_key)` | the icon object, or `nil` | what the file passed as its icon; an alias shares its parent's |
| `routines.load_settings(routine_key)` | map | the values a routine begins with, as its registration's `load_settings` reads them: every setting it declares, read from its own file, held to its kind and repaired by the routine's rule, keyed by setting key. The root set reads the global section; an alias reads its own named section of the parent's document and also carries its `routine_title` on the map. A registration with no settings is an empty map |
| `routines.save_settings(routine_key, settings_values)` | `bool` | write a settled set back to the routine's document through its registration, which applies the routine's rule first so what lands is a set a run accepts, and writes an empty value for a setting the map holds nothing for. The document is opened once, every key put into it, and written once by `save()`; a set that changes nothing writes nothing. The root set writes the global section; an alias writes its own named section, leaving the root config alone. `false`, writing nothing, when the key is not registered or the routine declares no settings |
| `routines.push(routine_key, callback, activation)` | `bool` | build the routine's scene from its class, handing the activation to its `init()`, and push it, handing the returned value to the callback. The activation is a `RoutineActivation(routine_key, settings_values)` — the key the routine is launched under and the map its settings are read from, which is `routines.load_settings(routine_key)` for a launch from the routine's own file |

Registering the same key twice is one routine: the later file wins. The title is what the browser
shows, and a routine without one shows its key; an icon is optional too, and a routine without one,
or one whose icon is `nil`, is a text-only row. The device's own scenes — the home scene in
`/system/home.be`, the settings rows it opens in `/system/settings/device.be` and
`/system/settings/jingle.be`, the browser in `/system/browser.be` — are not registered at all; they
are well-known scenes the system pushes directly. There is no fixed number of routines: the registry
is a Berry list, so it grows as files register.
