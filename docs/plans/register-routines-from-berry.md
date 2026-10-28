# Register a routine from Berry

`routines` already registers a routine, reads its set back as a map and writes one back: a file
declares itself with one flat call, names the page its launch builds, and the record it made holds
the settings rows the file declared. Nothing around that is reachable. `/system/browser.be` names
`List`, `ux` and `SettingsEditScene`, none of which exists, so the file does not load and no browser
appears; home carries no gesture that would open one; and `routines.title` and
`routines.settings_cfgs`, the two names a page reads for its heading and its settings, are not
answered. This plan carries what is left between a registered routine and one a finger can launch:
the records reach Berry, a key launches its page, and that page's settings list opens, edits, and
hands its set back. The device's own settings list is not this plan's.

## `rows` and `title` answer the registry's records

`/system/browser.be` builds its list from `routines.rows()` and heads a page from
`routines.title(routine_key)`; the module answers neither name, so neither call can be made and a
page has no heading to draw.

`rows()` answers one value per record and per alternate, in the order they registered, each carrying
the glyph string its file gave or nothing when it gave none, its title, and its key — the value a
launch is made from. `title(routine_key)` answers the same title, for a key or an alternate, and nil
for a key nobody registered. A title a file left out is the key, the fallback `register` already
makes, and neither call disturbs a record's set.

Checked: with the three routine files loaded, `rows()` answers three values carrying their glyphs in
`/routines` order, and `title` answers for a key, for an alternate, and nil for an unknown key.

## A key launches its page

`RoutineRegistry::push` builds a page through the record's factory and hands it to the stack, but
nothing calls it: the module answers no `push`, so no registration can be launched and `push` is dead
code.

`routines.push(routine_key, callback, activation)` puts the record's page on the stack and answers
whether a page is showing. The page is an instance of the class its file registered, built with the
activation as its one argument — the key, and the record's clean set, which is the map
`load_settings` answers. The browser passes nil for the callback. A key nobody registered
answers false with the stack as it was, and so does a page whose `init` raises, which logs one `[E]`
line naming the routine and the message.

Checked: OK on a row shows that routine's page, built from the set the record holds; Back from the
page returns to the rows; a page whose `init` raises logs one line and leaves the rows showing.

## A record's declarations are readable for the list

`register` consumes the declaration list it is handed, one row per declaration, and the record keeps
the rows; both pages and the browser then ask `routines.settings_cfgs(routine_key)` for that same
set, so a list can be built from it, and the module answers no such name. What a record holds are
rows the store reads, not the declarations a file wrote, so the declarations are not readable again
as they were given.

`settings_cfgs(routine_key)` answers the record's declarations in the order its file gave them, with
the key, label, kind, bounds, step and entries each was declared with, for a key or an alternate; nil
for a key nobody registered and for a record that declared none. Reading them leaves the record's own
rows as they were, so a list opened twice edits the same declarations twice.

Checked: the browser's Forward shows a list whose rows are the routine's declarations in declaration
order with their labels, and OK there launches the routine with the set it settled on.

## The stack is moved from Berry

`/system/browser.be` pushes its settings list with `ux.push(...)`, closes itself with `ux.pop(nil)`,
and a routine page leaves through the same pop. `Ux` is the stack in C++ and no file can reach it, so
no page can put another on top or take itself off, and the browser's file does not compile.

`ux.push(page, callback)` shows a page over the one showing and runs the callback when it pops, handed
what the page answered; `ux.pop(value)` closes the top page. Both answer whether a page is showing. A
pop at the root answers false and leaves it, and the stack's cap of eight frames stays.

Checked: Forward from the browser shows the settings list over it, OK closes the list, and the
browser is showing again having read the set it handed back.

## A settings list is a page Berry builds

`SettingsEditScene(settings_cfgs, settings_values, title)` is the list the browser's Forward and both
pages' edit open: the heading, one row per declaration with its label and the value in force, Up and
Down moving the selection, Forward opening the row's editor, OK keeping the set, and Back or Cancel
putting it back as it was opened. `SettingsScene` draws and drives exactly that, and is built only by
whoever holds the stack and the gesture queue; nothing makes one from a Berry declaration list and a
map.

A list built that way shows the declarations it was handed under the title it was handed — no heading
for nil — starts each setting from the map in force, or from the declaration's default where the map
holds nothing, and hands the set it settled on back to the callback it was pushed with, in the shape
`save_settings` takes.

Checked: Forward on a row edits that value, OK closes the list, and the routine's document holds the
new value after the launch that follows; Back leaves the document as it was.

## The browser's rows move through a window

`/system/browser.be` builds `List(6)` and calls `set`, `move` and `selected_row` on it, reading
`rows`, `selected`, `first()` and `visible`; no such value exists in the image or in C++, so the
browser's file cannot compile. The chrome draws a row eight pixels high under a scrollbar showing how
far through the list the window is.

The browser shows six rows at a time. Up and Down move the selection by one and carry the window with
it, the selected row is drawn inverted, and the scrollbar marks the window's place in the list. Three
registrations fill less than the window, so it never moves and the scrollbar is full.

Checked: with three routines the rows show with their glyphs and titles and the scrollbar is full;
with more than six registered, Down past the sixth moves the window on by one.

## Home opens the browser

`/system/home.be` answers OK and Cancel and nothing else: the doubles the old page carried went with
its gestures, and its comment says both pages wait for their ports. `/system/order` says home names
`browserScene`, which it does not, so the browser is a scene nothing reaches.

Home's Up double press shows the browser, and Back or Cancel in the browser returns to home's own
page. Nothing in the browser starts a run: its `activate` stops the chamber, and only a launch begins
one.

Checked: Up twice on home shows the rows; Back returns to home's readout with the device's target
still set; a run under way is stopped before the rows are drawn.

## Consequences

`/system/order`'s note that home names `browserScene` and `DeviceSettings` states what is not true
until this lands, and a file compiled against a name nothing answers does not load: the browser is
pushed from home, and the device's settings list is not part of this plan.

The parking lot's entry for the browser's page surface is gone, because this plan is that work now.

## Deferred

- `keys`, `entry` and `icon`: no file calls any of the three.
- The device's own settings list: `DeviceSettings` in `/system/order`'s comment and the rows
  `cfg/settings.ini` holds are their own plan.

## How it is known to hold

1. `python scripts/arduino.py --task build` compiles the names the module gains and the settings list
   built from a Berry declaration list.
2. `python tools/unit/check.py` holds the scene-stack and settings-list cases on the host: a push and
   a pop, a list's rows in declaration order, a default where the map holds nothing, and the set a
   list hands back.
3. `python tools/berry/check.py` is broken and is not run. On the device the check is `--task upload`
   and the `[E]` lines: the browser and home load, the three rows draw, a row launches, and no line
   names a call nothing answers.