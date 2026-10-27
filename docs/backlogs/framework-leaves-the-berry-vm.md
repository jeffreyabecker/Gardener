# The framework leaves the Berry VM

The backlog for [framework-leaves-the-berry-vm.md](../plans/framework-leaves-the-berry-vm.md). The
component model, the scene lifecycle with the stack that drives it, the registry and the settings
model stop being Berry classes the interpreter compiles and become native classes the bindings
register before `/system` loads. Every page keeps the contract it is written against: the same
classes to extend, the same calls answering the same values.

The epics are worked in order — the C++ component framework, the page model, the component
bindings, the settings and then the record — because each is written in terms of the one before: a
scene draws components, and the settings editors are scenes built from components. The C++ is
finished and checked in C++ before any of it is bound to the interpreter. Every item leaves the tree
working, with `python tools/berry/check.py` passing and the pages drawing what they drew before. The
C++ check and the Berry check are what an item is verified by; a firmware build is run at the end of
each epic, where the accumulated C++ is the compiler's question.

What stays Berry is what a page is: `records.be`, `list.be`, `overlay.be`, the browser, the home
scene and the routines themselves, and `components/pressure_display.be`, which reads the machine's
readings rather than drawing them and is not part of this move. The plan's deferred items — loading
routines on demand, and the record classes the native calls answer with — are not this backlog's
work.

A class that lands in a new source file is also named in `check.py`'s source list, so the host
harness compiles what the device does, and a `.be` file leaves `/system/order` once the last class
in it is native. Each epic's summary is written under
`docs/backlogs/completed/framework-leaves-the-berry-vm/` as the epic's last item lands.

"Native" takes two shapes in this backlog. The pure framework — the components — is ordinary C++
in `libraries/Gfx/src/ui/`, in `namespace gardener::gfx::ui`: typed fields and plain methods, no
`bvm`, no Berry, drawing through a `Canvas` over a `DisplayDevice`. It is implemented and verified
in C++, against a stub `DisplayDevice`, before any Berry touches it. The
Berry-coupled models — the scene, the stack, the registry and the settings kinds — are C++ classes
in `libraries/Gardener/`, wrapped by a file under `routines/bindings/` that unpacks arguments,
calls the class and pushes its answer, and that alone reaches a page's own override back through
the interpreter. Neither shape leaves a `.be` file beside its class: the two drift, and the image
pays the compile cost of both. For the component framework, a Berry suite proves only that a
binding forwards; the drawing and layout behaviour is proven in C++.

## Epic 1 — the C++ component framework

The drawing primitives a page composes its content from, as ordinary C++ classes in
`libraries/Gfx/src/ui/`. Nothing in them names `bvm` or Berry: they hold typed fields and draw
through a `Canvas` over a `DisplayDevice`. Each one is implemented and verified in C++ against a
stub `DisplayDevice`, before any binding exists.

The classes, complete: `Component`, `Text`, `Icon`, `ProgressBar`, `Scrollbar`, `Duration`,
`Layout`, `RowLayout`, `ColumnLayout`. With them three value types the model needs: `Size`, the
room a component answers from `use`; `Cell`, the share of its parent a layout child declares; and
`Alignment`, where a thing sits in the space it is given.

`PressureDisplay` and `AutoPressureDisplay` are deliberately not here: they read the machine's
readings and print them in the panel's unit, which is the machine's business rather than the
drawing's, so both stay in `components/pressure_display.be`.

- [x] 1.1 — The host check: `tools/gfx/check.py` builds the Gfx sources with a `main.cpp` against a
  stub `DisplayDevice` — a bitmap answering `size`, `bitsPerPixel`, `buffer` and `flush` and keeping
  the pixels for assertions — and runs named checks, exiting non-zero on a failure, the way
  `tools/berry/check.py` does. Verified: the check builds, and a trivial case passes and a broken
  one fails.
- [x] 1.2 — `Component` in `libraries/Gfx/src/ui/Component.h`: the box as a `Rect`, `inverted`,
  `setBox`, the `use(w, h)` room hook answering the whole cell, and `draw` running the inverted
  flip last, which is what every subclass's base call lands on. Verified by a C++ case that draws a
  plain and an inverted component and reads the differing pixels back from the stub.
- [x] 1.3 — `Text` in `Text.h`: the value as a NUL-terminated string, the `Font*` and the scale,
  `use` answering `measureText`'s ink box clipped to the cell it was given, and `draw` laying the
  string down then the flip. Verified by the C++ port of the text cases — the face, the scale, the
  no-face system face and the clipped ink.
- [x] 1.4 — `Icon` in `Icon.h`: the 12×8 default box and the glyph hook a subclass draws in it, the
  base draw only the flip. Verified by a C++ subclass drawing its glyph into the stub.
- [x] 1.5 — `ProgressBar` in `ProgressBar.h`: the value clamped to 0–100, the framed fill inside the
  box. Verified by the C++ port of the bar-at-its-ends case.
- [x] 1.6 — `Scrollbar` in `Scrollbar.h`: `hollow`, `index`, `count` and `visible`, the thumb sized
  and placed from them, clamped to the box. Verified by the C++ port of the thumb case.
- [x] 1.7 — `Duration` in `Duration.h`, on `Text`: the seconds set before a draw and the text for
  them — minutes, seconds, hours, or the `--` for none. Verified by the C++ port of the
  duration-format case.
- [x] 1.8 — `Layout` in `Layout.h`, with `RowLayout` and `ColumnLayout` and the cell and alignment
  value types: `place` resolving each child's declared cell against the box, the siblings stacked
  along the axis they divide, each narrowed to its `use` room and moved to where the alignment puts
  it, and `draw` drawing the children in order before the flip. Verified by the C++ port of the
  layout suite's box cases — the equal thirds, the rounded-span clamp, the alignment, the pixel
  row and the drawing order.

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

## Epic 3 — the component bindings

Thin `be_pushclass` wrappers under `routines/bindings/`, one file per Gfx class, the C++ object held
on the instance as `Config`'s is. A callback unpacks its arguments, calls the class and pushes its
answer; the wrapper alone reaches a page's override back through the interpreter, so a Berry
subclass still overrides `draw` and ends with `super(self).draw()`. The Berry suites that covered
the drawing and the layout arithmetic lose those cases to the C++ check and keep only the
forwarding ones.

- [ ] 3.1 — The `Component` binding and the bridge it proves: a Berry class extends the native base,
  overrides `draw`, and `super(self).draw()` reaches the native flip. `component.be` leaves the
  image. Verified: `python tools/berry/check.py --tests` passes with the components suite reduced to
  the forwarding cases, and the routine frames still draw.
- [ ] 3.2 — The `Text` and `Icon` bindings: the string, face and scale in, the ink box back, and the
  glyph hook a Berry subclass draws. `text.be` and `icon.be` leave the image. Verified: the
  remaining text and icon cases pass, and the browser and settings rows draw their icons as before.
- [ ] 3.3 — The `ProgressBar` and `Scrollbar` bindings. Their `.be` files leave the image. Verified:
  the reduced cases pass and a routine's bar and a scrolled list draw as before.
- [ ] 3.4 — The `Duration` binding, on the `Text` binding. `duration.be` leaves the image. Verified:
  the reduced case passes and the timer in a chrome frame reads as before.
- [ ] 3.5 — The `Layout`, `RowLayout` and `ColumnLayout` bindings: the children list, the cell and
  alignment translated between Berry's records and the C++ types. `HomeChrome`,
  `RampedIntervalChrome`, `RampedStaticChrome` and the overlay's band still extend the native base
  from `.be`, and their pinned boxes are unchanged. `layout.be` leaves the image. Verified: the
  layout and chrome suites are reduced to forwarding, and the band and the chrome frames still
  draw.
- [ ] 3.6 — `/system/order` drops the component files it no longer needs, and `--memory` no longer
  lists them: the check's `framework` figure falls by all but the pressure display's share of the
  component group. `pressure_display.be` stays, and still draws the home and chrome figures.
  Verified by `python tools/berry/check.py --memory` and the full `--tests` run.

## Epic 4 — the settings model

The editor base and the kinds: what a routine declares and what the panel edits it with. This is the
largest of the three groups, and it is last because its scenes are built on `Scene` and its editors
on the components.

- [ ] 4.1 — `EditCfg` is native — the record a routine's declaration builds: key, default, label,
  icon and nullable, the bounds a value is held to, `parse`, `value`, `text`, the scene it opens,
  and the `problem` that keeps an unusable kind out of a declaration. The number kind, the base
  editor's own, is native with it, whatever the number kinds' merge leaves that as. Verified: the
  settings cases pass, every routine's declaration logs what it logged before, and `--memory` no
  longer lists `/system/settings/edit_cfg.be` or `/system/settings/number.be`.
- [ ] 4.2 — `EditScene` is native — the modal value editor every kind extends: the band, the
  heading, the hint, the measure and the percent, the keys `handle` answers, `nudge`, the hold,
  `cancel` and `draw_page`. Verified: every value editor drives as before, the settings cases pass,
  and `--memory` no longer lists `/system/settings/edit_scene.be`.
- [ ] 4.3 — `settings.be` is native: `duration_seconds` and `duration_text`, and `ChoiceItem` with
  `ChoiceRing` — the entry a choice is held as, the ring the editor steps through, `current` and
  `nudge`. Verified: the settings cases pass, a choice row and a duration row read as before, and
  `--memory` no longer lists `/system/settings/settings.be`.
- [ ] 4.4 — `SettingsView` is native: the row pitch, the gap under a heading, the scale a value is
  drawn at and the rows a box holds. Verified: the list draws the same rows, and `--memory` no
  longer lists `/system/settings/view.be`.
- [ ] 4.5 — `BoolEditScene` and `BoolEditCfg` are native: the on, off and nil value, its nudge, its
  text and the scene it opens. Verified: the settings cases and home's device list pass, and
  `--memory` no longer lists `/system/settings/bool.be`.
- [ ] 4.6 — `ChoiceEditScene` and `ChoiceEditCfg` are native: the ring and the list built from it
  once the scene has its box, the nil entry a nullable setting carries, an entry's own gestures, and
  the cfg's normalization, parse and text. Verified: the settings cases pass, the jingle row and
  every routine's choice setting walk, and `--memory` no longer lists `/system/settings/choice.be`.
- [ ] 4.7 — `PressureEditScene` and `PressureEditCfg` are native: the pressure a value is held to,
  its step, its low end, its text and the scene it opens. Verified: the settings cases and each
  routine's pressure setting walk pass, and `--memory` no longer lists
  `/system/settings/pressure.be`.
- [ ] 4.8 — `DurationEditScene` and `DurationEditCfg` are native: minutes and seconds, the hold a
  step repeats on, the bounds, the text and the scene. Verified: the settings cases and the interval
  routines' duration setting walk pass, and `--memory` no longer lists
  `/system/settings/duration.be`.
- [ ] 4.9 — `JingleEditScene` and `JingleEditCfg` are native, on the native choice pair: the library
  the row draws from, the preview a pick plays and the cancel that stops it. Verified: the settings
  cases and home's jingle row pass, and `--memory` no longer lists `/system/settings/jingle.be`.
- [ ] 4.10 — `SettingsEditScene` is native: one row per kind with the value it shows, the heading,
  the window the box holds, Forward and OK on the selected row, and Back writing the settled set
  through the routine's rule. Verified: the drive's settings walk passes for every routine, so every
  key lands, and `--memory` no longer lists `/system/settings/settings_edit.be`.
- [ ] 4.11 — `DeviceSettings` and `BluetoothEditCfg` are native: the device set home edits, the rule
  it holds, and the row that keeps a Bluetooth setting's own scene. Verified: home's settings list
  opens, edits and closes with the same values written, and `--memory` no longer lists
  `/system/settings/device.be`, so no file under `/system/settings/` is in the table.

## Epic 5 — the record

The documents that state the contract, and the accounting the plan asks for.

- [ ] 5.1 — [berry-harness.md](../specs/berry-harness.md) states the smaller boot: the `framework`
  figure down by the three groups' combined cost — about 78,567 host bytes — the `--memory` table
  free of their files, and the load order naming only what `/system` still compiles. Verified: the
  figures are the ones `python tools/berry/check.py --memory` and `--tests` print on the tree.
- [ ] 5.2 — [routine.md](../specs/ui/routine.md) describes the scene, the stack, the registration,
  the registry and the settings kinds as the bindings' native classes, with `/system/order` naming
  only the files still compiled and a new system file still an edit to it.
- [ ] 5.3 — [bindings.md](../specs/ui/bindings.md) lists those classes as part of the surface a
  routine calls, beside the three modules and the records, rather than as `/system` files.
- [ ] 5.4 — [components.md](../specs/ui/components.md) names the component classes in
  `libraries/Gfx/src/ui/` where it names an implementation, the box, `use`, the inverted flip and
  the construction contract unchanged.
- [ ] 5.5 — [scenes.md](../specs/ui/scenes.md) points its scene map at the bindings where a base
  scene is implemented, every scene, gesture and box unchanged.
