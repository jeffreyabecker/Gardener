# The framework leaves the Berry VM

The backlog for [framework-leaves-the-berry-vm.md](../plans/framework-leaves-the-berry-vm.md). The
component model, the scene lifecycle with the stack that drives it, the registry and the settings
model stop being Berry classes the interpreter compiles and become native classes the bindings
register before `/system` loads. Every page keeps the contract it is written against: the same
classes to extend, the same calls answering the same values.

The epics are worked in order — the components, then the page model, then the settings — because
each is written in terms of the one before: a scene draws components, and the settings editors are
scenes built from components. Every item leaves the tree working, with `python
tools/berry/check.py` passing and the pages drawing what they drew before. The check compiles the
bindings on the host, so it is what an item is verified by; a firmware build is run at the end of
each epic, where the accumulated C++ is the compiler's question.

What stays Berry is what a page is: `records.be`, `list.be`, `overlay.be`, the browser, the home
scene and the routines themselves. The plan's deferred items — loading routines on demand, and the
record classes the native calls answer with — are not this backlog's work.

A class that lands in a new source file is also named in `check.py`'s source list, so the host
harness compiles what the device does, and a `.be` file leaves `/system/order` once the last class
in it is native. Each epic's summary is written under
`docs/backlogs/completed/framework-leaves-the-berry-vm/` as the epic's last item lands.

## Epic 1 — the component model

The drawing primitives a page composes its content from, and the native class path the rest of the
effort follows: a class the bindings register before the image loads that a `.be` class still
extends — declaring its own instance variables, overriding what it overrides, and reaching the base
with `super(self)`.

- [ ] 1.1 — `Component` is native: `init`, `set_box`, `use` and the inverted flip in `draw`, with
  `pos`, `dim` and `inverted` read and written as now. Every standard component, `ListRow`, `List`,
  the overlay's cells and the page chromes still extend it from `.be`, keep their own instance
  variables and reach the base with `super(self)`. Verified: `python tools/berry/check.py --tests`
  passes, the component frames draw as before, and `--memory` no longer lists
  `/system/components/component.be`.
- [ ] 1.2 — `Text` is native: the string at the face and scale it was built with, `use` answering
  the ink box the layout places it by, and `draw` laying down the same ink, the inherited inverted
  flip included. Verified: the four text cases pass, `--art` of a routine draws the same frame, and
  `--memory` no longer lists `/system/components/text.be`.
- [ ] 1.3 — `Icon` is native: the glyph, drawn in the box it is given. Verified: the browser and the
  settings list draw their icons as before, and `--memory` no longer lists
  `/system/components/icon.be`.
- [ ] 1.4 — `ProgressBar` is native: the bar at its value, in the box it is given. Verified: the
  progress-bar case passes, a routine's bar draws as before, and `--memory` no longer lists
  `/system/components/progress_bar.be`.
- [ ] 1.5 — `Scrollbar` is native: the thumb for the rows showing out of the rows there are, drawn
  inside its box. Verified: the scrollbar case and the list cases pass, a scrolled list draws as
  before, and `--memory` no longer lists `/system/components/scrollbar.be`.
- [ ] 1.6 — `PressureDisplay` is native, and `AutoPressureDisplay` with it: the main figure and the
  secondary one, the corner and side-by-side placements, the scales each picks and the text each
  prints. Verified: the three pressure-display cases pass, the home and chrome frames draw the same
  figures, and `--memory` no longer lists `/system/components/pressure_display.be`.
- [ ] 1.7 — `Duration` is native on the native `Text`: the format, the text it prints for a number
  of seconds, and its draw. Verified: the duration-format case passes, the timer in a chrome frame
  reads as before, and `--memory` no longer lists `/system/components/duration.be`.
- [ ] 1.8 — `Layout` is native, with `RowLayout` and `ColumnLayout`: `bounds`, the children and the
  alignment, `place` stamping each child's `pos` and `dim` before `draw` draws them in order, and
  the span and alignment arithmetic answering the same boxes. `HomeChrome`,
  `RampedIntervalChrome`, `RampedStaticChrome` and the overlay's band still extend it from `.be`,
  and their pinned boxes are unchanged. Verified: the layout, chromes and overlay cases pass with
  the same boxes, the band still draws, and `--memory` no longer lists
  `/system/components/layout.be`.

## Epic 2 — the page model

The lifecycle a page extends, the stack that drives it, the record a routine registers and the
registry the browser draws. The four classes load before the routines today because every page
names them; native, they are installed before the image loads for the same reason.

- [ ] 2.1 — `Scene` is native: `init`, `default_box` answering the page box — the panel less the
  band — `to_box` stamping `pos` and `dim`, `take_events` draining the gesture queue, `handle`,
  `activate`, `draw_page`, `tick`, `draw` filling its box with paper and calling `draw_page`, and
  `poll` repainting on the 250 ms cadence. A page still overrides only what differs, and `ux` still
  stamps the box before `activate()` runs. Verified: every routine, the home scene and the browser
  drive as before, and `--memory` no longer lists `/system/scene.be`.
- [ ] 2.2 — `Ux` is native, and the global `ux` with it: the frames, the one held request, the
  polling flag and the band the overlay builds, which `ux.band` still answers. `reset`, `enter`,
  `poll` and `depth` answer the firmware as now; `push`, `pop` and `pop_to_root` keep the
  held-request rule, the eight-frame cap, the callback a popped scene hands its value to, and the
  re-activation of the scene below; a raise empties the stack and goes on to the firmware; the band
  is cleared and drawn after the page on every beat. Verified: the driver's scene re-entry and the
  depth the browser and settings cases assert are unchanged, and `--memory` no longer lists
  `/system/ux.be`.
- [ ] 2.3 — `Registration` and `RoutineAlias` are native: the key, title, icon, factory and settings
  declaration a routine still extends it with; `problem`, `repair`, `load_settings` and
  `save_settings` answering as now — the routine's document opened once per call, an alternate's
  `routine_title` carried on the map, and a kind that cannot work logged and left out. Verified:
  every routine registers, the alias and settings cases pass, and `--memory` no longer lists
  `/system/registration.be`.
- [ ] 2.4 — `RoutineRegistry` is native, and the global `routines` with it: `register`, `keys`,
  `rows`, `entry`, `title`, `icon`, `settings_cfgs`, `load_settings`, `save_settings` and `push`
  answer as now, the routine's document is still scanned for aliases when it registers, and the
  browser still draws one row per key. Verified: the alias and browser cases pass, and `--memory` no
  longer lists `/system/registry.be`, so `/system/order` names only the files still compiled.

## Epic 3 — the settings model

The editor base and the kinds: what a routine declares and what the panel edits it with. This is the
largest of the three groups, and it is last because its scenes are built on `Scene` and its editors
on the components.

- [ ] 3.1 — `EditCfg` is native — the record a routine's declaration builds: key, default, label,
  icon and nullable, the bounds a value is held to, `parse`, `value`, `text`, the scene it opens,
  and the `problem` that keeps an unusable kind out of a declaration. The number kind, the base
  editor's own, is native with it, whatever the number kinds' merge leaves that as. Verified: the
  settings cases pass, every routine's declaration logs what it logged before, and `--memory` no
  longer lists `/system/settings/edit_cfg.be` or `/system/settings/number.be`.
- [ ] 3.2 — `EditScene` is native — the modal value editor every kind extends: the band, the
  heading, the hint, the measure and the percent, the keys `handle` answers, `nudge`, the hold,
  `cancel` and `draw_page`. Verified: every value editor drives as before, the settings cases pass,
  and `--memory` no longer lists `/system/settings/edit_scene.be`.
- [ ] 3.3 — `settings.be` is native: `duration_seconds` and `duration_text`, and `ChoiceItem` with
  `ChoiceRing` — the entry a choice is held as, the ring the editor steps through, `current` and
  `nudge`. Verified: the settings cases pass, a choice row and a duration row read as before, and
  `--memory` no longer lists `/system/settings/settings.be`.
- [ ] 3.4 — `SettingsView` is native: the row pitch, the gap under a heading, the scale a value is
  drawn at and the rows a box holds. Verified: the list draws the same rows, and `--memory` no
  longer lists `/system/settings/view.be`.
- [ ] 3.5 — `BoolEditScene` and `BoolEditCfg` are native: the on, off and nil value, its nudge, its
  text and the scene it opens. Verified: the settings cases and home's device list pass, and
  `--memory` no longer lists `/system/settings/bool.be`.
- [ ] 3.6 — `ChoiceEditScene` and `ChoiceEditCfg` are native: the ring and the list built from it
  once the scene has its box, the nil entry a nullable setting carries, an entry's own gestures, and
  the cfg's normalization, parse and text. Verified: the settings cases pass, the jingle row and
  every routine's choice setting walk, and `--memory` no longer lists `/system/settings/choice.be`.
- [ ] 3.7 — `PressureEditScene` and `PressureEditCfg` are native: the pressure a value is held to,
  its step, its low end, its text and the scene it opens. Verified: the settings cases and each
  routine's pressure setting walk pass, and `--memory` no longer lists
  `/system/settings/pressure.be`.
- [ ] 3.8 — `DurationEditScene` and `DurationEditCfg` are native: minutes and seconds, the hold a
  step repeats on, the bounds, the text and the scene. Verified: the settings cases and the interval
  routines' duration setting walk pass, and `--memory` no longer lists
  `/system/settings/duration.be`.
- [ ] 3.9 — `JingleEditScene` and `JingleEditCfg` are native, on the native choice pair: the library
  the row draws from, the preview a pick plays and the cancel that stops it. Verified: the settings
  cases and home's jingle row pass, and `--memory` no longer lists `/system/settings/jingle.be`.
- [ ] 3.10 — `SettingsEditScene` is native: one row per kind with the value it shows, the heading,
  the window the box holds, Forward and OK on the selected row, and Back writing the settled set
  through the routine's rule. Verified: the drive's settings walk passes for every routine, so every
  key lands, and `--memory` no longer lists `/system/settings/settings_edit.be`.
- [ ] 3.11 — `DeviceSettings` and `BluetoothEditCfg` are native: the device set home edits, the rule
  it holds, and the row that keeps a Bluetooth setting's own scene. Verified: home's settings list
  opens, edits and closes with the same values written, and `--memory` no longer lists
  `/system/settings/device.be`, so no file under `/system/settings/` is in the table.

## Epic 4 — the record

The documents that state the contract, and the accounting the plan asks for.

- [ ] 4.1 — [berry-harness.md](../specs/berry-harness.md) states the smaller boot: the `framework`
  figure down by the three groups' combined cost — about 78,567 host bytes — the `--memory` table
  free of their files, and the load order naming only what `/system` still compiles. Verified: the
  figures are the ones `python tools/berry/check.py --memory` and `--tests` print on the tree.
- [ ] 4.2 — [routine.md](../specs/ui/routine.md) describes the scene, the stack, the registration,
  the registry and the settings kinds as the bindings' native classes, with `/system/order` naming
  only the files still compiled and a new system file still an edit to it.
- [ ] 4.3 — [bindings.md](../specs/ui/bindings.md) lists those classes as part of the surface a
  routine calls, beside the three modules and the records, rather than as `/system` files.
- [ ] 4.4 — [components.md](../specs/ui/components.md) names the bindings where it names an
  implementation, the box, `use`, the inverted flip and the construction contract unchanged.
- [ ] 4.5 — [scenes.md](../specs/ui/scenes.md) points its scene map at the bindings where a base
  scene is implemented, every scene, gesture and box unchanged.
