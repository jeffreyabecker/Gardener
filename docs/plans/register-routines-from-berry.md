# Register a routine from Berry

The routine registry is C++ now and nothing reaches it from a `.be` file: `routines` is undeclared in
the image, so `/system/order`'s `registry.be` is the only thing that ever defined it and the image's
own files name a global that no longer exists. This plan makes `routines` a native module whose
`register` takes a record built in Berry, so a routine file declares itself the way it does today,
and the registry answers the key, the title and the settings it was given.

It is the first of the models the panel's pages are written against, and it is deliberately the
narrowest: a record is registered and read back, and its page and its glyph are still C++-only
values. What is deferred is named at the end.

## The rule is a function over the set

Today a record's rule is split in two: `problem` answers a reason for a set, `repair` asks it again
after every setting it puts back, and the answer is a `const char*` that a Berry file could only
produce out of VM memory — a pointer the native side would have to copy before its next call, and a
sentence travelling back across the boundary so a page can draw it. `repair` takes a section and a
logger for nothing but the wording of that line. The split also leaves a rule reading the record's
settings rather than being handed them, so a routine cannot say what it accepts without
reimplementing the recovery as well.

A record holds one rule instead: a function of its settings, in the order the list shows them, and
of the set to hold. It writes the settings it does not accept and answers nothing. Nothing describes
the rule to the native side — a routine file supplies the function itself, and no structure standing
for the comparison crosses the boundary in either direction. The two names leave with it: there is
no `problem` to ask, and no separate repair for the registry to run.

The two rules in the tree are one such function each: `2_ramped_interval.be` puts `final_pressure`
back when it is under `initial_pressure`, and `5_ramped_static.be` puts `total_duration` back when
it is under the two holds. Each walks its settings in the order the list shows and puts back the
ones it rejects, which is the walk `repair` makes today, and `3_interval.be` holds no rule, which
accepts every set.

The values are the only thing that crosses. The set the store read becomes a Berry map for the call
and is read back from it afterwards, so a rule reads and writes by key; a number arrives in its
kind's type, a duration as whole seconds and a pressure in kPa, at the bounds the kinds carry — a
duration to 7200 s and a pressure to 100 kPa — so a comparison is between figures the rows already
hold. A key the routine writes that the record does not declare is dropped, and a value of the wrong
kind is written as nothing, which reads back as the default. A value inside its kind but outside the
row's bounds is held to them — a pressure written at 400 kPa reads back at 100 — because a set the
editors could never have produced is not one the routine asked for, and no rule sees the set again
after it has written it.

The call is protected, and nothing comes back from it but the set. A raise never unwinds through
the frames holding the values, and nothing is read back from a failed call, so a rule that raises
changes nothing: the load answers the set the store read. The raise is the caller's — a load or save
asked for from Berry answers that error, so a routine's own message reaches whoever made the call,
and a launch that meets one is refused rather than pushed — and the rule is called again on the next
load rather than struck off for raising once.

A repair is not logged. The section and the logger reach `repair` today for nothing but the wording
of `settings: <label>: <reason>`, and both go with that line: a set the rule changes is changed, and
the values read back are what says so.

A record whose rule is neither nothing nor a function is refused when the record registers, with one
line naming the routine, and the registry answers false with what it held unchanged.

Checked: a `ramped_static` document whose `initial_duration` and `target_duration` are 60 and whose
`total_duration` is 30 reads back through `loadSettings` with `total_duration` at its default of
300, and the same document read through the `cold` alternate reads back the same way. The same
document with a total that covers the holds reads back unchanged, value for value. A ladder set
whose `final_pressure` is 3.0 under an `initial_pressure` of 5.0 reads back with `final_pressure` at
its default of 20.0. A rule that writes 400 kPa into a pressure reads back at 100, and one that
writes a key the record does not declare is answered without it. A rule that raises on every call
leaves the document's values in the map it answers, and a `pcall` around a load made from Berry
answers the rule's own message. The interval's record, which holds none, answers every value as the
document gave it. A record whose rule is a
number rather than a function is refused by `routines.register` with one line, and `routines.keys()`
answers what it did before.

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
boot whose `init` takes the routine key, the title, the settings list and the rule, and
`routines.register` moves it into the registry.

The refusals the registry already makes do not move: a key that is not a plain word — empty, or
carrying `/` or `\` — is refused with one line and answers false, the title falls back to the key,
and a key already registered keeps its place while its record and its alternates are replaced. The
document scan that gives a record its alternates stays where it is, because the alternates are the
registry's.

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
- The registry's host cases move to the rule as a function: they assert the values it writes rather
  than a sentence, and the case that reached ` even at the defaults` goes. Nothing reports a repair
  any more — a rule that fixes nothing leaves the set alone, at runtime indistinguishable from one
  that accepted it, and a launch a raising rule refuses answers no page and no line — so the check
  is where a wrong rule shows up. The rule case in `tools/berry/tests/settings.test.be` goes with
  that suite.
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
