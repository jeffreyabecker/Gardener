# Named saved routines

The routine browser lists one row per routine **definition** — each `.be` file that calls
`routines.register(...)` — and a routine's values live in `/routines/<name>.cfg`, named after that
definition. There is therefore exactly one saved set of values per routine, and keeping a second
configuration of the same routine means shipping a second copy of its `.be`.

This plan separates code from configuration. A **definition** is a routine's code: its scene, its
icon and its settings schema. A **saved routine** is one named configuration of a definition: a
config carrying a display title, which definition it instantiates, and the settings that
instantiation uses. The browser lists saved routines by title; definitions stop being browser rows
and become what a saved routine is made from.

A saved routine's file is named by the device, not by the user: the title is a label and nothing
else, so it may contain capitals and spaces, and two saved routines are told apart by their titles
and by nothing in their filenames.

The control law, the wire, the scene stack and the gesture map are unchanged. The one new surface
is a scene that composes a title from the six keys.

## Requirements

### 1. Definitions load from `/modules/routines/`

Today `/system/*.be` (in the order `/system/order` names), `/modules/*.be` and `/routines/*.be` are
the folders the interpreter's loader knows (`Routines::begin` in
`libraries/PanelLink/src/routines/Routines.cpp`), and every `.be` under `/routines/` is a definition
the browser lists. The code and the configs therefore share one folder: adding a config means adding
it beside code, and the only thing that makes a `.be` file a definition rather than a module is
which folder it sits in.

Instead a definition lives at `/modules/routines/<name>.be` and loads **after** `/modules/*.be`.
`/routines/` holds configs and no `.be` at all. Definitions are still compiled after
`/system/registry.be`, `/system/scene.be` and `/system/components/component.be`, because Berry
resolves a global name at compile time and every definition calls `routines.register`, extends `Page`
and names an `Icon`.

Check: `fs list /routines` answers only `.cfg` files; the three definitions load from
`/modules/routines/`; the boot summary reports the same number of registered routines as today.

### 2. A config carries a title and its definition

Today `/routines/<name>.cfg` is a flat `key=value` document of settings only; the filename is the
routine's name, and nothing in the file says what routine it belongs to.

Instead every config carries two reserved keys beside the settings:

```
title=Bedtime
routine_name=interval
set_count=6
target_pressure=8.0
```

`title` is the label the browser draws and nothing else: not a filename, not a path, not anything
the store addresses. It may contain capitals and spaces — 1–16 characters drawn from `a`–`z`,
`A`–`Z`, `0`–`9`, space, `_` and `-`. `routine_name` is the definition instantiated when the config
is opened. The remaining keys are that definition's settings, read and written exactly as today.

A config's identity is its **filename stem**, which the device generates: four digits from `0001` to
`9999`, written as `/routines/<stem>.cfg`. Creation takes the lowest stem whose file does not exist,
and is refused when none is free. The stem is what `store.get`/`store.set` address and what the
browser hands a page, so a hand-written file remains addressable by its own.

The two reserved keys have defined absences. A config with no `title` lists under its filename stem.
A config with no `routine_name`, or one naming an unregistered definition, lists as a text-only row
that refuses to open and logs why. A config that cannot be parsed is skipped with a log line and
does not break the list.

The three shipped configs keep their filenames — `interval.cfg`, `ramped_interval.cfg`,
`ramped_static.cfg` — and gain `title=` (the definition's own title) and `routine_name=`. Their
stems are not four digits, and that is deliberate: a definition pushed bare by the harness or
`--art` writes `/routines/<definition>.cfg`, so keeping these names means the drive lands in a real
config instead of creating an orphan with no title.

Check: `fs read /routines/interval.cfg` shows both reserved keys above the settings; a config
written without `routine_name` lists and does not open; a newly created config is
`/routines/0001.cfg` holding `title=` and `routine_name=` and no setting keys.

### 3. A page knows which saved routine it is

Today one word does two jobs. A page holds the key it registered under in its `routine_key` field
(`self.routine_key = "interval"`), and `routines.load(routine_key)` uses that same word as the
registration key its settings schema is found by and as the store document its values are read from.

Instead a page carries two things: the **definition** it extends, which supplies the settings schema
(`routines.settings(definition)`), and the **saved document**, which is the config's filename stem
and supplies the values (`store.get(stem, ...)`, `store.set(stem, ...)`) and the settings list it
opens. A committed setting is written to the saved routine's file, never to a definition's.

A definition pushed bare — what the harness and `--art` do, and what `routines.push("interval", ...)`
answers today — keeps the current behaviour: the definition name is also the document.

Check: two saved routines of `interval` (`0001`, `0002`) edited in turn hold separate values and
editing one never moves the other; a bare push of `interval` still reads and writes
`/routines/interval.cfg`.

### 4. The browser lists saved routines by title

Today `BrowserScene.activate()` in `/modules/ui.be` sets `self.routine_keys = routines.keys()` and
`rows()` returns `routines.rows()`, so the list is the set of definitions this build ships.

Instead the list is the set of configs in `/routines/`, re-read on every `activate()` so a config
written over `fs` while the panel is up appears the next time the browser opens. A row is
`[definitionIcon, title]`: the title from the config, the icon from `routines.icon(routine_name)`,
and `nil` when the definition is gone, which draws a text-only row.

OK opens the saved routine: resolve `routine_name`, build that definition's scene and hand it the
config's stem as its document. Forward opens the saved routine's settings: the definition's settings
list, bound to write the config's document.

Check: the browser's rows match `/routines/*.cfg`, one per config, titled from each; a config naming
an unknown definition is listed, refuses to open, and logs why; a config added with `fs write` is a
row the next time the browser is opened, with no reboot.

### 5. The browser can create a saved routine

Today nothing creates a config. The browser opens what the image shipped and nothing else.

Instead a create gesture starts a three-step flow: a definition picker (the registry's titles and
icons), then the title editor, then the new config is written and its settings opened. The config is
written to the lowest free stem with `title=` and `routine_name=` and no setting keys, so its
settings read as the definition's defaults until one is first edited. An empty saved-routine list
still offers create.

The two refusals are in the editor: an empty title is refused as `EMPTY TITLE`, and a title another
config already carries — compared case-insensitively — is refused as `TAKEN`. A refused OK changes
nothing and stays in the editor.

Check: create `Bedtime` from `interval`; the config is `/routines/0001.cfg`, the row appears as
`Bedtime` with `interval`'s icon, its settings start at the definition's defaults, and
`fs read /routines/0001.cfg` shows the two reserved keys and no setting keys until one is edited;
creating `bedtime` again is refused as `TAKEN`.

### 6. The title editor composes the label

Today no scene enters text; every value is chosen with Up/Down or edited by a step.

Instead the editor is a field of up to 16 slots drawn on one line, a run of characters followed by
empty slots, with the cursor inverted. Up/Down cycle the character at the cursor through `a`–`z`,
`A`–`Z`, `0`–`9`, space, `_`, `-` and empty; Forward moves right onto the append slot at most; Back
moves left and, from the first slot, leaves without writing; OK accepts; Cancel leaves without
writing from any slot. Clearing a character slot clears every slot after it, so the field never has
a gap, and an accepted title is trimmed at both ends.

Check: a driven gesture sequence produces the expected title in the harness, capitals and spaces
included; Back at the first slot returns to the picker with nothing written; the alphabet admits no
newline and no leading `=`, so the editor cannot produce a title that would corrupt a config.

## Consequences

- `docs/specs/ui/routine.md`: where a definition lives, the config, its title and `routine_name`,
  the generated filename, and how a page is told its document.
- `docs/specs/ui/bindings.md`: `routines.keys()` enumerates definitions; the saved-routine list and
  the reserved keys are a new surface; `store` addresses a config's filename stem.
- `docs/specs/ui/scenes.md`: the browser's rows change meaning, and the picker and title-entry scenes
  join the map.
- `docs/specs/berry-harness.md`: the load order gains `/modules/routines/`; the `store` row
  describes configs rather than one document per definition.
- `docs/specs/serial-protocol.md`: section 9's `fs` tree moves the `.be` files under
  `/modules/routines`, and the `/routines` example shows a config with both reserved keys.
- `tools/berry/tests/browser.test.be`: its premise — rows are `routines.rows()` — no longer holds.
  It becomes: definitions are enumerated by the registry, saved routines are listed by title with
  their definition's icon, and a config naming an unknown definition is listed but refuses to open.
- `tools/berry/driver.be` keeps driving `routines.keys()` — definitions still exist — so the smoke
  walk is unchanged, and a drive of a definition with a shipped config writes that config.

## Deferred

The parking lot holds what this plan deliberately leaves out: renaming and deleting a saved routine
from the panel, per-instance icons, a wire command to open a saved routine by name, and `--art`
previewing one.

## How it is known to hold

- `python scripts/arduino.py --task build` compiles both sketches.
- `python tools/berry/check.py --tests` passes, with `browser.test.be` rewritten to the new rows and
  a suite covering the title editor and the create flow.
- `fs list /routines` answers configs only; `fs list /modules/routines` answers the definitions.
- A fresh image's browser shows one row per shipped config, titled from each config, with its
  definition's icon.
- Two saved routines of one definition (`0001`, `0002`) keep separate values across edits and across
  a reboot.
- A config naming an unknown definition is listed, does not open, and logs why.
- A config added over `fs` is a row the next time the browser is opened.
- A bare `routines.push("interval", ...)` still drives and edits `/routines/interval.cfg`, so the
  harness and `--art` behave as they do today.
