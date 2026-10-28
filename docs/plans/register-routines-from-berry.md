# Register a routine from Berry

`RoutineRegistry`, `Registration`, the settings rows and the routine document store are C++ now, and
nothing reaches them from a `.be` file. `Bindings.cpp` installs `vacuum`, `screen`, `device` and
`config`; there is no `routines` module and no `settings` one. The image's files still call
`routines.register(...)`, `routines.rows()` and `routines.load_settings(...)`, and declare their
settings with `NumericEditCfg`, `PressureEditCfg`, `DurationEditCfg` and `ChoiceEditCfg`, so the
image does not load. This plan makes the registration surface native — every declaration a binding
object that holds the C++ thing it names, taken apart once by `register` — and leaves the page, the
browser and the settings list alone.

## The registry is reachable from the interpreter

`routines` is installed by `Bindings.cpp` beside `vacuum`, `screen` and `device`, and answers
`register`, `keys`, `title`, `load_settings` and `save_settings` with the names and the answers the
Berry global had. The registry is built once by the panel's owner, before the image loads, with the
library and logger it already has; every later call arrives through the module.

The module is those five calls. `rows`, `icon`, `settings_cfgs`, `entry` and `push` wait for the
browser, the settings list and the page, because a call nothing can make is a surface with no check
behind it. The setting kinds are classes in a module of their own, `settings`, installed beside
`routines` by the same `Bindings.cpp` call.

Checked: a Berry file calls each of the five and gets an answer, and a key that is not registered
answers an empty map and an empty title rather than raising.

## A registration is a binding object and `register` takes it

The routine files build a `Registration` subclass and call `routines.register(...)` with an instance
of it, so a registration is a class the image has to have, and no `Registration` class exists in the
image or as a native binding. A native class installed at boot beside `Config` replaces it:
`init(routine_key, title, glyph, settings)` builds a `gardener::Registration` from those arguments
and the instance holds it, so a file subclasses it, keeps the page's own value for the page work,
and passes the rest up. `register(record)` takes that registration out of the instance and hands it
to the registry, and answers false with one line, changing nothing, for a value that is not one of
these instances. The glyph is the packed bitmap the routine's file carries — the width, the height,
then the rows — the same value `screen.icon` draws, so it travels through the registration
unchanged.

The refusals the registry already makes do not move: a key that is not a plain word is refused with
one line and answers false, the title falls back to the key, a key already registered keeps its
place while its record and alternates are replaced, and the document scan that gives a record its
alternates stays with the registry. A list holding anything that is not one of the `settings`
module's objects is refused the same way — one line naming the routine, false, and the registry as
it was.

Checked: a record reads its key back from `keys()` and its title from `title()`, a key with a slash
answers false and leaves the registry as it was, registering a key twice leaves one entry in the
place it first took, and a list holding nil answers false and registers nothing.

## The setting kinds are binding objects in `settings`

The routine files declare their settings with `NumericEditCfg`, `PressureEditCfg`,
`DurationEditCfg` and `ChoiceEditCfg`, objects written to build a row where the declaration stands;
none of the four exists. `settings` holds one class per kind — `settings.number`,
`settings.pressure`, `settings.duration`, `settings.choice` — and a call on one constructs a binding
object: the class builds the row the kind describes and the instance holds it. The arguments are the
ones the files already pass, and an argument of the wrong type, or one the kind needs and the call
left out, raises at the call with the module and the kind in the line, because the file that wrote
it is the file to fix.

The row is built where it can be. A row needs the gesture queue and the scene stack to make its
editor, so the declaration is made while the panel runs, which is where the image and the check both
are, and `register` takes the row out of each object and puts it on the record in the position the
list gave it. A registration's settings are the order of the objects it was handed, and nothing is
read back off a field.

A number takes the key, the default, the label, the formatter, the step, the bounds, the nullable
flag and whether it is whole; a whole one is a whole number wherever the setting's type shows,
loading and saving as an int at no decimals where a real one is a float. A pressure takes the same
without the whole flag, prints in the unit the panel prints in, its step being that many whole
display steps and the machine's clamp its bound, and a nullable declaration's nothing loads as nil.
A duration takes whole seconds over its step and its bounds and prints compact, `1m30s`. A choice
takes one entry per declared string, key and title alike, and a nullable declaration's nothing loads
as nil and prints `--`; a declaration with no entries answers a row with nothing to offer rather
than raising at boot.

Checked: a call on each class answers an object carrying a row of that kind and the default it was
given, and a registration declared with one of each answers four rows of those kinds, in the order
they were listed. A call passing a bound that is not a number raises and leaves the registry as it
was.

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

## Deferred

- The page and `push`: a factory has to build the Berry class from the activation and wrap it in a
  scene, which waits on `ux` and `Scene`.
- `rows`, `icon`, `settings_cfgs` and `entry`, which arrive with the browser and the settings list.
- The per-set rule a file declares beside its settings (`problem`): nothing reads it, and a refusal
  belongs to the row that settles a value, which arrives with the settings list.
- The image's files keep their chrome and page classes, so the image stays unloadable until that
  page work lands.

## How it is known to hold

1. `python scripts/arduino.py --task build` compiles the two modules and the rows a declaration
   builds.
2. The image's `[E]` lines on the device name no missing `routines` call once the page classes
   follow.
3. `python tools/berry/check.py --tests` and `--memory` hold once the interpreter's check runs
   again; it is broken and is not run today, so only the build is checked now.
