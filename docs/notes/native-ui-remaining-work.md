# The native UI: what is left

The scene stack, the value editors, the settings list, the band and the mark are C++ and the panel
draws through them. `/system` still loads six Berry files, and five of them name classes that are
gone — components, the scene lifecycle, the settings kinds — so the pages they build cannot run.
This is the checklist of what stands between here and a panel whose every page is native.

## Pages

- [ ] The home page as the stack's root: the title row, the pressure readout in the unit the device
      stores, the run's timer, and the gestures — OK starts or resumes, Cancel stops and vents,
      Forward held to a double opens the browser, Up held to a double opens the settings. It reads
      `vacuum().runState()`; the run's own state machine is `home_machine.be` and is still Berry.
- [ ] The settings list page: `ui::SettingsScene` over a `SettingsRow` per kind, reading and writing
      the device's document, `/cfg/settings.ini`.
- [ ] The browser page: the registry's routines as a list, its rows built from the registry.
- [x] `BerryScene`, a `ui::Scene` that calls into the VM for activate, handle, tick and draw, so a
      Berry page can run before it is ported. It roots the page in a list the VM owns: the collector
      marks a global or a list it holds, and the refstack is not marked. What it does not have is a
      way in — a native `ux.push(page)`/`pop()`/`pop_to_root()` for the pages themselves, and the
      harness case that drives one.
- [x] `screen`, the drawing a page describes: each call answers a `draw` record — one node of the
      tree that says what the page looks like — and `cell`, `align`, `measure` and `duration_text`
      answer the numbers a page divides its boxes with. Nothing walks the tree yet, so a page can
      build its drawing and nothing shows it: rendering the tree is the change that makes the pages
      below drawable.
- [ ] Delete `home.be`, `browser.be`, `registry.be`, `registration.be` and `records.be` as their
      classes land, with their lines in `/system/order`. `home_machine.be` stands alone and can stay
      until the run's machine is native.

## Rows and models

- [ ] A `SettingsRow` subclass per kind. `JingleScene` exists and no row opens it; `NumberScene`,
      `BoolScene`, `ChoiceScene` and `DurationScene` are the same — scenes with nothing to launch
      them.
- [ ] The registry: `Registration` and `RoutineAlias` in C++, with `problem`, `repair`,
      `load_settings`, `save_settings` and the `settings_cfgs` a routine declares.
- [ ] `AutoPressureDisplay`: a reading printed in the panel's unit, which the home page needs.
      `screen.pressure_readout` describes it — the box, the size hint and the two scales — so what is
      left is the drawing: the depth and the target in the unit in force, and the second reading in a
      corner or beside the first.

## The harness

- [ ] `tools/berry/check.py` stops at the scaffolding: `harness.be` and `driver.be` still call `ux`,
      which is deleted, and `routines`, which `registry.be` still defines — so the second failure
      needs a look at the load order or at a load that fails without saying so.
- [ ] The suites in `tools/berry/tests/` cover the deleted classes: components, layout, list,
      settings, overlay, home and browser.
- [ ] `--art` and `--scene` capture pages that no longer exist.

## Docs

- [ ] `docs/specs/ui/bindings.md`, `components.md`, `routine.md` and `scenes.md`, and
      `docs/specs/berry-harness.md`, describe files and classes the tree no longer has.

## Tooling

- [ ] `--task syntax <file>` in `scripts/arduino.py`: keep a verbose build's compile command and
      replay it with `-fsyntax-only` for one file, so a library change is checked in seconds rather
      than by a whole build. `--verbose` now reaches arduino-cli, which is what makes the command
      capturable.

## From the port so far

- The mark as root clips its own art: the band takes rows 52–63 and the mark's art reaches row 53.
  It goes away when a home page takes the root.
- `Routines` has two readers of one gesture queue — `presses()` for scenes and `takePress()` for the
  `device` module — which is deliberate: a press goes to whichever is reading.
