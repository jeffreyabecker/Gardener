# Text entry scene

The steps of [the text entry plan](../plans/text-entry-scene.md), in the order they land. Each item
is one branch, one commit and one diff, and leaves the tree building with the suite green. Branches
are `backlog/text-entry-scene/main`, `backlog/text-entry-scene/epic-<n>/main` and
`backlog/text-entry-scene/epic-<n>/task-<m>`, merged back with `--no-ff` as each item lands.

The numbers every item is checked against are the two tables at the end of the plan, field by field
and key by key: the cell is 6 x 8 px, the text row's cells are `(1 + 6k, 8, 6, 8)`, the rule's ink
is row 17, the key block starts at x 13 and its rows at y 19, 27 and 35.

## Epic 1 — the scene

- [ ] **Draw the page and its rule.** `panel/settings/TextScene.h/.cpp` with the bands the editor
      gives, the text row as 21 cells of 6 x 8 px at `(1 + 6k, 8)`, the rule's ink line at
      `(0, 17, 128, 1)` between two paper rows, the title band's label at the left and its `5/24`
      count at the right in a box of their own, and the hint band centred. Cases assert those boxes
      and the caret's inversion at `(1 + 6 * k, 8, 6, 8)`.
- [ ] **Move the caret.** Forward and Back step the caret over the string and out to the append slot,
      bounded by the cap at `min(length, cap - 1)`; past column 20 the row shows slots `first` to
      `first + 20` with `first` at `caret - 20`, so the caret is always on screen. Cases walk past
      both ends and to slot 23 of a 24.
- [ ] **Put the key block on the page.** A table of 42 keys, each with its row, its column and the
      columns it spans, turned into an absolute box of the block's origin plus the cell — never a
      share of the row. `a`–`m` and `n`–`z` down the two letter rows, the keypad's `7 8 9`, `4 5 6`,
      `0 1 2 3` in columns 13 to 16, and the third row's `↑`, `-`, `.`, `_`, the six-column `space`
      and the three-column `DEL`. The key cursor starts on `a`, Up and Down move between the text row
      and the key rows, and Forward and Back move along a row and wrap. Cases assert `a` at
      (13, 19, 6, 8), the space at (37, 35, 36, 8), `DEL` at (73, 35, 18, 8) and that a row wraps.
- [ ] **Type.** `ok:single` in the key rows overtypes at the caret and advances, appends past the
      last character, and overwrites at the cap rather than refusing; `ok:double` types the key
      twice; shift latches and the two letter rows answer `A`–`M` and `N`–`Z`. Cases cover each.
- [ ] **Delete and save.** `DEL` removes the character under the caret and closes the gap, and the
      ends of the value trim on save; `ok:single` and `ok:double` in the text row and any `ok:<n>`
      from anywhere save through the declaration's check; the hint band reads `OK=TYPE HOLD=SAVE` in
      the keys and `OK=SAVE` in the text row, and a refusal takes its place; Cancel answers the
      string it opened on. Cases cover each.

## Epic 2 — the setting

- [ ] **Add `TextRow`.** `SettingsRows.h/.cpp`: the kind word `text`, the cap in `max` — 16 unless
      the row asks otherwise, clamped to 24 — `limit()` narrowing that cap from a section's bound,
      the string printed and written, `parse()` trimming and truncating to the cap, and the scene
      built over the row's rules. A list over the row in `tools/unit/cases/gfx_scenes.cpp` writes the
      string back under its key, and a section's bound narrows a copy's cap.
- [ ] **Declare one from the image.** `bindings/Settings.cpp` gains `settings.text(key, default,
      label, limit, nullable)` beside the four kinds, and the record answers `text` with the cap in
      `max`.
- [ ] **Drive it on the host.** A routine under `firmware/data` that declares a text setting, and a
      `tools/berry/tests` case that opens it, types a name and saves, run with
      `python tools/berry/check.py`.

## Epic 3 — the picture

- [ ] **Walk it.** `tools/walkthrough/text.walk` opens a text row, types a name, shifts, corrects
      with `DEL` and saves; a second walk cancels.

When the last item lands, delete this file and the plan it breaks down.
