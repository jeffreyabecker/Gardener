# Register a routine from Berry

`RoutineRegistry`, `Registration`, the settings rows and the routine document store are C++ now, the
`settings` module and the native `Registration` class are installed, and there is no `routines`
module. The image calls eight names on one — `register`, `keys`, `title`, `rows`, `settings_cfgs`,
`load_settings`, `save_settings` and `push` — and declares its settings under names the module does
not answer, so the image does not load. This plan makes the registry reachable from Berry: the
module, `register` taking the record an instance holds, the rows the browser lists, and a set read
back as a map. It leaves the browser and the settings list alone: `settings_cfgs` and `push` wait
for them, and so do the `List`, `ux` and `SettingsEditScene` globals the browser is written against
and nothing declares.

## The registry is reachable from the interpreter

`routines` is installed by `Bindings.cpp` beside `vacuum`, `screen`, `device` and `settings`, and
answers `register`, `keys`, `title`, `rows`, `load_settings` and `save_settings` with the names and
the answers the Berry global had. The registry is built once, before the image loads, with the
library and logger it already has; every later call arrives through the module. Nothing in the tree
constructs a `RoutineRegistry` today, so the panel's owner gains one and the install is handed it —
the interpreter's page names four native modules and is about to name five.

The module is those six calls. `settings_cfgs`, `push`, `entry` and `icon` wait for the browser, the
settings list and the page, because a call nothing can make is a surface with no check behind it.

Checked: a Berry file calls each of the six and gets an answer; `keys()` answers every registered key
in registration order, each alternate after its record; and a key that is not registered answers an
empty map from `load_settings`, false from `save_settings` and an empty title from `title`, raising
nothing.

## `register` takes the record out of the instance

The routine files hand `routines.register(...)` an instance of the native `Registration` class that
is installed now, and nothing reads the record it holds. The declaration is four names —
`Registration(routine_key, title, glyph, settings)`: the key, the label, the glyph — the hex digits
that name its packed bitmap — and the list of `settings` declarations, in that order. The files hand
the init five, passing the class they registered as the page fourth, so the init reads that class as
the glyph and the declaration list as a fifth argument it has no name for. A page is not one of the
four, so the three files stop passing it until the page work lands.

`register(record)` takes the `gardener::Registration` out of the instance's `.p` and hands it to the
registry, answering what the registry answers, and false with one line naming the routine for a
value that is not one of these instances or one whose record has already been taken: the registry
owns a record from the first call. The glyph, the title and the settings rows travel with it, so
nothing is rebuilt here.

The refusals the registry already makes do not move: a key that is not a plain word is refused with
one line and answers false, the title falls back to the key, a key already registered keeps its
place while its record and alternates are replaced, and the document scan that gives a record its
alternates stays with the registry.

The three routine files change with it: each declares its settings with the module's kinds,
subclasses the record, drops the page class it was handing the init, and registers the instance, so
the declaration lines the image carries are the ones the surface answers.

Checked: a record reads its key back from `keys()` and its title from `title()`, a key with a slash
answers false and leaves the registry as it was, registering a key twice leaves one entry in the
place it first took, and registering one instance twice answers false the second time.

## A row is the glyph, the title and the key

`Registration` keeps the glyph as the string the file wrote — the hex digits that name the packed
bitmap — and nothing on this side decodes it: a row is drawn by `screen.icon`, which takes that
string and reads its bits when it paints the icon, so the record hands the browser a glyph it can
draw unchanged. The row the registry answers carries the same string, and `/system/records.be`
declares no row at all — a record reads with dots only on a class the parser declared, so there is a
field list missing as well.

`rows()` answers a list of `row` instances: one per record and one per alternate, in `keys()` order,
each carrying `icon` — the hex digits exactly as the file declared them, or nil when it named no
glyph — `title`, the label the browser draws, and `value`, the key the row launches. `records.be`
declares `row` from those three fields before the module answers one, so a row's `icon` is the value
`screen.icon` accepts unchanged.

Checked: a row for a record declared with a glyph reads back the digits the file declared, `rows()`
for two records and one alternate answers three rows in that order, and a record that named no glyph
answers a nil `icon`.

## A set is read back as a map and written from one

`load_settings(key)` answers a Berry map of the key's settings, each in its kind's type and held to
its row, with an alias's `routine_title` beside them. `save_settings(key, values)` takes such a map
and writes the declared keys once. A key the map leaves out is written as nothing.

Checked: a set loads with the document's values and its defaults, one value changes and is handed
back, and a fresh load answers the new value.

## The suites that drive what is gone leave

`tools/berry/tests/` still names `Scene`, `List`, `SettingsEditScene`, `ux`, `Icon`, the component
model, the routine pages and the device's chrome, and every one fails before it can run. They go
with the driver's art, scene and component entry points and the routines those draw; the suite
runner and one new suite stay, and the driver keeps what a plain check calls.

Checked: the files that name the gone classes are gone, and the runner and the new suite are the
only suite sources left.

## Consequences

The interpreter's page names four native modules and one class; it names five modules and the same
class. `/system/records.be` gains a record, and a record's field list is the contract with the call
that fills it, so the browser and whatever answers a row read the same three names. The browser, the
settings list and the routine pages are untouched by this plan: their `[E]` lines stay, and none of
them is a `routines` call this module refuses.

## Deferred

- The page and `push`: a record names no page yet, so nothing can be launched. Every call site in
  the image hands push three values — the key, a nil and an activation — while the registry's own
  push takes the stack and a callback and reads the settings again itself, so which of the two a
  Berry caller gets, and what the activation is for, is part of the page work. A factory has to build
  the Berry class from the activation and wrap it in a scene, which waits on `ux` and `Scene` and on
  a record gaining somewhere to keep the class it names.
- `settings_cfgs` and `entry`: `settings_cfgs` answers the settings declaration by pointer in C++,
  and the browser sizes that answer and hands it to the settings list, so what a cfg is on the Berry
  side — the list of declarations or a handle — is decided with the list that edits them. `entry`
  has no caller in the image.
- `icon`: nothing in the image calls it, and a row's glyph is the string above rather than a paint.
- The `List`, `ux` and `SettingsEditScene` globals browser.be names: `Ux` has no Berry surface,
  `List` does not exist, and the settings list is `SettingsScene` under another name. Berry resolves
  globals when it compiles, so no file that opens the browser or a routine's settings loads until
  all three exist.
- The per-set rule a file declares beside its settings (`problem`): nothing reads it, and a refusal
  belongs to the row that settles a value, which arrives with the settings list.

The browser's page surface — `List`, `ux`, `SettingsEditScene`, `settings_cfgs` and `push` — is
parked in [the parking lot](../parking-lot.md#the-browsers-page-surface-from-berry).

## How it is known to hold

1. `python scripts/arduino.py --task build` compiles the `routines` module, the `row` record it
   fills and what its register reaches.
2. `python tools/berry/check.py --tests` holds the new suite's cases — the rows, `register`'s two
   refusals and the map round trip — once the interpreter's check runs again. It is broken and is
   not run today, so the build is the whole of what is checked now.
3. Nothing on the device reaches the module yet: every `/routines` file and `/system/browser.be`
   still names `ux`, `SettingsEditScene` or `List`, so none of them loads. The `[E]` lines that stay
   are those names, and each goes with the page work rather than with this plan.

