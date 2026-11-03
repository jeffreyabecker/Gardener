# Text entry scene

The steps of [the text entry plan](../plans/text-entry-scene.md), in the order they land. Each item
is one branch, one commit and one diff, and leaves the tree building with the suite green. Branches
are `backlog/text-entry-scene/main`, `backlog/text-entry-scene/epic-<n>/main` and
`backlog/text-entry-scene/epic-<n>/task-<m>`, merged back with `--no-ff` as each item lands.

## Epic 1 — the scene

- [ ] **Draw the page.** `panel/settings/TextScene.h/.cpp` with the layout worked out from the band
      — the cell from the Adafruit face's own fields, the columns and rows from the band — the text
      line showing the opened string with the caret inverted at slot 0, the rule under it, and the
      title band's label and count. Cases assert 21 columns of 6 px on the text row, the rule's ink
      in row 17, the caret's inversion at slot k, and the title's `5/24`.
- [ ] **Move the caret.** Forward and Back step the caret over the string and out to the append slot,
      bounded by the cap at `min(length, cap - 1)`; a value longer than 21 columns is shown through a
      window that follows the caret. Cases walk past both ends and to slot 23 of a 24.
- [ ] **Put the key block on the page.** A table of keys, each with its column, its row and the
      columns it spans, turned into an absolute box of the block's origin plus the cell — never a
      share of the row. Three rows of 17 columns inset 13 px each side: `a`–`m` and `n`–`z` down the
      two letter rows, the keypad's `7 8 9`, `4 5 6`, `0 1 2 3` in columns 13 to 16, and the third
      row's `↑`, `-`, `.`, `_`, the six-column `space` and the three-column `DEL`. The key cursor
      starts on the first letter, Up and Down move between the text row and the rows, and Forward and
      Back move along a row and wrap. Cases assert the keys' boxes at absolute positions and that a
      row wraps.
- [ ] **Type.** `ok:single` in the key rows overtypes at the caret and advances, appends past the
      last character, and refuses past the cap; `ok:double` types the key twice; shift latches and
      the two letter rows answer `A`–`M` and `N`–`Z`. Cases cover each.
- [ ] **Delete and save.** `DEL` removes the character under the caret and closes the gap, and the
      ends of the value trim on save; `ok:single` and `ok:double` in the text row and any `ok:<n>`
      from anywhere save through the declaration's check; Cancel answers the string it opened on.
      Cases cover each, including a refusal printed in the hint band.

## Epic 2 — the setting

- [ ] **Add `TextRow`.** `SettingsRows.h/.cpp`: the kind word `text`, the cap in `max` — 16 unless
      the row asks otherwise, clamped to 24 — the string printed and written, `parse()` trimming and
      truncating to the cap, and the scene built over the row's rules. A list over the row in
      `tools/unit/cases/gfx_scenes.cpp` writes the string back under its key.
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
