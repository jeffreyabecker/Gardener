# Register a routine from Berry

The routine registry is C++ now and nothing reaches it from a `.be` file: `routines` is undeclared in
the image, so `/system/order`'s `registry.be` is the only thing that ever defined it and the image's
own files name a global that no longer exists. This plan makes `routines` a native module whose
`register` takes a record built in Berry, so a routine file declares itself the way it does today,
and the registry answers the key, the title and the settings it was given.

It is the first of the models the panel's pages are written against, and it is deliberately the
narrowest: a record is registered and read back, and its page and its glyph are still C++-only
values. What is deferred is named at the end.

## The rule is a declaration, not a call

Today a record's rule is a `std::function` over the settings map, which is what replaced the Berry
`problem` override: `repair` runs it, and registration keeps it. A Berry file cannot supply one
without the registry calling back into the interpreter on every load, every save and every repair,
and a reason answered that way has to survive in memory the caller owns.

A record declares the rule instead: the setting that must be at least the sum of the settings it
names, and the reason it refuses a set that is not. The two rules in the tree are one such
comparison each — `final_pressure` covering `initial_pressure`, and `total_duration` covering
`initial_duration` plus `target_duration` — and a record that declares no rule accepts every set.
The comparison is between numbers whatever the setting holds, a count of whole seconds or a pressure
in kPa, so a member of `std::any` is read by its type rather than by a cast the rule names.

Checked: a set whose `total_duration` is 30 against holds of 60 and 60 answers `TOTAL < HOLDS` and
reads back repaired to 300; the same with `final_pressure` below `initial_pressure` answers
`FINAL < START`; a record with no rule answers nil for any set.

## The registry is reachable from the interpreter

`routines` is installed by `Bindings.cpp` beside `vacuum`, `screen` and `device`, and answers
`register`, `keys`, `title`, `load_settings` and `save_settings` with the names, arguments and
answers the Berry global had. The registry itself is built once by the panel's owner, before the
image loads, with the library and logger it was already given; every later call arrives through the
module.

The module is only those five calls. `rows`, `icon`, `settings_cfgs`, `entry` and `push` arrive with
the consumer that needs them — the browser, the settings list and the page — because a call nothing
can make is a surface with no check behind it.

Checked: a Berry file calls each of the five and gets an answer, and a call naming a key that is not
registered answers an empty map and an empty title rather than raising.

## A record is built in Berry and registered

Today `Registration` in `registration.be` is a Berry class the routine files extend, and
`routines.register` takes an instance of it. The native record replaces it: a class installed at
boot whose `init` takes the routine key, the title and the declaration, and `routines.register`
moves it into the registry.

The registry's own refusals do not move: a key that is not a plain word — empty, or carrying `/` or
`\` — is refused with one line and answers false, the title falls back to the key, and a key already
registered keeps its place while its record and its alternates are replaced. The document scan that
gives a record its alternates stays where it is, because the alternates are the registry's.

Checked: a suite registers a record and reads its key back from `keys()` with its title from
`title()`; a key with a slash answers false and leaves the registry as it was; registering the same
key twice leaves one entry, in the place it first took.

## The setting kinds are there for Berry to declare with

Today the routine files declare their settings with `NumericEditCfg`, `PressureEditCfg`,
`DurationEditCfg` and `ChoiceEditCfg`, and none of the four exists: they went with the Berry
framework, so no routine file can be loaded, and no declaration can be made.

Each of the four is a class installed at boot whose `init` takes the arguments the routine files
already pass — the key, the default, the label, the formatter, and the bounds, entries or nullable
flag its kind adds — and builds the row of its kind: a `NumberRow`, a `PressureRow`, a `DurationRow`
or a `ChoiceRow`. A record takes the list of them as its declaration, in the order the list shows it.
A row needs the panel's gesture queue and stack to build its editor, so a record is declared while
the panel is running, which is where the image and the check both are.

Checked: each kind's class builds a row of its kind carrying the default the declaration gave it,
and a record declared with one of each answers its settings in the order they were listed.

## A set is read back as a map and written from one

`load_settings(key)` answers a Berry map of the key's declared settings, each in its kind's type,
with an alias's `routine_title` beside them and the set repaired by the record's rule first.
`save_settings(key, values)` takes such a map, repairs it, and writes the declared keys once. A key
the map leaves out is written as nothing, as it is today.

Checked: a suite registers a record over a document, loads a set that answers the document's values
and its defaults, changes one value and hands it back, and reads the new value from a fresh load.

## The image does not carry the Berry registry

`registry.be` and `registration.be` leave the image and `/system/order`. `records.be` stays, because
the native calls still answer with the classes it declares, and the routines' documents stay where
they are.

Checked: `python tools/berry/check.py --memory` lists neither file, and the image loads with no
missing global.

## The suites and the driver that drive what is gone leave

The suites name `Scene`, `List`, `SettingsEditScene`, `ux`, `Icon`, the component model, the routine
pages and the device's chrome, and every one of them fails to compile before it can run. They go,
together with the driver's art, scene and component entry points and the routines those draw; the
suite runner and one new suite stay, and the driver keeps only what a plain check calls.

Checked: `python tools/berry/check.py --tests` exits zero — the image loads, the harness loads, the
registry's host cases pass and the new suite passes.

## Consequences

- `harness.be`'s routine count works again, and `--tests` becomes a check that can pass; the GIF
  capture behind `--art`, `--scene` and `--component` has no page to draw until a scene binding
  returns.
- The registry's host cases move to the declared rule: the fixture that answered a `std::function`
  answers a comparison, and the case for a rule that no set satisfies goes with the callback.
- The three routine files still do not load, because their chrome, icon and page classes are for the
  page work; what this plan makes work is their registration.
- The routine spec in `docs/archive/specs-and-plans.zip` describes a registry that no longer exists;
  nothing in the tree claims otherwise.

## Deferred

- The glyph. A Berry icon draws through `screen` at absolute coordinates, so it needs the canvas a
  component is given before a paint can be made of it — the parking-lot entry on Berry drawing
  through a canvas.
- The page and `push`. A factory has to construct the Berry class with the activation and wrap it in
  a scene, which waits on `ux` and `Scene` being bound again.
- `rows`, `icon`, `settings_cfgs` and `entry`: they arrive with the browser and the settings list.

## How it is known to hold

1. `python scripts/arduino.py --task build` compiles the module and the classes.
2. `python tools/berry/check.py --tests` exits zero: the image and the harness load, and the
   registry's host cases and the new suite pass.
3. `python tools/berry/check.py --memory` lists neither `registry.be` nor `registration.be`.
