# The native panel

The panel's scene stack, value editors and settings list are C++, and `/system` still loads six
Berry files, five of which name classes the tree no longer has — the component model, the scene
lifecycle and the settings kinds — so the pages they build cannot load. This is the work between
that image and a panel whose every page is native. The registry epic is what `link-the-routines.md`
asked for, in `docs/archive/specs-and-plans.zip`; the rest is the port the native scenes already
made possible, and the requirement for it is what the page does today.

Every item leaves the tree working: `python scripts/arduino.py --task build` passes, and what
touches `libraries/Gfx` or `libraries/Gardener` passes `python tools/gfx/check.py`, while what
touches the image passes `python tools/berry/check.py`.

## Epic 1 — the settings list

- [ ] 1.1 — A `SettingsRow` per setting kind, each building the scene that edits it: a number row a
  `NumberScene`, a bool row a `BoolScene`, a choice row a `ChoiceScene` over the entries it is handed,
  a duration row a `DurationScene`, a jingle row a `JingleScene` over the jingle library, and a
  pressure row a `PressureScene`. Verified: `tools/gfx/check.py` drives a list holding one row of
  each kind, opens each row's scene on Forward and reads back the value it answered; a jingle row
  that is cancelled leaves the key it opened on and sounds nothing.
- [ ] 1.2 — The rows a page builds from the device's document: `/cfg/settings.ini` read through the
  storage layer, one row per key it declares in document order, carrying the key, the label its kind
  gives it and the value in force, and no row for a key the document does not declare. Verified: a
  case over a fixture document answers the rows the fixture declares and none for the key it leaves
  out.
- [ ] 1.3 — The settings list reached from the panel: Up held to a double on the home page pushes
  `SettingsScene` over those rows, Forward opens the selected row's editor, and OK hands the settled
  set back to the document. Verified: the panel drives Up, an edit and OK, and `/cfg/settings.ini`
  holds the new value afterwards; Cancel leaves the document as it was opened.

## Epic 2 — the registry

- [ ] 2.1 — `Registration` carries the states and commands a routine declares and the kind each of
  its setting configs is, with the cut rule: a declaration naming what it does not declare loses that
  name at registration. Verified: a routine that names a state it never declares is registered and
  read back without it.
- [ ] 2.2 — The table is collected once, at boot, before the link tasks spawn, and `current_state()`
  answers the page's state from it rather than from the interpreter. Verified: the check asserts the
  table's routines, states, commands and settings for the three routines, and that a read of the
  current routine and its state runs with no VM call.
- [ ] 2.3 — The panel renders the table its own way: a routine page's title, its `settings_cfgs`,
  its states and its commands come from the table, so the C++ page and the wire cannot disagree
  about an order or a value. Verified: a page built from a routine's registry entry prints the
  title and settings the table holds.

## Epic 3 — the browser and the image

- [ ] 3.1 — The browser page is a native `Scene`: the rows are the registered routines in registry
  order, Up and Down move with the selection inverted, Forward opens the selected routine's settings
  list over its declared `settings_cfgs` and its `.ini`, and OK launches it on the set it settled.
  Verified: `tools/gfx/check.py` drives the list and a launch, and the launched page opens on the
  settled settings.
- [ ] 3.2 — The browser's settings list is a `SettingsScene` over rows built from the routine's
  declaration, not from a Berry record, and OK hands the set to the launch. Verified: a case opens a
  routine's list, edits one setting and reads the value the launch started on.
- [ ] 3.3 — `browser.be`, `registry.be`, `registration.be` and `records.be` leave the image and
  `/system/order`, and routines register through the C++ record instead. Verified:
  `python tools/berry/check.py --memory` lists neither the files nor the objects they made, and the
  three routines still drive.
- [ ] 3.4 — `home.be` and `home_machine.be` leave the image once the home page is native: the stack's
  root is a C++ scene, and the mark stays the fallback for an image that names none. Verified:
  `--scene home` draws the frame the Berry page drew.

## Epic 4 — the harness

- [ ] 4.1 — `harness.be` and `driver.be` stop calling `ux`: the harness loads the image the way the
  firmware does and drives the native stack through the binding, so the check runs past its
  scaffolding. Verified: `python tools/berry/check.py` completes.
- [ ] 4.2 — The suites that drove deleted Berry classes go, and what they asserted moves to
  `tools/gfx/check.py` against the native scene: components, layout, list, settings, overlay, home
  and browser. Verified: no suite names a class the tree does not have, and the gfx check covers
  what the suites covered.
- [ ] 4.3 — `--art` and `--scene` name pages that exist. Verified: every named capture draws a frame.
- [ ] 4.4 — `--task syntax <file>` in `scripts/arduino.py`: keep a verbose build's compile command
  and replay it with `-fsyntax-only` for one file, so a library change is checked in seconds rather
  than by a whole build. Verified: the task compiles one file, and fails on a deliberate syntax
  error in it.

## Epic 5 — the record

- [ ] 5.1 — The drawing verbs get a spec: a file in `docs/specs/` listing the box a call takes, the
  composed verbs and the distribution functions, in place of the archived component spec its
  predecessor wrote. Verified: the file names no component class.
- [ ] 5.2 — `routine.md`, `scenes.md` and `bindings.md` state the page box, the drawing a page does
  and the modules a routine can call as the tree has them. Verified: none of the three names a class
  or a file the tree no longer has.
- [ ] 5.3 — The check gets a spec: a file in `docs/specs/` stating the image it loads, the scenes it
  drives and the captures it writes. Verified: every command in the file runs as written.
