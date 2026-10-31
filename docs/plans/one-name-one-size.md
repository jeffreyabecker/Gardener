# One name for a face, one rule for a size

## What is wrong now, and where

The fonts module grew a second vocabulary beside the first, and the drawing API now asks for text in
four spellings that do not agree on what a size is.

**Two nouns for one thing.** A face is a `const Font*`, named through `find`; a family is a `const
Family*`, named through `findFamily` ([Fonts.h](../../libraries/Gfx/src/Fonts.h:17)). `find` is
`findFamily`'s first cut spelled a second way, so one name answers both and the two cannot disagree
— but the leaves still disagree about which noun they are handed: `text` takes a face and a scale
([Drawing.h](../../libraries/Gfx/src/layout/Drawing.h:189)) and `fitted` takes a family and no scale
([Drawing.h](../../libraries/Gfx/src/layout/Drawing.h:193)). A caller writing a component has to
decide which of the two it will be built on, and that decision is what the rest of this plan
removes.

**A fallback with four names on it.** The module answers "what does a page that names nothing draw
in" three times over: `defaultFamily` says what it is, `setDefaultFamily` sets it, and `familyOr`
and `faceOr` apply it ([Fonts.h](../../libraries/Gfx/src/Fonts.h:32)). One line in the panel writes
the setter ([PanelRoot.cpp](../../firmware/gardener/PanelRoot.cpp:64)) and nothing reads the getter
outside the suites, so all four exist to carry `ttyp0` from the composition that chose it to the
leaves that draw — which is a job an argument does, without a global to hold it.

**Four spellings of a size.** `text` and `borrowedText` take a multiplier; `duration` takes a face
and no size at all ([Drawing.h](../../libraries/Gfx/src/layout/Drawing.h:201)); `glyph` takes a
multiplier of the Adafruit face; `pressureReadout` takes a face, a main multiplier and a second
multiplier, plus a `size_hint` that decides whether they sit side by side
([PressureReadout.h](../../libraries/Gardener/src/panel/page/PressureReadout.h:18)); `fitted` takes
the box's height. `centred` in Berry takes a face and a multiplier and centres by measuring the
string first ([Screen.cpp](../../libraries/Gardener/src/bindings/Screen.cpp:255)).

**The scenes cannot be asked for a design.** Each body fixes its own size in an anonymous namespace:
`Row = 8` in `SettingsScene` and `ChoiceScene`, `FieldScale = 2` in `DurationScene`, `ChipScale = 2`
in `BoolScene`, `ArrowHeight = 7` in `EditorScene`, `GlyphTop = 2` in `Overlay`, and the three font
arguments of the readout. A composition that wants the panel in another design has to edit the
scenes, and the two that were converted to a family name it inline
([NumberScene.cpp](../../libraries/Gardener/src/panel/page/NumberScene.cpp:58),
[EditorScene.cpp](../../libraries/Gardener/src/panel/page/EditorScene.cpp:147)).

**What the API costs today.**

| what it asks | how many |
| --- | --- |
| name questions — `find`, `findFamily`, a default, a setter, two coalescers | 6 |
| registrations — `registerFamily` | 1 |
| text leaves — `text`, `borrowedText`, `fitted` | 3 factories, 3 classes |
| `resolve` | 4 overloads |
| Berry kinds — `text`, `fitted`, `centred` | 3, over `font`, `family` and `scale` |

## The shape of the change

Two rules and one deletion, and everything below follows from them.

### A name is a family

**Every name a caller can write answers a family.** `findFamily` answers, in order: a family the
library ships, a family a caller registered, and a shipped face's name as a family of one cut. `""`
and a name nobody holds answer nothing, because a lookup has no opinion about a name it does not
hold. So a page that names `ttyp0` gets a design to fit, a page that names `ttyp016` gets that one
cut, and a page that names a word nobody registered gets no family — which draws in the Adafruit
family, because that is what no family means. The picture the module promises is the same one; what
changes is that the promise is kept once, where the fit is made, instead of inside the lookup.

`find` and `registerFont` go. A face's name is a family of one cut, so a caller that has a single
face registers it as a family of one (`registerFamily(name, &cut, 1)`), which is one table, one set
of refusals and one capacity ([Fonts.h](../../libraries/Gfx/src/Fonts.h:63)). `RegisteredFaces` goes
with the second table; `RegisteredFamilies` is the only budget a caller has to know.

The four designs the library ships hold the twelve faces between them: `adafruit` at one cut,
`ttyp0` at three, `profont` and `x11` at four each. Beside them is a table of one-cut families, one
per shipped face, so a page written against a size keeps drawing while a page that names a design
gets a ladder to fit. The Adafruit row of that table repeats the design it already answers to, and
the step that deletes `find` drops it; the other eleven are the faces with no one-cut name of their
own. A lookup walks the designs, then the faces, and allocates nothing.

### The size is the box

**Every leaf that draws text fits the height of the box it is given.** The multiplier leaves the
public API: it is not on a factory, not on the record the walk reads, and not in the module. A
caller that wants smaller text gives the leaf a smaller box, which is what a layout already says —
the settings rows are eight rows tall, the editor's bands are shares of the page, and the readout's
two numbers are two cells.

So the public way to ask for a size is to declare the cell, and the answer to "which cut" is the
resolver's, unchanged: the tallest cut whose line box fits, at the largest whole scale of it that
fits, narrowed by the string's width. `scale` survives inside `resolve` and inside `Text`, which is
the rasteriser; neither is a size a page or a scene writes.

The plain text leaves that place a string at the top-left of their box keep working by declaring
`{Start, Start}`, which is the one thing the placement argument adds.

A family pointer that is null is the Adafruit family, decided in `resolve` itself, so a leaf and a
measurement share one fallback and neither asks what a default is.

### One text leaf

`text(Box, String value, const Family* family, Justification place)` is the leaf. It resolves,
measures, places the line box by `place` and draws. `fitted` is the same leaf with the default place
(centre), so it goes; `borrowedText` and the owning `text` merge into it.

The value is owned. Two callers hand the leaf a string that dies with the call: the settings scene
builds its row as a local
([SettingsScene.cpp](../../libraries/Gardener/src/panel/page/SettingsScene.cpp:15)), and the readout
builds two readings as it draws — and a node outlives the call that made it. Owning every value is
one rule in place of a lifetime argument at each call, and the strings are a row and a reading long.

`glyph(Box, code, scale)` and `drawGlyph` go with it: a glyph by code is a one-byte string, and the
two nodes in the overlay that draw one become `text` with a one-byte value.

### No default

There is no default family, because a default is a name a caller never wrote. A family pointer that
is null is the Adafruit family, and that line sits inside `resolve`
([shapes/Text.cpp](../../libraries/Gfx/src/shapes/Text.cpp:1)), beside the faces it chooses
between. `defaultFamily`, `setDefaultFamily`, `familyOr` and `faceOr` go with it.

What the panel drew in ttyp0 because a global said so, it draws in ttyp0 because the composition
that builds its scenes names the design once and hands it to them. A scene that wants the Adafruit
face passes no family at all — which is what a row eight pixels tall does, since no ttyp0 cut's line
box fits eight.

### The Adafruit face

The face is `AdafruitFont`, in `fonts/AdafruitFont.h` and `.cpp`, named `"adafruit"`, and
`AdafruitFamily` and `AdafruitCuts` follow it. It was `BuiltinFont` and `"builtin"`, a name that
says nothing about where the table came from, taken from Adafruit_GFX's glcdfont.c. The BSD notice
stays where it is, at the head of the file the table is in, and the fonts README says where the
design came from. The old name is not kept as an alias: `builtin` answers no family, and no family
is the Adafruit family, so a stale name draws what it would have drawn and a reader finds no second
name for the table.

### The components take a family

Every component that draws text takes a `const Family*`:

```cpp
PaintNodeRef text(Box box, String value, const Family* family = nullptr,
                  Justification place = {Justify::Center, Justify::Center});
PaintNodeRef duration(Box box, DurationFormat format, int32_t seconds,
                      const Family* family = nullptr, bool known = true);
PaintNodeRef pressureReadout(Box box, const SystemState& state, const Family* family = nullptr);
```

`pressureReadout` loses `size_hint`, `scale` and `secondary_scale`. The two numbers are two cells:
the reading in the upper three fifths of the box and the target in the rest, each fitted, and a
reading too wide for its cell is narrowed by the resolver rather than by a second multiplier. A page
that wants them side by side gives the readout a box that is wide and short, which is a
composition's business.

A scene that needs a leaf's ink — to size an inverted title, to give a row the ink's box — asks
`measureText(family, value, width, height)`, which is the resolver plus a measurement and answers
exactly the ink the draw will take. Measuring at a multiplier goes: the question a scene has is
"what will this string take in this box", and its box is what it knows.

The scenes take a family in their constructors: `EditorScene(gestures, ux, label, family)`, and the
bodies hand it to what they draw, so a composition names the design once. The scenes that draw
nothing but eight-pixel rows — the settings list and the choice column — take no family, because a
row that short fits no cut of any design and so draws in the Adafruit face by naming nothing. The
duration scene inverts a field sized by its title's ink, which it measures with the family it was
given.

The composition is where the panel's design is written down: `PanelRoot` resolves `ttyp0` once where
it builds its scenes and passes it to each, so the panel's look is a line a reader can find rather
than a global a leaf has to consult.

## Alternatives, and why not

- **A `TextStyle` value carrying a family and a scale** — one argument instead of two, and every
  component unpacks the same struct. It keeps the multiplier, which is the thing the API cannot
  agree on, and it makes callers build a value to say what a box already says.
- **Keep `scale` and add `family` everywhere** — the cheapest diff and the state this plan exists to
  undo: two words for a size, one of which means a different number for every design.
- **A leaf per placement** — `text` at the top-left and `centred` in the middle, as Berry has them.
  The placement is one argument, and a kind per argument is how the vocabulary grew the drift.
- **Keep `builtin` as an alias for `adafruit`** — two names for one table, and a reader has no way
  to tell which is the one the module writes. `git` holds the old name.
- **Keep a face registry beside the families** — `find` and a separate table of caller-registered
  faces: two names for one design, two budgets and two sets of refusals, and a caller with a single
  face registering it twice.
- **Keep a default family and its setter** — a leaf that names nothing keeps drawing in whatever the
  composition chose, and the panel keeps the one line it writes today. It is a global that four
  public names exist to serve, one with no reader outside the suites, and it makes a picture depend
  on something no leaf was told.
- **A page that names a cut and a multiplier** — the face-and-scale form kept as a special case
  beside the family. It is a second way to ask, and the size it asks for is never narrowed to the
  box the way the resolver's is, so a leaf can draw text wider than its cell and leave the clip to
  the view.

## Entry points it has to hold on

- **The rasteriser**: `Text`, `drawFixedGlyph`, `measureText`'s glyph walk and `resolve` in
  [shapes/Text.cpp](../../libraries/Gfx/src/shapes/Text.cpp:1). They keep a face and a scale; the
  change is above them.
- **The face tables** in [Fonts.cpp](../../libraries/Gfx/src/Fonts.cpp:1): the twelve shipped faces,
  the one-cut families built from them, the four shipped families, and the refusals a family's name
  is made of.
- **The design's own header** [fonts/AdafruitFont.h](../../libraries/Gfx/src/fonts/AdafruitFont.h),
  which declares the face and declares the family of one cut beside it, so `resolve` can name the
  fallback from a header that belongs to a design rather than a default.
- **The coalescer** the four names hold today: `chosenFamily` in
  [Fonts.cpp](../../libraries/Gfx/src/Fonts.cpp:72), the one-line `familyOr` and `faceOr`, and the
  `setDefaultFamily` call in [PanelRoot.cpp](../../firmware/gardener/PanelRoot.cpp:64) that is the
  only thing that ever wrote it.
- **The prose that still says built-in** — four comments in
  [shapes/Text.h](../../libraries/Gfx/src/shapes/Text.h:13), one in
  [layout/Drawing.h](../../libraries/Gfx/src/layout/Drawing.h:83), one in
  [DrawTree.cpp](../../libraries/Gardener/src/bindings/DrawTree.cpp:114) and the row comments in the
  settings and choice scenes, each of which is rewritten by the step that changes the signature it
  describes.
- **The Berry walk** — `faceOf` and `familyOf` in
  [DrawTree.cpp](../../libraries/Gardener/src/bindings/DrawTree.cpp:108), and the five calls in
  [Screen.cpp](../../libraries/Gardener/src/bindings/Screen.cpp:232) that take a face, a scale or
  both: `text`, `centred`, `duration`, `pressure_readout` and `measure`.
- **The record**: `font` and `scale` go, `family`, `horizontal` and `vertical` stay, and the kinds
  `centred` and `fitted` fold into `text`.
- **The scenes**: the seven constructors, `PressureReadout`'s signature and the constants in each
  body's anonymous namespace.
- **The pages**: [home.be](../../firmware/data/system/home.be:19) and the two ramp chromes, which
  name `ttyp0` today and would name it through the one call.
- **The suites**: [gfx_drawing.cpp](../../tools/unit/cases/gfx_drawing.cpp),
  [gfx_fonts.cpp](../../tools/unit/cases/gfx_fonts.cpp),
  [gfx_scenes.cpp](../../tools/unit/cases/gfx_scenes.cpp),
  [screen.test.be](../../tools/berry/tests/screen.test.be) and
  [ink.test.be](../../tools/berry/tests/ink.test.be), which measure at a scale.

## How it is known to have worked

- `python tools/unit/check.py`: a name answering a one-cut family and an unheld name answering
  nothing, a `text` leaf with no family drawing the Adafruit face in an eight-row box and filling a
  forty-five row band with `ttyp022` when given the design, and a scene built with the profont
  family drawing its title in it.
- `python tools/berry/check.py --tests`: `screen.text` with a family in a box, and the ink test's
  claim that a string draws inside the width `measure` answers becoming exact rather than loose,
  because both now come from the resolver.
- The pictures: every band the tree draws in is answered by the cut its page used to name by hand,
  so the panel is unchanged — including the bands that grew because the panel's default used to
  answer them, which name `ttyp0` where they sit.

## Steps

1. **The Adafruit name.** `AdafruitFont` and `"adafruit"` through the tables, the fonts README and
   the cases that name it.
2. **A name is a family.** One-cut families for the shipped faces, `find` folded into `findFamily`,
   and `registerFont` folded into `registerFamily`, with the refusals and the budget that follow.
3. **One text leaf.** `text` fitting its box, taking a place and owning its value; `borrowedText`
   and `fitted` folded into it, and every leaf call in the scenes and the pages converted.
4. **The glyph as a string.** `glyph` and `drawGlyph` folded into a one-byte value through the leaf.
5. **Measuring a box.** `measureText(family, value, width, height)` and the scenes that size a box
   from it.
6. **The components take a family.** `duration`, `pressureReadout` with its two cells, and the scene
   constructors, with the panel naming `ttyp0` once where it builds them.
7. **The module.** `centred` and `fitted` folded into `screen.text`, `screen.measure` taking a box,
   and the record carrying a family and two places and nothing else.
8. **The surface.** `defaultFamily` and `setDefaultFamily` go with the global they read; `resolve`
   and `measureText` take a family and answer the Adafruit family for none; `familyOr`, `faceOr`,
   the face-taking `measureText` and the two name-taking `resolve` overloads go; `findFamily`
   answers nothing for a name nobody holds; and `find` goes with the Adafruit row of the one-cut
   table — so `Fonts.h` is a family, a lookup and a registration.
9. **The pages and the chromes** name the design they draw in where they place a leaf.

## Notes

- The plan that rewrites the Berry drawing vocabulary is still open in this folder: it removes
  `centred`, renames `bar`, `cell` and `align`, and states the parity between `screen` and gfx. This
  plan deletes `centred` as well, so whichever lands second rebases and must not reintroduce it; the
  parity table belongs to that plan and gains this plan's single text call.
- `scale` is not deleted from the tree, only from what a caller writes: `resolve` answers one,
  `Text` draws one, and the rasteriser's loops are built on it.
- After this, the Adafruit family is named in its own header, in the shipped tables and in
  `resolve`'s one line, and nowhere else: the eight-row rows name nothing and get it, everything
  else names a design, and `Fonts.h` is a `Family`, a lookup and a registration. That is the measure
  of whether the change landed.
