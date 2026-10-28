# Register a routine from Berry

The routine registry is C++ now and nothing reaches it from a `.be` file: `routines` is undeclared
in the image, so `/system/order`'s `registry.be` is the only thing that ever defined it and the
image's own files name a global that no longer exists. This plan makes `routines` a native module
whose `register` takes a record built in Berry, so a routine file declares itself the way it does
today, and the registry answers the key, the title and the settings it was given.

It is the first of the models the panel's pages are written against, and deliberately the narrowest:
a record is registered and read back, while its page and its glyph stay C++-only. What is deferred
is named at the end.

## Each row carries the validator its editor asks

Each registration row carries one validator in two forms, because a row is declared before any set
exists and a scene holds the set it edits. Both answer whether the value is accepted and the text
that refuses it, and a refused value is never written: the editor stays open with that text beside
it, so a set is never settled on a row's refusal. `5_ramped_static.be` refuses a `total_duration`
under the two holds with `TOTAL < HOLDS`, `2_ramped_interval.be` refuses a `final_pressure` under
its `initial_pressure` with `FINAL < START`, and `3_interval.be` validates nothing. Nothing rewrites
a set; the old repair and the callback it drove are gone.

```cpp
// The whole check: the declaration, the set in force, and the value an editor is about to keep. It
// answers whether the value is accepted and, when it is not, the text the scene shows.
using Validator = std::function<std::pair<bool, String>(
    const std::vector<SettingsRow*>& settings, const SettingValues& values,
    const std::any& proposed)>;

// The same check closed over the settings and the set, which is the form a scene holds.
using CurriedValidator = std::function<std::pair<bool, String>(const std::any& proposed)>;
```

Checked: the `total_duration` row refuses 30 with `TOTAL < HOLDS` against a set whose holds are 60
and 60 and accepts 400, the `final_pressure` row refuses 3.0 under 5.0 with `FINAL < START`, the
interval's rows accept anything, a document holding `total_duration=99999` reads 7200, and an entry
no row offers reads its default.

## The registry is reachable from the interpreter

`routines` is installed by `Bindings.cpp` beside `vacuum`, `screen` and `device`, and answers
`register`, `keys`, `title`, `load_settings` and `save_settings` with the names, arguments and
answers the Berry global had. The registry is built once by the panel's owner, before the image
loads, with the library and logger it already had; every later call arrives through the module.

The module is only those five calls. `rows`, `icon`, `settings_cfgs`, `entry` and `push` arrive with
the consumer that needs them — the browser, the settings list and the page — because a call nothing
can make is a surface with no check behind it.

Checked: a Berry file calls each of the five and gets an answer, and a key that is not registered
answers an empty map and an empty title rather than raising.

## A record is built in Berry and registered

Today `Registration` in `registration.be` is a Berry class the routine files extend, and
`routines.register` takes an instance of it. The native record replaces it: a class installed at
boot whose `init` takes the routine key, the title and the settings list, which `register` moves
into the registry.

The refusals the registry already makes do not move. A key that is not a plain word — empty, or
carrying `/` or `\` — is refused with one line and answers false; the title falls back to the key; a
key already registered keeps its place while its record and alternates are replaced; and the
document scan that gives a record its alternates stays where it is, the alternates being the
registry's.

Checked: a record reads its key back from `keys()` and its title from `title()`, a key with a slash
answers false and leaves the registry as it was, and registering a key twice leaves one entry in the
place it first took.

## The setting kinds are there for Berry to declare with

The routine files declare their settings with `NumericEditCfg`, `PressureEditCfg`, `DurationEditCfg`
and `ChoiceEditCfg`, and none of the four exists: they went with the Berry framework, so no routine
file loads and no declaration can be made.

Each is a class installed at boot whose `init` takes what the routine files already pass — the key,
the default, the label, the formatter, and the bounds, entries or nullable flag its kind adds — and
builds a `NumberRow`, `PressureRow`, `DurationRow` or `ChoiceRow`. A record takes the list in the
order the list shows it, and each row holds its value to the rules its own declaration gave. A row
needs the gesture queue and stack to build its editor, so records are declared while the panel runs,
which is where the image and the check both are.

Checked: each class builds a row of its kind carrying the default it was given, and a record
declared with one of each answers its settings in the order they were listed.

## A set is read back as a map and written from one

`load_settings(key)` answers a Berry map of the key's settings, each in its kind's type and held to
its row, with an alias's `routine_title` beside them. `save_settings(key, values)` takes such a map
and writes the declared keys once. A key the map leaves out is written as nothing.

Checked: a set loads with the document's values and its defaults, one value changes and is handed
back, and a fresh load answers the new value.

## The image does not carry the Berry registry

`registry.be` and `registration.be` leave the image and `/system/order`. `records.be` stays, because
the native calls answer with the classes it declares, and the routines' documents stay where they
are.

Checked: `python tools/berry/check.py --memory` lists neither file, and the image loads with no
missing global.

## The suites and the driver that drive what is gone leave

The suites name `Scene`, `List`, `SettingsEditScene`, `ux`, `Icon`, the component model, the routine
pages and the device's chrome, and every one fails before it can run. They go with the driver's art,
scene and component entry points and the routines those draw; the suite runner and one new suite
stay, and the driver keeps what a plain check calls.

Checked: `python tools/berry/check.py --tests` exits zero — the image and harness load, and the
registry's host cases and the new suite pass.

## Consequences

- `harness.be`'s routine count works again and `--tests` becomes a check that can pass; the GIF
  capture behind `--art`, `--scene` and `--component` has no page to draw until a scene binding
  returns.
- The registry's host cases assert what a load and a save answer, rather than a repaired set and a
  log line; the ` even at the defaults` case and the rule case in `settings.test.be` go.
- The three routine files still do not load, their chrome, icon and page classes being page work;
  what this plan makes work is their registration.
- The routine spec in `docs/archive/specs-and-plans.zip` describes a registry that no longer exists,
  and nothing in the tree claims otherwise.

## Deferred

- The glyph: a Berry icon draws through `screen` at absolute coordinates, so it needs the canvas a
  component is given — the parking-lot entry on Berry drawing through a canvas.
- The page and `push`: a factory has to build the Berry class from the activation and wrap it in a
  scene, which waits on `ux` and `Scene`.
- `rows`, `icon`, `settings_cfgs` and `entry`, which arrive with the browser and the settings list.

## How it is known to hold

1. `python scripts/arduino.py --task build` compiles the module and the classes.
2. `python tools/berry/check.py --tests` exits zero.
3. `python tools/berry/check.py --memory` lists neither `registry.be` nor `registration.be`.