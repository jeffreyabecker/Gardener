# Bindings

The native surface a routine calls: four modules — `vacuum`, `screen`, `device` and `store` — and
the records those calls answer with. This is the reference for what a routine can call; for what a
routine must be, see [routine.md](./routine.md).
Two of the names a scene uses are Berry classes rather than native modules: the registry the routine
browser draws (`/system/registry.be`) and the scene stack it pushes onto (`/system/ux.be`). They are
documented here too, because a routine calls them the same way.

## Values

Berry's numbers here are 32-bit integers and single-precision reals.

Several of the calls answer a **record**: a small object whose fields are read with a dot. Two of
them answer a list of records.

```berry
var s = vacuum.state()
var depth = s.pressure_kpa          # not s["pressure_kpa"]
```

A record is the machine as it stood when the call returned, not a live view: writing a field
(`s.pressure_kpa = 1.0`) changes the routine's copy and nothing else. Reading a field the record does not
have raises `attribute_error`, and the fields are the ones listed under each call. The classes are
declared in `/system/records.be`, which the image loads before any page.

A routine's settings are not native records: each one is a setting config the routine builds from
`/system/settings/`, so it carries the key, the default and the kind's own rules rather
than being answered by a call. The classes and their arguments are listed under the settings a
routine registers in [routine.md](./routine.md).

## `vacuum` — the machine

| Call | Returns | Notes |
| --- | --- | --- |
| `vacuum.target(pressure_kpa)` | — | sets the depth to pull to, in kPa below ambient; `0` is the ambient target and releases the chamber. A value outside `0` … `floor` is ignored by the machine. A shallower target while pulling or holding vents to it; a deeper one pulls. Both return to the run |
| `vacuum.target_off()` | — | sets the ambient target, `0`; releases the chamber if it was pulling or holding |
| `vacuum.clamp(pressure_kpa)` | `real` | the same number held to the machine's bounds, `min` … `floor`. See below |
| `vacuum.start()` | `bool` | false when the machine refuses: no valid reading, or a latched fault. Valid at any target, `0` included |
| `vacuum.stop()` | — | stops the pump and vents. The target is kept |
| `vacuum.reset()` | — | clears a latched fault, then vents |
| `vacuum.state()` | a `reading` record | see below |
| `vacuum.now_ms()` | `int` | `millis()` |
| `vacuum.log(text)` | — | one `[I]` diagnostic record, written when the device's `log` set includes `I` |

`vacuum.state()` answers a `reading`, read with a dot:

| Field | Type | Meaning |
| --- | --- | --- |
| `pressure_kpa` | real | current depth below ambient |
| `target` | real | the target; **0 is the ambient target**, which holds the chamber released |
| `duty` | real | pump duty, percent |
| `floor` | real | deepest target the machine accepts |
| `min` | real | shallowest non-zero target the machine accepts |
| `state` | string | `IDLE`, `PULL`, `HOLD`, `VENT` or `FAULT`. A `VENT` from `stop()` ends at `IDLE`; a target vent returns to `PULL`/`HOLD` |
| `fault` | string | the latched code (`sensor`, `vent_timeout`, `no_progress`, `overshoot`), or `""` |
| `batt` | int | percent, or **-1 when no gauge is fitted** |
| `sensor_ok` | bool | a valid pressure reading is available |

Every pressure is kPa whatever the panel is printing. `screen.unit()` says what the glass shows.

`vacuum.clamp(pressure_kpa)` is the bounds rule in one place: below `min` it answers `min`, above `floor` it
answers `floor`, otherwise the number it was given. A target editor steps through it, a stored
target is restored through it, and a page that reads a pressure to start from uses it too — so no
routine carries its own copy of the two constants:

```berry
var s = vacuum.state()
var start = vacuum.clamp(s.pressure_kpa)         # a safe starting point for the editor
vacuum.target(vacuum.clamp(stored))     # and a stored one it cannot trust yet
```

`min` is the shallowest non-zero target: `vacuum.target(min)` is taken, anything between 0 and `min`
is not, and `vacuum.target(0)` is the ambient target — the chamber released — and is always taken.

## `screen` — the panel

Coordinates are panel pixels, origin top left. Nothing reaches the glass until `flush()`.

| Call |
| --- |
| `screen.draw_clear([color])` |
| `screen.draw_pixel(x, y [, color])` |
| `screen.draw_line(x0, y0, x1, y1 [, color])` |
| `screen.draw_hline(x, y, w [, color])` |
| `screen.draw_vline(x, y, h [, color])` |
| `screen.draw_rect(x, y, w, h [, color])` |
| `screen.draw_fillrect(x, y, w, h [, color])` |
| `screen.draw_invert(x, y, w, h)` |
| `screen.draw_roundrect(x, y, w, h, r [, color])` |
| `screen.draw_fillroundrect(x, y, w, h, r [, color])` |
| `screen.draw_circle(x, y, r [, color])` |
| `screen.draw_fillcircle(x, y, r [, color])` |
| `screen.draw_ellipse(x, y, rw, rh [, color])` |
| `screen.draw_fillellipse(x, y, rw, rh [, color])` |
| `screen.draw_triangle(x0, y0, x1, y1, x2, y2 [, color])` |
| `screen.draw_filltriangle(x0, y0, x1, y1, x2, y2 [, color])` |
| `screen.draw_rotatedrect(cx, cy, w, h, angle [, color])` |
| `screen.draw_fillrotatedrect(cx, cy, w, h, angle [, color])` |
| `screen.draw_text(x, y, size, inverted, text [, font])` |
| `screen.text_bounds(text, size [, font])` |
| `screen.width()` |
| `screen.height()` |
| `screen.page_width()` |
| `screen.page_height()` |
| `screen.flush()` |

`width()` and `height()` answer the panel's size in pixels. `page_width()` and `page_height()`
answer the area a page lays out within — the panel less the overlay's status band — so a layout
derives its boxes from the glass it owns rather than subtracting a band it does not.

### Colours

The optional last argument is the colour: `1` is ink and `0` is paper. With no argument the shape
and line calls draw ink, so a call written before colours existed draws what it always drew;
`draw_clear` with no argument clears the panel to paper, because a page's first call has to leave
paper behind it.

Paper draws exactly as ink does, one colour instead of the other: `draw_fillrect` with `0` clears a
box, `draw_line` with `0` rubs a line out, and `draw_circle` with `0` erases a disc. That is how a
page takes back part of what it drew without clearing the whole panel.

`1` and `0` are the whole vocabulary. A non-integer raises `type_error` and any other integer
raises `value_error` — `screen.<name>: colour 0 or 1 expected` — and the page is dropped like any
other mistake, so a third shade cannot quietly read as paper.

A saved frame draws ink black and paper white, which is the opposite of the glass, where a pixel
is lit for ink and dark for paper; in either it is the same pair of colours.

`draw_invert` takes no colour because it is the one call that does not put ink or paper down: it
flips what is already inside the rectangle, ink to paper and paper to ink, so it has to come after
the pixels it is meant to flip. That is also the only way to highlight a line drawn in a
proportional face, which `draw_text`'s `inverted` cannot do — see Faces below.

### Faces

`draw_text` takes an optional face name as its last argument, and `text_bounds` takes the same name
as its optional third. Without one, or with `nil` or `"builtin"`, both use the built-in 5x7. The
names are the compiled Adafruit GFX faces:

| Name | Face |
| --- | --- |
| `sans9`, `sans12`, `sans18`, `sans24` | `FreeSans` at 9, 12, 18 and 24pt |
| `sansbold9`, `sansbold12`, `sansbold18`, `sansbold24` | `FreeSansBold` at 9, 12, 18 and 24pt |
| `mono9`, `mono12`, `mono18`, `mono24` | `FreeMono` at 9, 12, 18 and 24pt |
| `monobold9`, `monobold12`, `monobold18`, `monobold24` | `FreeMonoBold` at 9, 12, 18 and 24pt |
| `tomthumb`, `picopixel` | the tiny 3x5 and 3x6 bitmap faces |

A name that is not in the table raises `value_error`, which drops the routine like any other
mistake.

With a custom face, `y` is the **baseline** of the line rather than the top of the glyph box — the
font format's own rule. With the built-in face, `y` is the top. `size` scales whichever face is set:
`1` is the face as designed, `3` is three times as tall. A custom face has no background, so
`inverted` has no effect on it; when a line needs a highlight, draw it and then `draw_invert` the
box `text_bounds` answered, which turns that rectangle's paper to ink and its ink to paper.

The face belongs to the call. The firmware restores the built-in face after drawing or measuring,
so handing the panel to the shared settings editor does not hand it a face as well.

```berry
screen.draw_text(0, 41, 1, false, value, "sansbold18")
var box = screen.text_bounds(value, 1, "sansbold18")
screen.draw_text(box.w + 4, 41, 1, false, "TGT", "sansbold9")
```

`text_bounds` answers a `bounds` record, read with a dot:

| Field | Type | Meaning |
| --- | --- | --- |
| `w` | int | the ink box width, in pixels |
| `h` | int | the ink box height, in pixels |

`screen.unit()` answers a `unit` record for the unit in force, and `device.units()` answers one for
every unit; read with a dot:

| Field | Type | Meaning |
| --- | --- | --- |
| `name` | string | `kpa`, `psi`, `inhg` or `mmhg` — the same token `device set unit` takes |
| `suffix` | string | what the overlay prints as the glass's unit, at the left of the bottom band |
| `per_kpa` | real | multiply a depth in kPa by this to get the panel's number |
| `whole_step` | real | one "whole" step in the panel's unit, for an editor |
| `decimals` | int | digits the unit's fine step can express |

The same numbers live in `panel/Units.h`, so a routine and the panel's own formatting cannot
disagree about what a psi is. `suffix` is read to name a unit being chosen, not to append to a
reading: the overlay prints the glass's unit itself.

### The overlay owns the bottom band

The bottom 12 rows of the glass, `y` 52–63, are the overlay's. It clears them and draws them last
on every `flush()`, so a routine never has to. Left to right:

| Field | `x` |
| --- | --- |
| the unit the glass prints in | 2–25 |
| the serial link's icon, while it is connected | 50–61 |
| the Bluetooth link's icon, while it is connected | 66–77 |
| the run state's icon | 100–111 |
| the battery, when a pack is fitted | 116–127 |

The run state's icon blinks while the pump pulls or the valve vents — lit for 65% of a 1000 ms
period — so a machine that is working reads differently from one that has stopped; holding, idle
and fault are steady.

The band is opaque: a page's own text and shapes belong above `y` 52, and anything drawn there is
painted over before the frame goes to the glass. The unit is one for the whole glass, so a page
draws the number and never the suffix after it: a readout that says `12.5` is a depth in the unit
the band names.

## `device` — the device around the machine

The unit the panel prints, the target the default program restores, the selected jingle, the piezo
and the presses a page has been given. The first three belong to the device rather than to a
routine's `store` file, and all are keys of `/cfg/settings.ini`'s `[device]` section: `unit`,
`target` and `jingle`. The unit is the same switch the serial `device` domain carries; the
diagnostics switch is not here, because it belongs to the serial link rather than to the glass. The
radio is not a setting: `device.bluetooth()` reports whether the image was built with it.

| Call | Returns | Notes |
| --- | --- | --- |
| `device.units()` | list of `unit` records | every unit `device set unit` accepts, in `panel/Units.h` order, each carrying the fields `screen.unit()` answers |
| `device.set_unit(token)` | `bool` | applies the unit and writes it to `/cfg/settings.ini` under `[device]`; `token` is a unit's `name`, and false for one that is not in `units()` |
| `device.target()` | `real` | the default program's target in kPa, or **0 when none is set** |
| `device.set_target(pressure_kpa)` | — | stores a new target in `/cfg/settings.ini` under `[device]`; clamp it with `vacuum.clamp()` first |
| `device.bluetooth()` | `bool` | whether this build has the radio; `true` unless the image was compiled without it |
| `device.jingle()` | string | the selected jingle identifier, or `""` when none has been chosen |
| `device.set_jingle(id)` | — | stores the identifier as `[device] jingle` in `/cfg/settings.ini` |
| `device.all_jingles()` | list of `jingle` records | every jingle this build can play, in file order |
| `device.play_current_jingle()` | `bool` | plays the jingle `[device] jingle` selects, falling back to the first entry; `false` when the library is empty |
| `device.play_sequence(notes)` | `bool` | a sequence of note/duration pairs; `false` when malformed |
| `device.tone(hz, duration_ms)` | — | a square wave for that many milliseconds; `0` means until `device.silence()`; `hz` is clamped to `BuzzerMinHz` … `BuzzerMaxHz` |
| `device.silence()` | — | silence and stop any sequence |
| `device.take_gesture()` | a gesture string, or `nil` when the queue is empty | one whole gesture, from the panel's own queue |

`screen.unit()` answers the unit in force, so a page does not need `device.units()` to read it —
only to cycle it, or to label the units it cycles. `device.target()` is the stored setting, not the
machine's live target: a page
hands it to the machine when it becomes visible. A page that edits these values writes them through
`device` rather than through its own `store`.

```berry
var pressure_kpa = device.target()
if pressure_kpa > 0.0
  vacuum.target(vacuum.clamp(pressure_kpa))
end
```

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
  vacuum.log(jingle.key + "=" + jingle.full_title)
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

The root scene is the `__rootScene` global `/modules/ui.be` sets, pushed at boot by the firmware. It
is not registered anywhere; it is what the device's go-home gesture returns to.

## `routines` — the registry (a Berry class)

The registry is not a native module. `/system/registry.be` defines `class RoutineRegistry` and the
global instance `routines`; a routine file's top level calls its methods and the routine browser
draws its list.

| Call | Returns | Notes |
| --- | --- | --- |
| `routines.register(routine_key, title, icon, factory, settings)` | `bool` | bind a routine key to the label the browser draws, the scene class that draws it and the class that declares its settings. The scene is built from `factory` on every push; `title` is the row label, or `nil` to show the key; `icon` is an `Icon` instance or `nil`; `settings` is a class answering static `settings()` and `problem(values)`, or `nil` when the routine has nothing to edit |
| `routines.keys()` | list of strings | every registered routine key, in registration order |
| `routines.title(routine_key)` | string, or `nil` | the label the browser draws for the routine |
| `routines.settings(routine_key)` | list of setting configs, or `nil` | what the routine declares, broken kinds left out |
| `routines.repair(routine_key, settings_cfgs, settings_values)` | string, or `nil` | apply the routine's rule to the values, keyed by setting key, putting refused settings back to their defaults in place, and answer the reason it applied or `nil` when the set was valid. It writes nothing |
| `routines.problem(routine_key, settings_cfgs, settings_values)` | string, or `nil` | the rule's verdict on the same settings and values, applying nothing |
| `routines.icon(routine_key)` | the icon object, or `nil` | what the file passed as its icon |
| `routines.load(routine_key)` | map | the values a routine begins with: every setting it registers, read from its own file, held to its kind and repaired by the routine's rule, keyed by setting key. A routine with no settings is an empty map |
| `routines.save_settings(routine_key, settings_values[, settings_cfgs])` | `bool` | write a settled set back to the routine's file, applying the routine's rule first so what lands is a set a run accepts, and writing an empty value for a setting the map holds nothing for. `settings_cfgs` may be left off, and then the settings the routine declares are looked up by its key. `false`, writing nothing, when the routine declares no settings |
| `routines.push(routine_key, callback[, settings])` | `bool` | build the routine's scene from its class, handing `settings` to its `init()`, and push it, handing the returned value to the callback. A `nil` settings map is `routines.load(routine_key)` |

Registering the same key twice is one routine: the later file wins. The title is what the browser
shows, and a routine without one shows its key; an icon is optional too, and a routine without one,
or one whose icon is `nil`, is a text-only row. The device's own scenes — the readout and the system
settings in `/modules/ui.be`, the browser in `/modules/browser.be` — are not registered at all; they
are well-known scenes the system pushes directly. There is no fixed number of routines: the registry is a Berry list, so
it grows as files register.

## `store` — the routine's own values

| Call | Returns | Notes |
| --- | --- | --- |
| `store.get(name, key, fallback)` | the fallback's type | a routine's value, or `fallback` when the key was never set |
| `store.get(name)` | `map` | every key in the document, as the text the file holds |
| `store.get(name, defaults)` | `map` | one entry per key in `defaults`, typed by its default, or the default itself when the key was never set |
| `store.set(name, key, value)` | `bool` | write a routine's value |
| `store.set(name, values)` | `bool` | write every entry of `values` in one write, keeping the keys it does not name |

A routine's values live in its own document at `/cfg/routines/<name>.cfg`, named after the routine, and a
data-image upload can ship initial ones beside its `.be`. A routine writes its own; a page that edits
another routine's values (the browser does) uses the same named forms. The name is a plain word — a
name with `/` or `\` is refused, because it would address another file.

The **fallback sets the type** the stored text is read back as:

```berry
var target = store.get("interval", "target", 0.0)   # real
var count  = store.get("interval", "count", 0)      # int
var muted  = store.get("interval", "muted", false)  # bool: "true" and "1" are true
var label  = store.get("interval", "label", "")     # anything else: the text as written
```

`store.set` writes a string as it stands, and a number or a bool in the interpreter's own text form.
A key that was never set returns the fallback rather than `nil`, so a first boot needs no special
case. A key that holds an empty value reads back as `""` — the way a cleared nullable setting is
kept, and the sentinel the settings view tests to tell "nothing" from a real value.

The **map forms** are the same rule for the whole document, read once and written once.
`store.get(name)` answers every key as the text the file holds; `store.get(name, defaults)` answers
one entry per key in `defaults`, each read back as that default's type and answered by the default
itself when the document holds nothing for it. `store.set(name, values)` merges the map into the
document: a key the map does not name is kept, and the file is written at most once however many
keys the map carries. A document that is not there reads as an empty map, so a first boot needs no
special case either, and a map that changes nothing writes nothing.

```berry
var all      = store.get("interval")   # every key, as text
var settings = store.get("interval", {target: 0.0, count: 0, muted: false})
store.set("interval", {target: 12.5, count: 3})   # one write; `muted` is kept
```

The file is a flat `key=value` document, which is what makes it readable off the device and
editable in the data image:

```
target=12.5
count=3
```

Two limits. It holds **32 keys** — a `set` past that returns `false`. And a `set` writes to flash,
though only when a value actually changed: setting the same value on every pass costs nothing,
while storing a *changing* value on every pass wears the partition. Write on the event, not on the
frame. A map `set` is one write for the whole map, so a settings page that commits a settled set
pays one, not one per key.
