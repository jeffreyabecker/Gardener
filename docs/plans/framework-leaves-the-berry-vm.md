# Moving the framework out of the Berry VM

## What this is

The interpreter's heap holds the compiled form of the framework every page is written against: the
scene lifecycle and the stack that drives it, the registry pages register into, the settings model
and the component model. The check bills this now — [berry-harness.md](../specs/berry-harness.md)
shows `framework 120288 B, image 171602 B` on the host at boot, roughly 60 KB and 86 KB on the
board. This plan states the requirement for moving the framework out: those models stop being
compiled into the VM and become native classes the bindings register before `/system` loads, while
every page keeps its contract unchanged. It deliberately leaves the routines alone: they are pages,
not framework, and loading them later is a different question, deferred to the parking lot.

## Requirements

### The page model stops being compiled into the interpreter

**Now.** `scene.be`, `ux.be`, `registration.be` and `registry.be` are compiled at boot. Their
classes — `Scene`, `Ux`, `Registration`, `RoutineAlias`, `RoutineRegistry` — become heap objects,
billing the boot heap 17,982 host bytes that every page shares.

**Instead.** The same classes are native, registered before the image loads. A page still extends
`Scene`, overrides `to_box`, `activate`, `draw_page`, `handle` and `tick`, and is stamped with its
box and activated by the stack. `ux.push`, `ux.pop` and `ux.pop_to_root` keep the held-request rule
and the raise-empties-the-stack rule, and the firmware's `reset`, `enter`, `poll` and `depth` calls
on `ux` behave as now. A routine still extends `Registration`, and `routines.register`, `keys`,
`rows`, `title`, `icon`, `settings_cfgs`, `load_settings`, `save_settings` and `push` answer as they
do. A scene's `self.pos` and `self.dim` keep reading and writing the same way, however the native
class carries them.

**Number.** The boot heap falls by 17,982 host bytes (about 9 KB on the board), and the check's
`framework` figure falls by the same.

**Check.** The check passes — every routine loads, every page drives, every suite case passes — and
`--memory` no longer lists the four files.

### The settings model stops being compiled into the interpreter

**Now.** `settings/*.be` compiles the editor base, the kinds, the settings list and the choice ring,
billing 41,768 host bytes at boot — the largest single group, even after the merge of the number
kinds that is in flight.

**Instead.** The editor base and the kinds are native. A routine's settings declaration still builds
the `EditCfg` records, `cfg.scene(value)` still opens the editor scene, the list still walks the
kinds and writes a settled set through the routine's rule, and the home scene's `DeviceSettings`
still edits the device set. Nothing a page reads — the cfg's fields, the scene factory, the editor's
value, nudge and draw — changes shape.

**Number.** The boot heap falls by the settings group's compiled cost, 41,768 host bytes today
(about 21 KB on the board), whatever the merge leaves it at when the work starts.

**Check.** The check passes — the settings suite and every routine's settings walk pass — and
`--memory` no longer lists `/system/settings/*.be`.

### The component model stops being compiled into the interpreter

**Now.** `components/*.be` compiles the `Component` base and the standard components, billing
18,817 host bytes at boot.

**Instead.** `Component` and the standard components — text, layout, icon, progress bar, scrollbar,
pressure display, duration — are native, drawing through the same `screen` module. A page still
calls `super(self).init()`, `set_box`, `use` and `draw` with the same ink, and the layout still
places children by the same box arithmetic. A component's `pos`, `dim` and `inverted` keep reading
and writing the same way.

**Number.** The boot heap falls by 18,817 host bytes (about 9.4 KB on the board).

**Check.** The check passes — the components and layout suites pass — and `--memory` no longer lists
`/system/components/*.be`.

## Consequences

- The harness spec's "what it holds" section: the `framework` figure falls by the three groups'
  combined cost, about 78,567 host bytes (39 KB on the board), and the `--memory` table shrinks to
  the files still compiled. The word "framework" still means `/system`; what changes is how much of
  it is Berry.
- The UI specs — the scene lifecycle, the components, the bindings and the routine registration —
  state the models that stay. They change only where they name the classes' implementation, not the
  contract a page is written against.
- The load order: `/system/order` names fewer files, and the record classes plus the pages that
  remain are what the loader compiles.
- The machine, the wire and the control law are untouched: the change is the bindings and the data
  image's system files. No serial-protocol or vacuum-control change.

## Deferred

- Loading routines on demand, so only the active page's compiled form bills the boot heap — about
  25 KB on the board. A loader-contract change, not a code move; it has its own parking-lot line.
- The record classes in `records.be` — `point`, `bounds`, `reading`, `unit`, `jingle`, `links` —
  stay Berry. Only the parser declares instance variables today, and the native callbacks answer
  with these records, so moving them is a separate question.

## How it is known to hold

`python tools/berry/check.py` passes — every file compiles, every registration looks like a routine,
and every page survives the drive — and `python tools/berry/check.py --tests` passes with the same
case count, the memory suite reporting each page against the smaller boot. `python
tools/berry/check.py --memory` shows the `framework` figure down by the three groups' combined cost
and the table free of their files.
