# Register a routine from Berry

The routine registry is C++ now and nothing reaches it from a `.be` file: `routines` is undeclared
in the image, so `/system/order`'s `registry.be` is the only thing that ever defined it and the
image's own files name a global that no longer exists. This plan makes `routines` a native module
whose `register` takes a record built in Berry, so a routine file declares itself the way it does
today, and the registry answers the key, the title and the settings it was given.

It is the first of the models the panel's pages are written against, and deliberately the narrowest:
a record is registered and read back, while its page and its glyph stay C++-only. What is deferred
is named at the end.

## The rule is a check the set has to pass

A record's rule used to repair the set: settings went back to their defaults in the order the list
shows them and a reason was answered to nobody, at the far end of a load or a save — after the list
had closed, on a row the user had left. The repair is gone, and what is left is worse than nothing.
The `Rule` callback still sits in the record with nothing reading it, `parse` takes a document's
text as it comes, and the two relations the routines have — a total under its holds, a final
pressure under its start — are enforced by nothing at all.

Two things take its place, and neither is a callback that rewrites a set. A value is its row's to
hold: a number or a duration outside its declared bounds answers the bound, and a choice naming an
entry the row does not offer or a value of another type answers the default, so nothing a row cannot
hold reaches the set, the text a document gives included. The relations between settings are the
record's: one check over the whole set, answering the text that refuses it, or nothing when a run
can use it. `2_ramped_interval.be` answers `FINAL < START` for a `final_pressure` under its
`initial_pressure`, `5_ramped_static.be` answers `TOTAL < HOLDS` for a `total_duration` under the
two holds, and `3_interval.be` answers nothing. The set crosses to the check as a Berry map and the
text comes back as one copied string. The `Rule` alias, its constructor argument and its member go.

The check runs where a set is settled and changes nothing. A save it refuses writes nothing and a
launch it refuses pushes no page, so nothing runs on a set its routine would not accept; a set it
accepts is written once and launched as read. The text is what the list shows beside the row it
settled on, and the list's own surface arrives with the settings list. Nothing logs a repair.

The call is protected. A check that raises changes nothing, is answered to whoever made the call — a
Berry caller's `pcall`, or a refused launch — and is called again next time rather than struck off
for raising once. A record whose check is neither nothing nor a function is refused at registration,
with one line naming the routine, and the registry keeps what it held.

Checked: a document with a total of 30 under holds of 60 and 60 reads back 30 and saves false, and
the same document with a total covering the holds saves; a `final_pressure` of 3.0 under 5.0 answers
`FINAL < START`, and a launch on it is refused; `total_duration=99999` reads 7200,
`initial_pressure=400` reads 100, and a choice no row offers reads its default; the interval accepts
every set and launches on it; a check that is a number is refused with one line, and `keys()`
answers what it did before.

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
boot whose `init` takes the routine key, the title, the settings list and the check, which
`register` moves into the registry.

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
its row, with an alias's `routine_title` beside them. `save_settings(key, values)` takes such a map,
refuses it when the record's check does, and otherwise writes the declared keys once. A key the map
leaves out is written as nothing.

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
- The registry's host cases assert what the rows held and what a save answers, rather than a
  repaired set and a log line; the ` even at the defaults` case and the rule case in
  `settings.test.be` go.
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