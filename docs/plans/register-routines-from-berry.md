# Register a routine from Berry

`RoutineRegistry`, `Registration`, the settings rows and the routine document store are C++ now, the
`settings` module and the native `Registration` class are installed, and there is no `routines`
module. The image's files still call `routines.register(...)`, `routines.rows()` and
`routines.load_settings(...)` and declare their settings under names the module does not answer, so
the image does not load. This plan makes the registry reachable from Berry — a `routines` module
whose `register` takes the record an instance holds — and leaves the page, the browser and the
settings list alone.

## The registry is reachable from the interpreter

`routines` is installed by `Bindings.cpp` beside `vacuum`, `screen`, `device` and `settings`, and
answers `register`, `keys`, `title`, `load_settings` and `save_settings` with the names and the
answers the Berry global had. The registry is built once by the panel's owner, before the image
loads, with the library and logger it already has; every later call arrives through the module.

The module is those five calls. `rows`, `icon`, `settings_cfgs`, `entry` and `push` wait for the
browser, the settings list and the page, because a call nothing can make is a surface with no check
behind it.

Checked: a Berry file calls each of the five and gets an answer, and a key that is not registered
answers an empty map and an empty title rather than raising.

## `register` takes the record out of the instance

The routine files hand `routines.register(...)` an instance of the native `Registration` class that
is installed now, and nothing reads the record it holds. `register(record)` takes the
`gardener::Registration` out of the instance's `.p` and hands it to the registry, answering what the
registry answers, and false with one line naming the routine for a value that is not one of these
instances or one whose record has already been taken: the registry owns a record from the first
call. The glyph, the title and the settings rows travel with it, so nothing is rebuilt here.

The refusals the registry already makes do not move: a key that is not a plain word is refused with
one line and answers false, the title falls back to the key, a key already registered keeps its
place while its record and alternates are replaced, and the document scan that gives a record its
alternates stays with the registry.

The three routine files change with it: each declares its settings with the module's kinds,
subclasses the record, and registers the instance, so the declaration lines the image carries are
the ones the surface answers.

Checked: a record reads its key back from `keys()` and its title from `title()`, a key with a slash
answers false and leaves the registry as it was, registering a key twice leaves one entry in the
place it first took, and registering one instance twice answers false the second time.

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

1. `python scripts/arduino.py --task build` compiles the `routines` module and what its register
   reaches.
2. The image's `[E]` lines on the device name no missing `routines` call once the page classes
   follow.
3. `python tools/berry/check.py --tests` and `--memory` hold once the interpreter's check runs
   again; it is broken and is not run today, so only the build is checked now.
