# A face chosen by the height it has to fill

## What is wrong now, and where

A picture of text is pinned to the built-in 5x7 twice over: the face is one size for every caller,
and the size it can be is a whole-number multiplier of that one size.

**A face cannot say where its baseline is.** `Font` carries `width` and `height` for a fixed face and
`yAdvance` for a variable one, and `yAdvance` is read by nothing
([Font.h](../../libraries/Gfx/src/Font.h:56)). A variable face therefore says how far the next line
moves and not how many rows sit above the baseline and how many below. So
[Drawing.cpp](../../libraries/Gfx/src/layout/Drawing.cpp:17) finds a baseline by measuring the string
it is about to draw: `measureText(font, value, scale).height`. The baseline becomes a property of
the string. An `A` in `ttyp016` starts on the box's first row and `Ag` starts three rows lower,
because the ink of the pair is three rows taller than the ink of the letter; two text leaves sharing
one band do not share a baseline. And no caller can ask a face how tall it is without having a
string to measure.

**Three cuts and no family.** The three ttyp0 faces are registered as three unrelated names
([Fonts.cpp](../../libraries/Gfx/src/Fonts.cpp:78)), so nothing in the tree says they are one design
at three sizes and nothing can choose between them. A caller wanting "the smallest that fits" would
have to know all three and compare them itself.

**A page cannot know a height to choose by.** A box declares shares of the cell its layout gives it
and the share becomes pixels only as the node draws, in `Box::in`
([Paint.h](../../libraries/Gfx/src/layout/Paint.h:26)). A cell above 1 is a pixel count and a page
that declares one knows its height; the fractions every band in the tree declares are not a number
until the layout has run. Whatever chooses a size from a height has to run at the draw, where the
height exists.

**The only size lever multiplies.** `size` is a whole-number multiplier of a face's pixels
([Text.h](../../libraries/Gfx/src/shapes/Text.h:12)), so the built-in face gives an 8, 16 or 24 px
line and nothing between, on a 6, 12 or 18 px advance. The panel is 128x64
([PanelRoot.cpp](../../firmware/gardener/PanelRoot.cpp:15)); an editor page's title band is 10 px and
its contents band 45 px, and the home page's timer cell is 48 px by 64 px. Every page draws in the
default face, and the scenes that raise a scale raise it to 2
([NumberScene.cpp](../../libraries/Gardener/src/panel/page/NumberScene.cpp:16)), so a 45 px band is
spent as 16 px of text and the 10, 13 and 17 px lines the ttyp0 faces already cost flash for are
named by nothing.

## The shape of the change

Four pieces, each one question: what a face's line box is, what a family is, which cut fills a
height, and what a page writes to ask for it.

### The face says its line box

`Font` loses `yAdvance` and gains `ascent` and `descent`: the rows above the baseline and the rows on
and below it, for both kinds. `lineHeight(face)` is their sum, so a face's height no longer needs a
string, and it is the field the resolver fits. A fixed face fills them from its cell, `ascent` being
`height - 1` and `descent` 1, because the last row of a fixed cell sits on the baseline and a row on
the baseline is a descent row; its line is then the cell's eight rows, which is what `measureText`
already answers for it. A variable face takes the numbers from its own header, which
[from_u8g2.py](../../tools/fonts/from_u8g2.py:192) already reads and today only subtracts to get the
line advance.

`baseline()` becomes `ascent * scale` for both kinds, and `drawFixedGlyph` places its cell from
`ascent` rather than from `height - 1`, which is the same number said in the face's own words. A
glyph may reach past the line box — `g` does in every ttyp0 cut — and the draw clips it to the box as
it clips everything else.

The pictures this moves are the ones drawn in a variable face, which is nothing on the glass today
because no page names one. It moves the case that pins a ttyp0 `A`
([gfx_drawing.cpp](../../tools/unit/cases/gfx_drawing.cpp:329)), which reads rows from the box's top
and has been reading a baseline the face does not share.

### The family

```cpp
// The cuts of one design, shortest first: a page names the design and a height picks the cut.
struct Family {
  const char* name;
  const Font* const* cuts;
  uint8_t count;
};

const Family* findFamily(const char* name);
```

`Family` and `findFamily` go in [Fonts.h](../../libraries/Gfx/src/Fonts.h:1), with the names, beside
`find`. Two are shipped and both are static tables in the module: `ttyp0`, the cuts `ttyp011`,
`ttyp016` and `ttyp022` in that order, and `builtin`, the built-in face as a family of one cut.
`findFamily(nullptr)` and a name the module does not hold answer `builtin`, which is the answer
`find` gives an unknown name: a page that names a family nobody shipped draws as it does without
one rather than not drawing.

There is no `registerFamily`. Nothing but the module has a family to name. A host tool or a case
that wants a family of its own builds a `Family` value, and the resolver takes a reference to it.

### The resolver

A family and a height answer a face and a scale. Both questions go in
[shapes/Text.h](../../libraries/Gfx/src/shapes/Text.h:1) beside `measureText`, because the second one
measures; the family itself stays in `Fonts.h`.

```cpp
// A face and the scale that spends a height with it.
struct Fit {
  const Font* face;
  uint8_t scale;
};

// The tallest cut whose line box fits at scale 1, at the largest whole scale that fits: the design
// first, so a cut at one scale beats a shorter cut doubled, and then the height spent whole.
Fit resolve(const Family& family, int16_t height);

// The same answer narrowed to a width: the height's run of scales, then the next shorter cut's, down
// to the shortest. The first whose string fits the width is the answer; when none does, the first is
// drawn and the box clips it.
Fit resolve(const Family& family, const char* value, int16_t width, int16_t height);
```

The rule, said once: **a height is spent by the tallest cut it can hold at all, and then by as many
of that cut's lines as it holds.** The scale is `height / lineHeight(cut)`, floored and at least 1,
and a height shorter than every cut answers the shortest cut at scale 1. The tie that matters is a
cut at 2 against the cut above it at 1: 34 px is `ttyp022` doubled, not `ttyp016` tripled, because
the design is the picture and the scale is its coarsening.

The second form exists because a fitted string usually has a width too, and on this panel the width,
not the height, is what settles small cells: the home page's timer is `4m30s` in a 64 px cell, which
is 55 px of `ttyp022` and 110 px doubled, so the timer is answered at scale 1 inside a 48 px band.
The candidates the width walks are the height's run of scales for the chosen cut, then the same run
for each shorter cut. A page whose string does not fit the shortest cut at scale 1 gets the shortest
cut at scale 1 and the box's clip, which is a page holding the wrong string, and a size nobody asked
for is worse than a clipped one.

The cost is one division per cut to find its scale and one `measureText` per candidate walked, which
is three walks of the string in the usual case and never more than one per step of a cut's scale run.
The panel repaints every 250 ms.

| the box the panel gives it | what `ttyp0` answers | the line it spends |
| --- | --- | --- |
| a settings row, 8 px | `builtin` 1 | 8 |
| an editor's title band, 10 px | `ttyp011` 1 | 10 |
| the home page's title, 16 px | `ttyp016` 1 | 13 |
| the settings chrome, 20 px | `ttyp022` 1 | 17 |
| an editor's contents band, 45 px | `ttyp022` 2 | 34 |
| the home page's timer cell, 48 px by 64 px | `ttyp022` 1 | 17 |
| a full-height page, 64 px | `ttyp022` 3 | 51 |

The table also shows what the family cannot do: between 17 and 34 px it can only answer 17 px,
because a second copy of `ttyp022`'s pixels is not a size between the cuts. A band that needs to be
spent more fully needs a cut the family does not have, which is a face to convert rather than a rule
to add.

### The leaf a page writes

`fitted` is a leaf of the drawing vocabulary: a string, a family and the two justifications, drawn in
the box it declares at the cut and scale the box's height takes.

```cpp
PaintNodeRef fitted(Box box, const char* value, const Family* family = nullptr,
                    Justify horizontal = Justify::Center, Justify vertical = Justify::Center);
```

The string is borrowed, as `screen.text` borrows one: a page's strings belong to the interpreter and
outlive the draw. The leaf resolves as it draws, after `Box::in` has turned the declared sides into
pixels. The line box is placed by the vertical justification and the glyphs sit on its baseline, so
two fitted leaves in one row share a baseline, and the ink is placed across by the horizontal
justification, which is `alignedOffset` over the measured width. Place the line box rather than the
ink because the line box is the height that was fitted, and what is left of the box after it is the
margin on both sides of the same thing.

Berry reaches it as one call, and the record carries the family in a field of its own rather than
overloading `font`:

- `screen.fitted(x, y, w, h, value[, family[, horizontal[, vertical]]])`, which writes the `fitted`
  kind, `value`, `family` and the two justifications, both written as `center` when the page names
  neither;
- a `family` text field on the `draw` record, read the way `font` is read
  ([DrawTree.cpp](../../libraries/Gardener/src/bindings/DrawTree.cpp:106)), so a name that is a
  family and a name that is a face stay two questions: `find` answers a face, `findFamily` a family;
- the `fitted` branch of `paintOf`, which builds the leaf from the node's box, string, family and
  justifications.

`font` keeps its meaning and so does `scale`: a page that wants one cut of a design names it at a
scale, and a page that wants the design fitted names the family and no scale.

## Alternatives, and why not

- **Resolve in the page** — `screen.font_at("ttyp0", 45)`, one call with the page deciding. It works
  where a page declares a pixel height and nowhere else, and every band in the tree declares a share,
  so the page has no number to pass. It also chooses twice, once when the tree is built and once when
  it draws, which is the drift a size rule exists to stop.
- **Scale only, no cuts** — spend the height with the built-in face at the scale that fits. The face
  is already in flash, but the multiplier is the problem: 8, 16 and 24 px lines on a 6, 12 and 18 px
  advance, so a 45 px band is 24 px of 3x3 blocks while three cuts sit unread.
- **A fractional scale** — a size between the cuts and between the doubles. The renderer is an
  integer block writer over a 1-bit frame ([Text.cpp](../../libraries/Gfx/src/shapes/Text.cpp:44)); a
  fraction needs resampling and antialiasing, which a 1-bit frame cannot show and this library has no
  filter for.
- **A height in pixels on `text`** — `text(box, value, face, 34)`, the same face at a distance. It
  makes the record's `scale` mean a distance where it means a multiplier, and it still leaves the
  page choosing the number.
- **A `registerFamily` mirroring `registerFont`** — four names, sixteen cut slots and two refusals.
  No caller registers one, the module ships the only families a page can name, and a case that wants
  a family of its own has a `Family` value to hand the resolver.
- **The family in every page's record** — a list of cuts in a call, `[[8, "ttyp011"], [17,
  "ttyp016"]]`. It puts the table where it will be copied and missed, and a page that leaves a cut
  out changes the picture for a reason nobody can see in the page.
- **Resolve on the ink rather than the line box** — fit the rows the actual glyphs take. A string of
  `AA` would then be fitted to a taller cut than a string of `Ag`, and a page would change size as
  its number changed. The face's line box is what a band holds; the ink is what the draw measures.

## Entry points it has to hold on

- **The `Font` initializers** in
  [BuiltinFont.cpp](../../libraries/Gfx/src/fonts/BuiltinFont.cpp:1) and the three ttyp0 `.cpp` files,
  and the one [from_u8g2.py](../../tools/fonts/from_u8g2.py:192) writes. A face built by hand in a
  case ([gfx_drawing.cpp](../../tools/unit/cases/gfx_drawing.cpp:63)) needs the two numbers too.
- **The draw and the measure of a variable face**, which keep answering the same pixels. The baseline
  moves; the glyphs, the advances and the measured ink do not.
- **The walk and the module together**: a kind, its field on the record, its branch in `paintOf` and
  its call in `screen` land in one commit, or a page that names the kind draws nothing between them.
- **The scenes that place text by `measureText`** —
  [EditorScene](../../libraries/Gardener/src/panel/page/EditorScene.cpp:33),
  [PressureReadout](../../libraries/Gardener/src/panel/page/PressureReadout.cpp:34) and the editor
  bodies in `NumberScene`, `DurationScene`, `BoolScene` and `ChoiceScene`. Each either becomes a
  `fitted` leaf or keeps its scale on purpose; a leaf in a cell of its own has its height already,
  and one that computes cells from ink has not.
- **The pages** that would name a family: [home.be](../../firmware/data/system/home.be:19) and the two
  ramp chromes, whose titles and timers are the bands the family changes most.
- **The suites**: [gfx_drawing.cpp](../../tools/unit/cases/gfx_drawing.cpp) for the resolver, the
  sizes and the baseline, and [screen.test.be](../../tools/berry/tests/screen.test.be) for the call
  and the record's fields.

## How it is known to have worked

- `python tools/unit/check.py` compiles and runs the suites. The new cases are the resolver's rule as
  a table — a height below the shortest cut, each cut at scale 1, the 17-to-34 px gap, the largest
  scale, a width that steps the scale down, a width that steps the cut down, and a width nothing
  fits — and two fitted leaves in one band whose baselines agree.
- `python tools/berry/check.py` loads the image and drives every page with `fitted` in it, and its
  artifact GIF of the home page shows the timer filled where the built-in 5x7 left it small.
- The table above read against a 128x64 panel: every band the tree draws in is a row, and the row's
  answer is the picture on the glass.

## Steps

1. **The line box.** `ascent` and `descent` on `Font` in place of `yAdvance`, `lineHeight`, the
   generator and the four faces, `baseline()` and `drawFixedGlyph`, and the cases that pin a glyph's
   rows.
2. **The families.** `Family`, `findFamily`, `ttyp0` and `builtin`, and the case for a name that is
   neither.
3. **The resolver.** `Fit` and the two `resolve`s, and the case table that is its rule.
4. **The fitted leaf.** `gfx::fitted`, the node, the record's `family` field, the walk's branch,
   `screen.fitted` and the Berry case.
5. **What draws in it.** An editor's title, refusal and field text, the home page's title and timer,
   the two chromes, and the artifact captures re-recorded against them.

## Notes

- The Berry drawing vocabulary is being rewritten in the open plan in this folder that removes
  `screen.centred` and restates the parity between `screen` and the gfx vocabulary. The pages still
  call `centred` today, so this plan's leaf belongs in that parity as one more factory and `centred`
  should leave rather than gain a sibling; the later of the two lands rebased on the earlier.
- The scenes' scales are choices this plan does not revisit. `NumberScene`'s value at 2, the settings
  rows at 8 px and the arrows at 7 px are pictures measured against the built-in face, and a fitted
  replacement is a picture decision each one makes for itself. The pressure readout is the first of
  them to revisit, because its `size_hint` is already a question about the box.
- The three faces' ascent and descent come from the u8g2 header of the source named in each face's
  comment. That source is not in the tree, so the tool is run against a copy of it, and the diff
  should be the `Font` initializer and nothing else.
