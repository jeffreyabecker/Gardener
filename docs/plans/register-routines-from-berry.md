# Register a routine from Berry

`RoutineRegistry`, `Registration`, the settings rows and the routine document store are C++ now, and
nothing reaches them from a `.be` file. `Bindings.cpp` installs `vacuum`, `screen`, `device` and
`config`; there is no `routines` module. The image's files still call `routines.register(...)`,
`routines.rows()` and `routines.load_settings(...)`, and build `Registration` and `*EditCfg` classes
that are not there, so the image does not load. This plan makes the registration surface native and
leaves the page, the browser and the settings list alone.

## The registry is reachable from the interpreter

`routines` is installed by `Bindings.cpp` beside `vacuum`, `screen` and `device`, and answers
`register`, `keys`, `title`, `load_settings` and `save_settings` with the names, arguments and
answers the Berry global had. The registry is built once by the panel's owner, before the image
loads, with the library and logger it already has; every later call arrives through the module.

The module is only those five calls. `rows`, `icon`, `settings_cfgs`, `entry` and `push` wait for
the browser, the settings list and the page, because a call nothing can make is a surface with no
check behind it.

Checked: a Berry file calls each of the five and gets an answer, and a key that is not registered
answers an empty map and an empty title rather than raising.

## A record is built in Berry and registered

The routine files build a `Registration` subclass and call `routines.register(...)` with it; no
`Registration` class exists in the image or as a native binding. A class installed at boot whose
`init` takes the routine key, the title, the glyph and the settings list replaces it, and `register`
moves the record into the registry. The glyph is the packed bitmap the routine's file carries — the
width, the height, then the rows — the same value `screen.icon` draws, so it travels through the
registration unchanged.

The refusals the registry already makes do not move: a key that is not a plain word is refused with
one line and answers false, the title falls back to the key, a key already registered keeps its
place while its record and alternates are replaced, and the document scan that gives a record its
alternates stays with the registry.

Checked: a record reads its key back from `keys()` and its title from `title()`, a key with a slash
answers false and leaves the registry as it was, and registering a key twice leaves one entry in the
place it first took.

## The setting kinds are there for Berry to declare with

The routine files declare their settings with `NumericEditCfg`, `PressureEditCfg`,
`DurationEditCfg` and `ChoiceEditCfg`, and none of the four exists. Each is a class installed at
boot whose `init` takes what the routine files already pass — the key, the default, the label, the
formatter, and the bounds, entries or nullable flag its kind adds — and builds a `NumberRow`,
`PressureRow`, `DurationRow` or `ChoiceRow`. A record takes the list in the order the list shows it,
and each row holds its value to the rules its own declaration gave.

A row needs the gesture queue and stack to build its editor, so records are declared while the panel
runs, which is where the image and the check both are.

Checked: each class builds a row of its kind carrying the default it was given, and a record
declared with one of each answers its settings in the order they were listed.

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
- The image's files keep their chrome and page classes, so the image stays unloadable until that
  page work lands.

## How it is known to hold

1. `python scripts/arduino.py --task build` compiles the module and the classes.
2. The image's `[E]` lines on the device name no missing `routines` global once the page classes
   follow.
3. `python tools/berry/check.py --tests` and `--memory` hold once the interpreter's check runs
   again; it is broken and is not run today, so only the build is checked now.
