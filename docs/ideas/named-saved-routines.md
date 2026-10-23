# Named saved routines

The routine browser today lists one row per routine **definition** — each `/routines/*.be` file
that registers. A routine's settings live in a file named after the definition
(`/routines/<name>.cfg`), so there is exactly one saved set of values per routine, and keeping a
second configuration of the same routine means shipping a second copy of its `.be`.

This plan splits the two things. A **definition** is a routine's code: its scene, its icon and its
settings schema. A **saved routine** is one named configuration of a definition: a file holding
which definition it is and the settings that instantiation uses. The browser lists saved routines;
the definitions move out of the browser's list and become what a saved routine is made from.

## What has to be true

1. The browser lists saved routines, one row per config in `/routines/`. Each row shows the saved
   routine's name and its definition's icon.
2. A saved routine is a `key=value` file at `/routines/<name>.cfg` with two reserved keys —
   `name=` (its plain-word identity, which is also its browser label and its store name) and
   `routine_name=` (the definition instantiated when it is opened) — plus that routine's settings.
3. The routine definitions live in `/modules/routines/`, load with the modules, and stay enumerated
   in the `routines` registry, so a saved routine can name one and the harness and `--art` can still
   drive one by name.
4. A routine page knows which saved routine it is: it reads and writes its settings under the saved
   routine's name, while its settings schema still comes from its definition.
5. The browser can create a saved routine on the panel: pick a definition, name it, and land in its
   settings. Renaming and deleting are not part of this plan.

## The two kinds of routine

| | Definition | Saved routine |
| --- | --- | --- |
| What it is | the routine's scene class, icon and settings scene | one named set of settings for a definition |
| Where it lives | `/modules/routines/*.be`, registered in `routines` | `/routines/*.cfg` |
| Name | the word it registers under (`interval`) | the word the user gives it (`bedtime`), in `name=` |
| Enumerated by | `routines.names()` | the browser |
| Settings | the schema (`settings()`) | the values, in the same file |

A definition with no saved routine is not a browser row; it exists to be instantiated — from the
browser's create flow, or by the harness. A saved routine with no matching definition is a row that
cannot open.

## The config

Every `/routines/*.cfg` is a saved routine:

```
name=bedtime
routine_name=interval
set_count=6
target_pressure=8.0
work_duration=600
```

`name` and `routine_name` are reserved. `name` is the saved routine's identity: the word the
browser shows, the document `store.get`/`store.set` address, and the filename
(`/routines/<name>.cfg`). It is a plain word with no `/`, `\` or space, as today, so it is one
`fs` token and one filename. `routine_name` names a definition the registry knows. The remaining
keys are the definition's settings, read and written exactly as today's routine files are.

Because `name` is also the filename, the three shipped configs keep their filenames and gain the
two reserved keys:

```
name=interval
routine_name=interval
set_count=4
…
```

A config whose `name` key is missing or not a plain word falls back to its filename for the label.
A config whose `routine_name` is missing, or names a definition that is not registered, is listed
as a text-only row and refuses to open, logging why. A config that cannot be parsed at all is
skipped, with a log line, rather than breaking the browser.

## The registry

`routines.register(name, icon, factory, settings)` and `routines.names()`, `icon()`, `settings()`,
`push()` and `push_settings()` keep their meaning, but now describe definitions only. The registry
remains the enumeration a `routine_name` resolves against, and its rows remain what the create
flow's definition picker draws.

The saved-routine list is a new surface beside it. The browser needs every `/routines/*.cfg`
identity in order, and each one's `name` and `routine_name`. This is data, not Berry code, so it is
read through the settings source and the file library rather than a registry. It lists only
plain-word config names, and it never lets a config name touch `/settings.cfg`, which stays the
device's own and reserved.

## The browser

`BrowserScene` keeps its shape — a `List`, Up/Down to move, OK to open, Forward to edit, Back/Cancel
to leave — but its rows come from the saved-routine list instead of `routines.names()`:

- **A row** is `[definitionIcon, name]`. The icon comes from the definition (`routine_name` →
  `routines.icon()`); a saved routine whose definition is gone is a text-only row.
- **OK** opens the saved routine: resolve `routine_name`, build that definition's scene, and hand it
  the saved name as its store identity.
- **Forward** opens the saved routine's settings: the definition's settings scene, bound to write
  the saved routine's file — the definition supplies the schema, the saved name supplies the
  document.
- **Create** starts a flow: a definition picker, then a name entry, then the new config is written
  and its settings opened.

The browser reads the saved-routine list on every `activate()`, as it reads `routines.names()`
today, so a config written over `fs` while the panel is up appears the next time the browser is
opened.

### Creating a saved routine

Three steps, one gesture each:

1. **Pick the definition** — a `List` of `routines.names()` with icons, like the browser's own rows.
   OK selects, Back returns.
2. **Name it** — the name editor below: compose a plain word from the characters a valid name
   allows, then OK accepts.
3. **Land in settings** — write `/routines/<name>.cfg` with `name=` and `routine_name=` (the
   settings keys are absent, so they read as the definition's defaults until first edited), then
   open that definition's settings scene bound to the new name. Closing the settings returns to the
   browser, which now shows the new row.

An empty saved-routine list still offers create: the browser draws its hint, and OK on the empty
list starts the flow.

### The name editor

The name entry is the one genuinely new scene, and its job is to compose a plain word with six keys.
It opens with an empty field, after the definition picker's OK.

**The field.** Up to 16 slots, left to right. Each slot is one character or empty; an empty slot is
drawn as an underscore. The field is drawn on one line at a size that keeps all 16 slots on the
glass, and the current slot is inverted. The editor never lets a gap open between characters: the
field is always a run of characters followed by empty slots.

**The cursor.** It starts at the first slot and is always on a character of the name, or on the first
empty slot — the append slot, where the next character will go.

**The keys.**

| Key | What it does |
| --- | --- |
| `up:single` / `down:single` | cycle the character at the cursor through `a…z`, `0…9`, `_`, `-` and empty, wrapping. At the append slot, Up enters `a`; Down at empty does nothing. Making a character slot empty clears it and every slot after it. |
| `forward:single` | move the cursor one slot right, onto the append slot at most. |
| `back:single` | move the cursor one slot left; at the first slot, leave the editor without writing and return to the definition picker. |
| `ok:single` | accept the name. |
| `cancel:single` | leave the editor without writing, from any slot, back to the picker. |

**Accepting.** OK commits the name as typed. An empty name is refused and the hint line shows
`EMPTY NAME`; a name whose `/routines/<name>.cfg` already exists is refused with `TAKEN`. The reason
stays on the hint line until the next press, the way the settings list draws its rule, and a refused
OK changes nothing and stays in the editor.

**Reading the screen.** The title is `NAME`, the field is the main line, and the hint line at the
bottom carries the stage or the refusal reason. Two hints cover it — `CH UP/DOWN  FWD=NEXT` while the
cursor is on a character, `CH UP/DOWN  OK=SAVE` at the append slot. The wording is the implementer's;
the shape is the shared value editor's.

**Why 16 and this alphabet.** 16 slots is the longest name that fits a browser row beside the widest
icon (12 px) at the row's face, and it is longer than any shipped name (`ramped_interval` is 15). The
alphabet is every character a plain word may contain — no `/`, `\` or space — so whatever the editor
produces is already a valid store name and one `fs` token. The editor is the only place a bad name
could be made, and it cannot make one.

## A routine knows which settings it owns

Today a page's name does three jobs: registry key, settings schema, and store document. The split
makes those the definition's and the saved routine's:

- `Page` keeps the **definition** it extends — the hard-coded word its `init()` passes today — for
  its settings schema, and gains the **saved routine's name** as the document its settings read and
  write.
- `Activation` reads each value from the saved routine's document while the `setting` list still
  comes from the definition: `routines.settings(definition)` for the schema,
  `store.get(savedName, …)` for the values.
- `open_settings()` opens the definition's settings scene with a writer that stores to the saved
  name; a committed value is written to the saved routine's file, never the definition's.

A definition pushed bare — what the harness and `--art` do — keeps today's behaviour: the definition
name is also the document, so the drive and the previews are unchanged. A saved routine opened from
the browser carries no settings map; its file is the source, exactly as a routine's file is today.

## Expected UX fallout

### On the panel

- **The browser's list changes meaning.** Today it is "the routines this build ships". After, it is
  "the routines you have saved". A definition with no saved routine disappears from the browser and
  is only reachable through Create. The stock image ships one saved routine per definition, so a
  fresh panel looks the same.
- **`up:double` and `forward:single` now edit the saved routine, not the definition.** Two saved
  routines of one definition — `bedtime` and `workout`, both `interval` — each keep their own
  values; editing one never moves the other.
- **The browser's Forward row is corrected along the way.** It must resolve the definition's
  settings scene; today it pushes the shared base `SettingsScene`, which is an empty list. The
  refactor is the natural place to fix that.
- **Naming is a new kind of editing.** No scene today enters text, so the name editor is the one
  genuinely new surface and the main UX cost: the panel's six keys now compose a word. Its model is
  spelled out in [The name editor](#the-name-editor); it stays inside the gestures of
  [gestures.md](../specs/gestures.md) — OK commits, Back leaves without writing, Up/Down change.
- **Names are plain words, not free text.** A saved routine cannot be called `Morning pull`; it is
  `morning_pull` or similar. That is the price of keeping `name` the filename and the store
  identity, and it is accepted.
- **Duplicate names are refused at entry**, because two configs cannot share a filename.
- **A config whose definition is missing is visible but inert.** It shows as a text-only row; OK
  and Forward log why nothing opens. This is the fate of any hand-written `/routines/*.cfg` that
  predates `routine_name=`, and of a saved routine whose `.be` was later removed.
- **Settings are written on the spot.** Creating writes the file immediately, and the first
  committed setting in the flow writes the first value; nothing is staged. Cancel inside the
  settings list still keeps the file, because the settings scene only reports values and the
  browser writes them — as today.

### On the wire

- `/routines` now holds only configs; the definitions appear under `/modules/routines`. `fs list`
  shows both, and `/modules/routines/<def>.be` is a valid `fs` path. Writing under either does not
  reload code until the next boot, unchanged.
- A new config written over `fs` — a file with `name=` and `routine_name=` — becomes a browser row
  the next time the browser is opened, because the browser re-reads its list on entry; no reboot is
  needed for data.
- [serial-protocol.md](../specs/serial-protocol.md) section 9's tree line and `/routines` example
  move the `.be` files to `/modules/routines`.

### For the harness and tests

- `driver.be` drives `routines.names()` — definitions — which still exist, so the smoke walk keeps
  driving each definition's scene and settings. Its settings writes land in
  `/routines/<definition>.cfg`; the shipped configs keep those filenames, so the writes land in real
  configs and preserve their reserved keys. A definition without a same-named config would see the
  drive create an orphan config in the host image; the browser must tolerate it.
- `browser.test.be`'s premise — the browser's rows are `routines.rows()` — is no longer true. It
  moves to: definitions are listed, saved routines are listed with their definition's icon, and a
  config naming an unknown definition is listed but refuses to open.
- `--art` continues to draw a definition by name. Previewing a saved routine by name is a small
  extension this plan does not require.
- The harness's `store` table gains a way to enumerate configs and read `name` and `routine_name`;
  the host image already loads `/modules/routines/*.be` through the same `Routines::begin`, so no
  extra host path is needed beyond the new load folder.

### Specs that change

- [routine.md](../specs/ui/routine.md): where a definition lives, the config and its two reserved
  keys, and how a page is told its saved name.
- [bindings.md](../specs/ui/bindings.md): `routines.names()` enumerates definitions, and the
  saved-routine surface (list plus reserved keys) is new.
- [scenes.md](../specs/ui/scenes.md): the browser's rows, and the create/definition-picker/name-entry
  scenes.
- [berry-harness.md](../specs/berry-harness.md): the load order gains `/modules/routines/`; the
  `store` row describes configs.
- [serial-protocol.md](../specs/serial-protocol.md): the `fs` tree and example.

## Deferred

- **Rename, delete, duplicate.** Create only. A saved routine is renamed or removed by hand through
  `fs` or by shipping a new image, and the browser tolerates the result. Nothing on the panel edits
  a name after creation.
- **Free-text labels.** `name` stays a plain word. A display label separate from the filename is a
  second identity the store would have to keep in step; not now.
- **Per-instance icons.** The icon always comes from the definition.
- **A wire command to open a saved routine.** None exists today and none is added; the browser is
  the only caller.
