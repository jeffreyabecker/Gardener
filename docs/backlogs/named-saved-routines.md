# Backlog — named saved routines

The tasklist for [the named saved routines plan](../plans/named-saved-routines.md): a definition is
a routine's code, and a saved routine is one titled configuration of it in a file the device names.
Each item is numbered `epic.item`, so every one has a stable name for its branch and its commit. An
item names the code that is wrong now, what is true when it lands, and the check that shows it.

The epics are worked in order and every item leaves the tree building. The spec line a behaviour
changes is written in the commit that changes it, so Epic 6 is the read-back rather than the only
place the contract is written.

## Epic 1 — The definition moves

- [ ] 1.1 Load `/modules/routines/` as a fourth folder, after `/modules/*.be`. Today
  `Routines::begin` in `routines/Routines.cpp` calls `loadSystem` and then `loadAll` for `"modules"`
  and `"routines"`, and `Library::list` in `storage/Library.h` is documented as taking one of the
  interpreter's folders. Load the fourth folder after the modules, because every definition calls
  `routines` and extends `Page`, and name it in the interface's contract. Both stores already list
  any folder path, so this is the load call and that comment. Check: the boot summary reports the
  same registered count as today, and `--tests` passes with an empty `/modules/routines/`.

- [ ] 1.2 Move the three definitions into the data image's `/modules/routines/`. Today
  `firmware/gardener/data/routines/` holds `interval.be`, `ramped_interval.be` and
  `ramped_static.be` beside their `.cfg` files. Move the `.be` files and leave the configs. Check:
  `fs list /routines` answers only `.cfg`; the browser still shows three rows; `--tests` passes,
  because the harness loads this same image.

## Epic 2 — The config's title

- [ ] 2.1 Give the three shipped configs their reserved keys. Today
  `firmware/gardener/data/routines/*.cfg` hold settings only. Add `title=` (each definition's own
  title) and `routine_name=` to each, keeping the filenames. Check: `fs read
  /routines/interval.cfg` shows `title=Interval` and `routine_name=interval` above the settings, and
  a fresh image's browser is unchanged.

- [ ] 2.2 Let Berry enumerate the configs. Today `/system/registry.be` enumerates definitions and
  nothing can list `/routines/*.cfg` or read a config's title and definition. Add a surface that
  returns every config's filename stem, title and definition, skipping a file that cannot be parsed
  and logging it, reporting a missing title as the stem, and refusing a config whose stem is not a
  plain word. Check: a harness assertion lists the three shipped configs with their titles and
  definitions; a config written with `fs write` appears; a malformed one is skipped with a log line;
  a config with no `title` lists under its stem.

## Epic 3 — A page knows which saved routine it is

- [ ] 3.1 Split the page's key into a definition and a saved document. Today a page holds one word
  in `self.routine_key`, and `routines.load(routine_key)` uses it both as the registration key for
  `routines.settings(routine_key)` and as the store document for `item.read(routine_key)`. Carry the
  config's filename stem as the document and keep the definition for the schema, so two configs of
  one definition read and write their own files. A definition pushed bare keeps its own key as the
  document, so the harness and `--art` are unchanged. Check: a bare push of `interval` still edits
  `/routines/interval.cfg`; a push carrying `0001` against `interval`'s schema edits
  `/routines/0001.cfg`.

## Epic 4 — The browser lists saved routines

- [ ] 4.1 Draw the rows from the configs. Today `BrowserScene.activate()` in `/modules/ui.be` sets
  `self.routine_keys = routines.keys()` and `rows()` returns `routines.rows()`. Read the config list
  on every `activate()`, draw `[definition icon, title]`, and draw a text-only row when the
  definition is gone. Check: there is one row per config, titled from each; a config naming an
  unknown definition is listed; a config written while the panel is up appears the next time the
  browser is opened.

- [ ] 4.2 Open a row through its definition. Today `choose()` pushes
  `routines.push(self.routine_keys[selected], nil)`. Resolve the row's definition, push that
  definition's scene and hand it the config's stem. Check: a run started from the row titled
  `Bedtime` reads `/routines/0001.cfg`, and one started from `Workout` reads `/routines/0002.cfg`.

- [ ] 4.3 Fix the Forward row. Today `open_selected()` pushes the shared base `SettingsEditScene`, which
  has an empty `settings()` list, so editing a routine's settings from the browser shows nothing.
  Push the definition's settings list with the config's stem as its writer, as the page's own
  `open_settings()` does. Check: Forward on a row lists that definition's settings and a committed
  value lands in the config's file.

## Epic 5 — Creating a saved routine

- [ ] 5.1 Add the title editor. Today no scene enters text. Add a scene that composes a label of up
  to 16 slots with Up/Down cycling `a`–`z`, `A`–`Z`, `0`–`9`, space, `_`, `-` and empty, Forward and
  Back moving the cursor, OK accepting, Cancel leaving, clearing every slot after a cleared one, and
  refusing an empty title as `EMPTY TITLE` and a title already in use as `TAKEN`, compared
  case-insensitively. Check: a driven gesture sequence in the harness produces `Bedtime pull`;
  Back at the first slot writes nothing; both refusals leave the editor open.

- [ ] 5.2 Add the definition picker. Today nothing lists definitions as something to choose. Add a
  list of the registry's titles and icons that returns the chosen definition and hands off to the
  title editor. Check: the picker lists every definition by title; Back returns to the browser.

- [ ] 5.3 Wire create into the browser. Today the browser has no create gesture and an empty list
  has nothing to draw. Add the gesture, take the lowest free stem from `0001` to `9999`, write
  `/routines/<stem>.cfg` with `title=` and `routine_name=` and no setting keys, and open its
  settings; offer create on an empty list. Check: creating `Bedtime` from `interval` writes
  `/routines/0001.cfg`, shows the row with `interval`'s icon, and reads the definition's defaults;
  creating a second routine writes `0002`; a gap left by a hand-deleted file is the next stem used.

## Epic 6 — The record

- [ ] 6.1 Update `docs/specs/ui/routine.md` for where a definition lives, the config's title and
  `routine_name`, the generated filename, and how a page is told its document. Check: the spec states
  the load folder, both keys and the stem rule, and names nothing that no longer exists.
- [ ] 6.2 Update `docs/specs/ui/bindings.md` for `routines.keys()` meaning definitions, the config
  list, and `store` addressing a filename stem. Check: each call in the tables matches what the code
  answers.
- [ ] 6.3 Update `docs/specs/ui/scenes.md` for the browser's rows and the picker and title-entry
  scenes. Check: every gesture the new scenes bind is in the map and follows the rules below it.
- [ ] 6.4 Update `docs/specs/berry-harness.md` for the load order and the `store` row. Check: the
  documented order is the order `check.py` loads.
- [ ] 6.5 Update `docs/specs/serial-protocol.md` section 9 for the `fs` tree and example. Check: the
  tree and the example match `fs list` on a device running the new image.
- [ ] 6.6 Rewrite `tools/berry/tests/browser.test.be` for the new rows, and add a suite for the title
  editor and the create flow. Check: `python tools/berry/check.py --tests` passes.
