# String editing on the panel

The design of a fifth settings kind, `text`, and the scene that edits it: a string held by a routine
or by the device, entered with the panel's six keys on a page that is 128 px wide and 52 px tall.
What it proposes is a set of UX conventions — a text row over a rule, a key block of three rows, a
caret, and what each key means on each row — that the next editor on this panel could copy rather
than invent again.

## What is wrong now, and where

Every value editor is an `EditorScene` subclass in `libraries/Gardener/src/panel/settings/` —
`BoolScene`, `NumberScene`, `DurationScene`, `ChoiceScene`, `PressureScene` — and every one is built
by a row in `SettingsRows.h/.cpp`. `EditorScene` owns the page: a title band, a contents band and a
hint band, Up/Down to `nudge()`, OK through the row's curried validator to a pop, Cancel to a pop
that puts back what the kind opened on.

The panel's grammar, as the pages use it today:

| key | meaning |
| --- | --- |
| Up, Down | step the value under the keys |
| Forward, Back | the page's own work: next field, open, another page |
| OK | save, through the declaration's check |
| Cancel | put the value back and leave |

and the hint band says it in the tree's words: `OK=SAVE`, `NEXT`.

**No kind holds a string.** `SettingKind` and `SettingDeclaration` are kind-agnostic, the Berry
module offers `number`, `pressure`, `duration` and `choice`, and nothing in the tree stores a
free-text value. So this is a new row, a new scene, a new declaration on the Berry side, and a new
kind word: `text`.

**One timing fact shapes the keys.** `Buttons` holds a short press for a 350 ms double window and
reports two presses inside it as one `:double`. A scene that steps on a tap must answer `:double`
(and every existing editor does not, which is why a quick Up-Up is dropped today). A text editor
lives on repeated presses, so its conventions may not put anything destructive on `:double` — and a
double on the keys is a key typed twice, not a commit.

## Does it fit? The arithmetic

The glass is 128 x 64, but a page is **128 x 52**: `Routines::begin` sets a 12-row overlay across
the bottom (`Overlay::Height`) and `Ux` hands the page the rows above it. On 52 rows the editor's
bands are:

| band | share | rows |
| --- | --- | --- |
| title | 0.15 | 8 |
| contents | 0.70 | 36 |
| hint | 0.15 | 8 |

What the faces give in that band:

| face | advance | line box | columns in 128 px | lines in 36 px |
| --- | --- | --- | --- | --- |
| Adafruit 5x7, fixed | 6 (5 ink + 1 gap) | 8 (ascent 7 + descent 1) | **21** | 4 |
| X11 misc 5x7 | 5 | 7 | 25 | 5 |
| ttyp0 11, the composition's design | 6 | 10 | 21 | 3 |

The alphabet asked for — `a-z`, `A-Z`, `0-9`, space, `-`, `_`, `.` — is 66 characters. The key block
is **17 columns of 6 px** — 102 px, inset 13 px each side — in three rows:

| row | columns | keys |
| --- | --- | --- |
| 1 | 0–12, then 14–16 | `a`–`m`, then `7 8 9` |
| 2 | 0–12, then 14–16 | `n`–`z`, then `4 5 6` |
| 3 | 0–3 | `↑`, `-`, `.`, `_` |
| 3 | 4–9 | `space`, six columns wide |
| 3 | 10–12 | `DEL`, three columns wide |
| 3 | 13–16 | `0 1 2 3` |

Shift folds the case into the two letter rows, column 13 is the gutter that separates the letters
from the keypad, and the keypad's zero sits in it, to the left of the one.

Three key rows cost 24 of the contents band's 36 px; the text row costs 8 and the rule between them
3, which is 35 of the 36:

| rows | what |
| --- | --- |
| 0–7 | the title band: the label and the count |
| 8–15 | the text row, 21 columns across the full width |
| 16–18 | the rule: a paper row, an ink row, a paper row |
| 19–42 | the key block, three rows of 8 |
| 43 | spare |
| 44–51 | the hint band |

**The cap's ceiling is 24.** The text row shows 21 characters at a time, so a longer value is seen
through a window that follows the caret; 24 is the longest a setting may hold, which is a product
limit and not this page's — the window would carry any length. A row asks for 16 unless it asks for
something else.

**Without shift it would not fit.** The two letter rows would become four, 26 letters a row of 13,
which is one row more than the page has. The face carries the case in its own glyphs, so the shift
key is what buys the two rows of letters.

## The conventions

1. **The page is three parts.** A text row across the full width, a rule, and a key block inset from
   both edges; every cell 6 x 8 px at the Adafruit face. The text and the keys are separate lattices,
   and the rule says so: the line above is the record, the block below is the keys. The cell comes
   from the face's own fields (`width + 1`, `lineHeight`), the text row's columns are the band's
   width over that cell, and the block is centred in the band. A bigger page therefore shows more of
   the value and a wider margin round the keys, rather than stretching either: the block is the
   alphabet's own 17 columns wherever it lands.

   **Every key is a box, not a share.** The key table gives each key its column, its row and how many
   columns it spans, and the scene turns that into an absolute `gfx::Box` of the block's origin plus
   column x 6, row x 8, span x 6 by 8. On a 128 px page the block starts at x = 13 and the key rows
   at y = 19, 27 and 35, so `a` is (13, 19, 6, 8), the space is (37, 35, 36, 8) and `DEL` is
   (73, 35, 18, 8). A proportional row of shares would round at every boundary and walk the keys
   off the lattice, and the key cursor has to be one key's own box in any case. The text row is
   placed the same way, a cell box per character, which is what makes the caret's inversion exact.
   The whole of it is written out, field by field and key by key, in the table at the end of this
   document.
2. **The top row is the string, not a preview.** The text row is where the caret lives, so the value
   being edited is never hidden behind the keyboard and there is nothing separate to read. It shows
   21 columns; a longer value is a window that follows the caret, and the count in the title band
   says how long the value is.
3. **Exactly one cell is inverted, and it is what OK acts on.** In the text row the inverted cell is
   the caret; in the key rows it is the key cursor. While the cursor is in the keys the caret is
   drawn as a low underline, so it stays visible as the insertion point without competing.
4. **Up/Down change row; Forward/Back move along the row.** Rows wrap at the ends, as the settings
   list does, and Up from the text row goes to the first key row. In the text row Forward and Back
   are the caret, and the window follows it when the value runs past the 21 columns on show.
5. **OK acts on the row it is in.** In the key rows OK types the key under the cursor at the caret
   and steps the caret right; in the text row OK saves and closes through the declaration's check.
   A double is two quick presses of one button that the classifier has already merged, so in the key
   rows `ok:double` types the key **twice** — which is what a user typing quickly meant — and in the
   text row it saves, exactly as a single does. This keeps one commit path, the validator
   `EditorScene` already owns.
6. **OK held past its first second keeps the string, from anywhere.** `ok:2` and every whole second
   after it are the save, so a string is kept without walking the cursor back up to the text row. A
   hold is deliberate in a way a double is not, which is why the hold is the gesture that saves from
   the keys. The hint band says both: `OK=TYPE  HOLD=SAVE` while the cursor is in the keys,
   `OK=SAVE` in the text row.
7. **Typing is overtype, with an append slot.** A key replaces the character under the caret and
   steps right; with the caret past the last character it appends. The caret never goes past
   `min(length, cap - 1)`, so at the cap typing overwrites instead of refusing — the title band's
   `24/24` is what says why.
8. **`DEL` is the delete.** It removes the character under the caret and closes the gap, so the value
   is always a contiguous run and the row always shows exactly what will be stored. A space is a
   character like any other: `space` types it, it is kept inside a value, and only the ends trim on
   save — a space over a character is a space, not a deletion.
9. **Shift latches.** Pressing `↑` inverts it and the two letter rows redraw as `A`–`M` and `N`–`Z`;
   it stays until pressed again, so `ABC_v2` costs one shift rather than four. Latching, not one-shot,
   because the rows then show the case in force.
10. **The wide keys are worded**: `space` over six columns, `DEL` over three, and the case key bare
    `↑` (CP437 `0x18`). `DEL` is exactly three cells of the face — `D`, `E`, `L` — and a dot in a
    six-column space key would say nothing about what it types.
11. **The title band carries the label and the count**: `NAME` at the left, `5/24` at the right. The
    count makes the cap visible, which is what stops a user wondering why the last character is
    being replaced.
12. **The value is trimmed.** Leading and trailing spaces are dropped when the value is read, which
    is what `Config::parse` already does to every value it reads.
13. **Nothing is `--`.** A nullable row holding nothing prints `--` in the list and opens on an empty
    string with the caret at slot 0; an all-blank result answers no value, as an empty choice key
    does.
14. **Cancel puts back what it opened on**, by writing `opened_` back in `canceled()`, as every other
    kind does.
15. **The cap rides in the declaration's `max`.** `SettingDeclaration` already carries step, min and
    max; a text row puts `min = 0`, `max = cap` — 16 unless the row asks otherwise, and 24 at the
    ceiling, the longest value a setting may hold — so a page reading a routine's record sees the
    limit without a new record field. The bound is clamped to the ceiling. The alphabet is the row's
    own and is not in the declaration.
16. **The scene names its face.** The composition's design is ttyp0 — 10 px line, three lines in the
    band, no fixed advance — so the lattice names the fixed Adafruit face itself, exactly as the hint
    band already does for its marks.

## The page, drawn

```
 +----------------------------------------------------------+   128 x 52
 |NAME                                                  5/24|   title   8 px
 |hello_world                                               |   text    8 px
 |----------------------------------------------------------|   line    1 px 
 |      a b c d e f g h i j k l m   7 8 9                   |   keys    8 px
 |      n o p q r s t u v w x y z   4 5 6                   |   keys    8 px
 |    ↑ - . _ [space] [DEL]   [ 0 ] 1 2 3                   |   keys    8 px
 |----------------------------------------------------------|   line    1 px 
 |                 OK=TYPE HOLD=SAVE                        |   hint    8 px
 +----------------------------------------------------------+
```

The text row is 21 columns, and a longer value is seen through a window that follows the caret; the
rule occupies rows 16 to 18 with its ink in the middle one. The hint band prints `OK=TYPE HOLD=SAVE`
while the cursor is in the keys — eighteen of the band's twenty-one columns, so it fits without
naming the arrows the page's own grammar has already taught — and `OK=SAVE` alone in the text row.
A refusal from the declaration prints there in place of the words, which is the band's
existing job.

## The shape of the change

- **`panel/settings/TextScene.h/.cpp`** — the scene. State: the string it opened on, the string being
  edited, the caret index, the cap, nullable, the row and key the cursor is on, and the shift state.
  `handle()` intercepts the four arrows and the OK gestures: in the key rows a single types the key
  once and a double types it twice, while every save — `ok:single` and `ok:double` in the text row,
  and any `ok:<n>` from anywhere — is handed to `EditorScene::handle(Gestures::OkSingle)` so the
  validator and the pop stay in one place. `nudge()` is the caret step, which is what Forward/Back
  call in the text row; `value()`, `nullable()`, `canceled()` and `text()` follow the other kinds;
  `titleComponent()`, `pageContents()`, `hintText()` are the page. The alphabet, the three key rows,
  the rule, the six-column space and the three-column `DEL` are file-local constants, and the cell
  size comes from the face's own fields (`width + 1`, `lineHeight`) rather than from numbers written
  in the scene. Each key is a box the table places absolutely — column, row and span times the cell
  from the block's origin, never a share of the row — and the text row's cells are placed the same
  way. The window the text row shows is worked out as the caret moves, the way a list's visible
  window is (`windowOf` in `ChoiceScene`), so the caret is always on screen.
- **`panel/settings/SettingsRows.h/.cpp`** — `TextRow : SettingsRow`: `declaration()` names `text`
  and carries the cap — 16 by default, clamped to 24 — `stored()` writes the string, `parse()` trims
  and truncates to the cap, `accept()` takes a `String`, and `scene()` builds the `TextScene`.
- **`bindings/Settings.cpp`** — a fifth kind class, `settings.text(key, default, label, limit,
  nullable)`, added to the module's table. The record needs no new field, because the cap is `max`.
- **`tools/unit/cases/gfx_editors.cpp`** — the scene's cases; **`gfx_scenes.cpp`** — a settings list
  over a `TextRow` writing the string back under its key.
- **`tools/berry/`** — a routine that declares a text setting and a suite case that drives it.
- **`tools/walkthrough/`** — a `text.walk`, for the picture.

## The alternatives, and why not

- **A caret ring with a reel** — the string on one line with the caret, Up/Down spinning the
  character under it through the alphabet, the character drawn large between two triangles. Closest
  to the existing editors, and it needs no new key grammar. Lost because 66 characters in a ring is
  around thirteen presses a character and only one character is ever visible; the grid shows all of
  them and bounds the distance to a row.
- **Four letter rows and no shift** — 26 letters a row of 13 is four rows, which is one more than the
  page has; the digits and punctuation would be on top of that.
- **X11 5x7's 5 px cells** — 25 columns across instead of 21 would give the text row four more
  characters, but its cells have no gap between them and the face is not the panel's.
- **One lattice for the text and the keys** — a character directly above the key that types it.
  Lost because the key block's 13 + gutter + 3 does not divide 21, and the rule already says the two
  are separate things: a line of a record above, a keypad below.
- **The key block as shares of the row** — `gfx::row` and `gfx::column` with fractional cells, which
  is how the editor's bands and the choice rows are built. Lost because 13 + gutter + 3 and the 6-
  and 3-column keys are not fractions of 128, and each boundary rounds: the keys would drift off the
  lattice by a pixel or two, and the inverted cursor would no longer be exactly the key it marks.
- **The space as the blank** — no erase key at all, a space overwritten on a character to clear it.
  It costs three of the space's nine columns to have a real delete instead, and three columns is
  worth it: without a delete a value can only be shortened at its end, and a blank in the middle is
  stuck there as a space.
- **A one-shot shift, or a gesture for case** — fewer presses for one capital, more for
  `ABC_v2`, and neither shows the case in force as plainly as a latched cell does.
- **`:double` for delete, or a chord** — both are available and both were considered; a double is
  what a user pressing a key twice quickly produces without meaning to, and this page is nothing but
  repeated presses. `DEL` is a cell on the page, and spends no gesture at all.
- **OK-double saving from the key rows** — a double is exactly what a user typing quickly produces,
  because the classifier cannot know the cursor moved between the presses, so on the keys it types
  the key twice rather than committing. The save that reaches the keys is the hold.
- **A DONE cell, or a second commit key** — the string is kept by OK in the text row, by a double
  there, or by a hold from anywhere, so there is no second key to learn and none of the 21 columns
  spent on one. `OK=SAVE` is the tree's own wording, and the hold is the panel's own gesture — the
  reset and the force-quit are held chords already.

## The entry points it has to hold on

- **The settings list**: `SettingsScene::open` builds the row's scene and curries the row's check
  with the declaration and the set in force; the list writes what pops under the row's key. A
  cancelled editor must therefore return the value it opened on.
- **The document**: `stored()` and `parse()` go through `Config`, which trims every value it reads
  and splits on the first `=`; a value beginning with `;` would be read as a comment, which the
  alphabet excludes by having no `;`.
- **The Berry declaration**: the kind word in the record, and the cap in `max`.
- **The overlay**: the page is the 52 rows above the strip; nothing may be drawn in the last 12.
- **The repaint cadence**: the tree is about seventy leaves — 43 in the key block, 21 on the text
  line, the rule and the chrome — built on each draw. By the parking lot's measurement a 24-leaf
  choice page is ~99 us and 24 allocations, so this is roughly three times that, at four draws a
  second — worth knowing, not worth caching for.

## How it is known to have worked

- `python tools/unit/check.py` cases for: a key typing at the caret and appending past the end; the
  caret stopping at the cap and overwriting there; a value of 24 with the window following the caret
  to the last slot; `DEL` removing a character and closing the gap, and a space in the middle kept
  while the ends trim on save; the shifted rows answering the other case; the four arrows moving
  rows and caret; OK typing in the keys and a double there typing twice; OK single and double in
  the text row saving, and `ok:2` saving from a key row; Cancel restoring the opened string; a
  section's bound narrowing a copy's cap; a nullable row answering no value for an empty string; a
  refused value printing in the hint band and keeping the editor open; and the drawn numbers — 21
  columns of 6 px on the text row, 17 in the key
  block inset 13 px each side, the keys' own boxes at absolute positions (`a` at (13, 19), the space
  at (37, 35) and 36 px wide, `DEL` at (73, 35) and 18 px wide), the rule's ink in row 17, the
  inversion at the caret and at the key cursor, the title's `5/24`.
- `python tools/berry/check.py` once the declaration exists, with the new suite case.
- `python tools/walkthrough/walk.py tools/walkthrough/text.walk` for the picture a human checks.

## Where this lands

The steps that get from here to the tree are `docs/backlogs/text-entry-scene.md` — ordered, one
commit each, deleted together with this plan when the last item lands. `docs/README.md` is not
touched: it does not list plans or backlogs.

## The layout, field by field and key by key

Every number is a `gfx::Box` in the page's own coordinates, on a 128 x 52 page — the glass less the
12-row overlay. The cell is the Adafruit face's 6 px advance by its 8 px line box; the bands are the
editor's 8 / 36 / 8; the text row's 21 columns are centred with a pixel at each edge; and the key
block's 17 columns are centred with 13 px at each edge.

**The fields and the lines.**

| field            | x  |  y |  w  | h  | what |
| ---------------- | -- | -- | --- | -- | --- |
| page             | 00 | 00 | 128 | 52 | the glass above the strip |
| title band       | 00 | 00 | 128 | 08 | 0.15 of 52, rounded |
| title label      | 00 | 00 | 098 | 08 | the row's `NAME`, at the left |
| title count      | 98 | 00 | 030 | 08 | `24/24` is 30 px, at the right |
| contents band    | 00 | 08 | 128 | 36 | 0.70 of 52 |
| text row         | 01 | 08 | 126 | 08 | 21 cells of 6 px, centred |
| rule, above      | 00 | 16 | 128 | 01 | paper |
| rule, ink        | 00 | 17 | 128 | 01 | the line itself |
| rule, below      | 00 | 18 | 128 | 01 | paper |
| key row 1        | 13 | 19 | 102 | 08 | 17 columns, centred |
| key row 2        | 13 | 27 | 102 | 08 | |
| key row 3        | 13 | 35 | 102 | 08 | |
| spare            | 00 | 43 | 128 | 01 | the band's last pixel |
| hint band        | 00 | 44 | 128 | 08 | 0.15 of 52 |
| hint words, keys | 10 | 44 | 108 | 08 | `OK=TYPE HOLD=SAVE`, centred |
| hint words, text | 43 | 44 | 042 | 08 | `OK=SAVE`, centred |

The rule is `gfx::Line::horizontal(0, 17, 128, gfx::Ink)`; the two paper rows are the band's own
paper and are named here only so the boxes add up. A text cell is `(1 + 6k, 8, 6, 8)` for screen
column `k`, and the row shows slots `first` to `first + 20`, where `first` is `0` until the caret
passes column 20 and `caret - 20` after that. The count is right-aligned in its 30 px and the label
left-aligned in its 98, so the two never meet.

**The keys.** Every key is 8 px tall, and its box is the one the inverted cursor paints.

| row | key | x | y | w | what it does |
| --- | --- | --- | --- | --- | --- |
| 1 | `a` | 13 | 19 | 6 | types `a`, or `A` with shift |
| 1 | `b` | 19 | 19 | 6 | types `b`, or `B` with shift |
| 1 | `c` | 25 | 19 | 6 | types `c`, or `C` with shift |
| 1 | `d` | 31 | 19 | 6 | types `d`, or `D` with shift |
| 1 | `e` | 37 | 19 | 6 | types `e`, or `E` with shift |
| 1 | `f` | 43 | 19 | 6 | types `f`, or `F` with shift |
| 1 | `g` | 49 | 19 | 6 | types `g`, or `G` with shift |
| 1 | `h` | 55 | 19 | 6 | types `h`, or `H` with shift |
| 1 | `i` | 61 | 19 | 6 | types `i`, or `I` with shift |
| 1 | `j` | 67 | 19 | 6 | types `j`, or `J` with shift |
| 1 | `k` | 73 | 19 | 6 | types `k`, or `K` with shift |
| 1 | `l` | 79 | 19 | 6 | types `l`, or `L` with shift |
| 1 | `m` | 85 | 19 | 6 | types `m`, or `M` with shift |
| 1 | `7` | 97 | 19 | 6 | types `7` |
| 1 | `8` | 103 | 19 | 6 | types `8` |
| 1 | `9` | 109 | 19 | 6 | types `9` |
| 2 | `n` | 13 | 27 | 6 | types `n`, or `N` with shift |
| 2 | `o` | 19 | 27 | 6 | types `o`, or `O` with shift |
| 2 | `p` | 25 | 27 | 6 | types `p`, or `P` with shift |
| 2 | `q` | 31 | 27 | 6 | types `q`, or `Q` with shift |
| 2 | `r` | 37 | 27 | 6 | types `r`, or `R` with shift |
| 2 | `s` | 43 | 27 | 6 | types `s`, or `S` with shift |
| 2 | `t` | 49 | 27 | 6 | types `t`, or `T` with shift |
| 2 | `u` | 55 | 27 | 6 | types `u`, or `U` with shift |
| 2 | `v` | 61 | 27 | 6 | types `v`, or `V` with shift |
| 2 | `w` | 67 | 27 | 6 | types `w`, or `W` with shift |
| 2 | `x` | 73 | 27 | 6 | types `x`, or `X` with shift |
| 2 | `y` | 79 | 27 | 6 | types `y`, or `Y` with shift |
| 2 | `z` | 85 | 27 | 6 | types `z`, or `Z` with shift |
| 2 | `4` | 97 | 27 | 6 | types `4` |
| 2 | `5` | 103 | 27 | 6 | types `5` |
| 2 | `6` | 109 | 27 | 6 | types `6` |
| 3 | `↑` | 13 | 35 | 6 | latches the case |
| 3 | `-` | 19 | 35 | 6 | types `-` |
| 3 | `.` | 25 | 35 | 6 | types `.` |
| 3 | `_` | 31 | 35 | 6 | types `_` |
| 3 | `space` | 37 | 35 | 36 | types a space |
| 3 | `DEL` | 73 | 35 | 18 | removes the character at the caret |
| 3 | `0` | 91 | 35 | 6 | types `0` |
| 3 | `1` | 97 | 35 | 6 | types `1` |
| 3 | `2` | 103 | 35 | 6 | types `2` |
| 3 | `3` | 109 | 35 | 6 | types `3` |

Column 13 — x 91 — is empty in rows 1 and 2 and holds the zero in row 3, which is what puts the zero
to the left of the one where a keypad puts it. The block runs from x 13 to x 114 with 13 px of margin
either side, and no two keys overlap: 42 of them, the last ending where the margin begins.
