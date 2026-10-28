# Registering a routine from Berry

`RoutineRegistry`, `Registration`, the settings rows and the routine document store are C++ now, and
no `.be` file can reach them: `Bindings.cpp` installs `vacuum`, `screen`, `device` and `Config`, and
there is no `routines` module and no `Registration` or `*EditCfg` class. The image's files still
call `routines.register(...)`, `routines.load_settings(...)` and build `Registration`, `Icon` and
`*EditCfg`, so the image does not load. The requirement is in
[register-routines-from-berry.md](../plans/register-routines-from-berry.md).

The two sides of the boundary, as they are named on each:

| Berry | C++ |
| --- | --- |
| `routines.register(record)` | `RoutineRegistry::add(Registration)` |
| `routines.keys()` | `RoutineRegistry::keys()` |
| `routines.title(key)` | `RoutineRegistry::title()` |
| `routines.load_settings(key)` | `RoutineRegistry::loadSettings()` → `Registration::loadSettings()` |
| `routines.save_settings(key, map)` | `RoutineRegistry::saveSettings()` → `Registration::saveSettings()` |
| `Registration(routine_key, title, list)` | `gardener::Registration` (`routines/Registration.h`) |
| `NumericEditCfg` | `gfx::ui::NumberRow` → `gfx::ui::NumberScene` |
| `PressureEditCfg` | `gardener::PressureRow` (`routines/PressureRow.h`) → `PressureScene` |
| `DurationEditCfg` | `gfx::ui::DurationRow` → `gfx::ui::DurationScene` |
| `ChoiceEditCfg` | `gfx::ui::ChoiceRow` → `gfx::ui::ChoiceScene`, entries `gfx::ui::ChoiceItem` |
| the values a set holds | `gfx::ui::SettingValues` (`std::map<String, std::any>`) |

Every item leaves the tree working: `python scripts/arduino.py --task build` compiles. The Berry
side of an item is not reached by that build — `tools/berry/check.py` is broken and is not run, and
a file under `firmware/data/` cannot change what the build says — so a Berry answer is checked by
the suite in item 4.4 when the check runs again, and on the device by the `[E]` lines once the page
work lands. Nothing here touches a page, a glyph, the browser or the settings list; those are the
plan's deferred list, and `rows`, `icon`, `settings_cfgs`, `entry` and `push` stay off the module
until they land.

## Epic 1 — the module and the record

- [ ] 1.1 — The registry the surface answers from belongs to the panel's interpreter owner:
  `Routines::begin` builds the `RoutineRegistry` from the `Library` and `Logger` it already holds,
  before the interpreter is installed and before any file is loaded, and `Routines` answers it.
  Verified: the build compiles, and `RoutineRegistry` is constructed in one place outside
  `tools/unit/cases/registry.cpp`.
- [ ] 1.2 — `Registration` is a class installed at boot beside `Config`, with `routine_key`, `title`
  and `settings` as the fields a subclass may read and an `init(routine_key, title, settings)` that
  stores exactly those three. Verified: the build compiles, and `install()` installs it before the
  interpreter compiles any file, so a routine file can subclass it.
- [ ] 1.3 — `routines` is a module installed by `Bindings.cpp` beside `vacuum`, `screen`, `device`
  and the `Config` class, answering `register(record)`: it reads the record's key and title, builds
  a `gardener::Registration` from them, and answers what `RoutineRegistry::add` answered. The
  registry's refusals stand as they are — a key that is not a plain word answers false and changes
  nothing, a key already registered keeps its place while its record is replaced. Verified: the
  build compiles; the module's callbacks are file-local and `Modules.h` declares the installer
  alone.
- [ ] 1.4 — The module answers `keys()` and `title(key)`: every registered key in registration order
  as a Berry list, and the label the record carries as a string, empty — never a raise — for a key
  that is not registered. Verified: the build compiles, and `title` of a key nothing holds answers
  `""`.

## Epic 2 — the declaration kinds

- [ ] 2.1 — `NumericEditCfg` is a class installed at boot whose
  `init(key, default, label, format, step, min, max, nullable, whole)` builds the
  `gfx::ui::NumberRow` the declaration describes — the default, the step and bounds, the formatter,
  the nullable nothing — over the gesture queue and scene stack the panel's `Routines` holds, and
  `register` puts that row on the record at the position the list declared it. A `whole` declaration
  is a whole number wherever the setting's type shows: it loads and saves as an int, at no decimals,
  where a real one is a float. Verified: the build compiles, and `Routines` answers the queue
  (`presses()`) and the stack a row is built with.
- [ ] 2.2 — `PressureEditCfg` is a class installed at boot whose
  `init(key, default, label, format, step, nullable)` builds a `gardener::PressureRow` over the unit
  the panel prints in, its step being that many whole display steps, the machine's clamp its bound,
  and `register` puts that row on the record in the declared position. A nullable declaration's
  nothing loads as nil rather than as a number. Verified: the build compiles, and the row's key,
  label and default are the declaration's.
- [ ] 2.3 — `DurationEditCfg` is a class installed at boot whose
  `init(key, default, label, format, step, min, max, nullable)` builds a `gfx::ui::DurationRow` of
  whole seconds over the declaration's step and bounds, printed compact as `1m30s`, and `register`
  puts that row on the record in the declared position. Verified: the build compiles, and a
  declaration exceeding its bounds is held to them the way the row holds any value.
- [ ] 2.4 — `ChoiceEditCfg` is a class installed at boot whose
  `init(key, default, label, format, entries, nullable)` builds a `gfx::ui::ChoiceRow` whose entries
  are one `gfx::ui::ChoiceItem` per declared string, key and title alike, and `register` puts that
  row on the record in the declared position. A nullable declaration's nothing loads as nil and
  prints `--`. Verified: the build compiles, and a declaration with no entries answers a row with
  nothing to offer rather than raising at boot.
- [ ] 2.5 — The three routine declarations — `RampedIntervalSettings`, `IntervalSettings` and
  `RampedStaticSettings` — pass the key, the title and the settings list to
  `Registration.init(routine_key, title, [...])` and nothing else, keeping the icon and the factory
  they are handed as their own for the page work. Verified: the three `init` bodies name three
  arguments to `super(self)`, and the lists they pass are unchanged. No build reaches a `.be` file;
  the device's `[E]` lines show it once the page work lands.

## Epic 3 — the set as a map

- [ ] 3.1 — `routines.load_settings(key)` answers a Berry map of the key's settings, each value in
  its kind's type — an int for a whole number and a duration, a real for a pressure, a string for a
  choice, nil for a nullable setting that holds nothing — read from the document through the record
  that declares them, with an alternate's `routine_title` beside them. A key that is not registered
  answers an empty map without opening a file. Verified: the build compiles, and the conversion is
  the one place `SettingValues` becomes a Berry map.
- [ ] 3.2 — `routines.save_settings(key, values)` takes such a map and writes the declared keys once
  through `Registration::saveSettings`, a key the map leaves out written as nothing, which reads
  back as its default; false — writing nothing — for a key that is not registered or a record that
  declares nothing. Verified: the build compiles, and one call writes one document.

## Epic 4 — the suites that drive what is gone

- [ ] 4.1 — The nine suites that name the gone classes are deleted from `tools/berry/tests/`:
  `browser.test.be`, `chromes.test.be`, `home.test.be`, `home_machine.test.be`, `memory.test.be`,
  `ramped_interval.test.be`, `ramped_interval_machine.test.be`, `ramped_static.test.be` and
  `ramped_static_machine.test.be`. Verified: `suite.be` and the suite 4.4 adds are the only suite
  sources left.
- [ ] 4.2 — The art entry points go: `harness_art`, `harness_art_scene`, `harness_art_component`
  and `harness_registered` — which only the art path calls — out of `driver.be`, and `callArt`,
  `callArtScene`, `callArtComponent` and the `--art`, `--scene` and `--component` flags with them
  out of `tools/berry/main.cpp` and `tools/berry/check.py`. Verified: no source under
  `tools/berry/` names a removed entry point.
- [ ] 4.3 — `driver.be`'s plain check is written over the five calls: `harness_check` asks `keys()`
  and then `title()` and `load_settings()` for every key, failing a line when either raises;
  `harness_editor`, `harness_run_page`, `harness_settings_map`, `harness_editor_settled` and
  `harness_activate` go with the settings list, the page and `push` they drive. Verified:
  `driver.be` names no `ux`, no `Scene`, no `SettingsEditScene` and no `settings_cfgs`, and
  `harness_check` is the only function `main.cpp` calls on a plain run.
- [ ] 4.4 — One new suite, `tests/routines.test.be`, covers the surface a routine now reaches: a
  registered record answers its key from `keys()` and its title from `title()`; a key with a slash
  answers false and leaves `keys()` as it was; a key registered twice answers one entry where it
  first took its place; a key nothing holds answers `{}` and `""`; a record declared with one of
  each kind answers a value for every declared key, in its kind's type, a nullable one as nil; and
  a changed value handed to `save_settings` reads back changed from a fresh `load_settings`.
  Verified: the suite is named by no other file and its cases name only the five calls and the four
  classes.
