# One name for a face, one rule for a size

## What is wrong now, and where

The fonts module grew a second vocabulary beside the first, and the drawing API now asks for text in
four spellings that do not agree on what a size is.

**Two nouns for one thing.** A face is a `const Font*`, named through `find`; a family is a `const
Family*`, named through `findFamily` ([Fonts.h](../../libraries/Gfx/src/Fonts.h:9)). Both are
public, both have a registry, and a leaf takes one or the other: `text` takes a face and a scale
([Drawing.h](../../libraries/Gfx/src/layout/Drawing.h:189)), `fitted` takes a family and no scale
([Drawing.h](../../libraries/Gfx/src/layout/Drawing.h:193)). A caller writing a component has to
decide which of the two it will be handed, and `builtin` is a face's name and a family's name at
once, so the decision cannot be made by reading the string.

**Two fallbacks, one for each noun.** A draw that names no face falls back to `defaultFace`, which
`setDefaultFace` sets ([Fonts.h](../../libraries/Gfx/src/Fonts.h:12)); a fitted leaf that names no
family falls back to the built-in family through `findFamily(nullptr)`. There is no single answer to
"what does a page that names nothing draw in", and `setDefaultFace` has no caller outside the
suites.

**Four spellings of a size.** `text` and `borrowedText` take a multiplier; `duration` takes a face
and no size at all ([Drawing.h](../../libraries/Gfx/src/layout/Drawing.h:201)); `glyph` takes a
multiplier of the built-in face; `pressureReadout` takes a face, a main multiplier and a second
multiplier, plus a `size_hint` that decides whether they sit side by side
([PressureReadout.h](../../libraries/Gardener/src/panel/page/PressureReadout.h:18)); `fitted` takes
the box's height. `centred` in Berry takes a face and a multiplier and centres by measuring the
string first ([Screen.cpp](../../libraries/Gardener/src/bindings/Screen.cpp:255)).

**The default face's name says nothing about where it came from.** `BuiltinFont`, `"builtin"` and
`BuiltinFamily` describe a table taken from Adafruit_GFX's glcdfont.c, whose BSD notice is in the
file ([BuiltinFont.cpp](../../libraries/Gfx/src/fonts/BuiltinFont.cpp:1)). "Built-in" is true of
every face in the tree.

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
| name questions — `defaultFace`, `setDefaultFace`, `faceOr`, `find`, `findFamily` | 5 |
| registrations — `registerFont`, `registerFamily` | 2 |
| text leaves — `text`, `borrowedText`, `fitted` | 3 factories, 3 classes |
| `resolve` | 4 overloads |
| Berry kinds — `text`, `fitted`, `centred` | 3, over `font` and `scale` |

## The shape of the change

Two rules, and everything below follows from them.

### A name is a family

**Every name a caller can write answers a family.** `findFamily` answers, in order: a family the
library ships, a family a caller registered, a shipped face's name as a family of one cut, and the
default family for `""` and for a name nobody holds. So a page that names `ttyp0` gets a design to
fit and a page that names `ttyp016` gets that one cut, and a name that means nothing draws in the
default family rather than not drawing, which is what the module already promises.

`find` and `registerFont` go. A face's name is a family of one cut, so a caller that has a single
face registers it as a family of one (`registerFamily(name, &cut, 1)`), which is one table, one set
of refusals and one capacity ([Fonts.h](../../libraries/Gfx/src/Fonts.h:59)). `RegisteredFaces` goes
with the second table; `RegisteredFamilies` is the only budget a caller has to know.

The one-cut families for the twelve shipped faces are a static table beside the families, built once
from the face table, so a lookup costs a walk of two small arrays and nothing is allocated.

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

### One text leaf

`text(Box, const char* value, const Family* family, Justification place)` is the leaf. It resolves,
measures, places the line box by `place` and draws. `fitted` is the same leaf with the default place
(centre), so it goes; `borrowedText` and the owning `text` merge into it.

The string is borrowed. An owning form existed for a caller whose string was a temporary; the only
such caller is the readout, which builds its reading as it draws and can keep it in a local for the
length of the call, as it already does.

`glyph(Box, code, scale)` and `drawGlyph` go with it: a glyph by code is a one-byte string, and the
two nodes in the overlay that draw one become `text` with a one-byte value.

### The default family

`defaultFamily()`, `setDefaultFamily()` and `familyOr()` replace the three face forms. `familyOr` is
what every leaf and every measurement goes through, so the fallback is one answer in one place: the
Adafruit family unless something sets another, and `setDefaultFamily(nullptr)` puts that back.

### The Adafruit face

`BuiltinFont` becomes `AdafruitFont`, in `fonts/AdafruitFont.h` and `.cpp`, and its name becomes
`"adafruit"`; `BuiltinFamily` and `BuiltinCuts` follow. The BSD notice stays where it is, at the
head of the file the table is in, and the fonts README says where the design came from. The old name
is not kept as an alias: `builtin` then answers the default family, which is the Adafruit family
unless a composition says otherwise, so a stale name draws what it would have drawn and a reader
finds no second name for the table.

### The components take a family

Every component that draws text takes a `const Family*`:

```cpp
PaintNodeRef text(Box box, const char* value, const Family* family = nullptr,
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
bodies hand it to what they draw, so a composition names the design once. Two exceptions are named
rather than inherited: the settings rows and the choice rows are eight pixels tall, which no ttyp0
cut fits, so those name the Adafruit family because the row's height is what chooses it; and the
duration scene inverts a field sized by its title's ink, which it measures with the family it was
given.

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
- **Keep `find` and `registerFont` for faces** — two registries, two fallbacks and two names for one
  design: the state this plan is undoing.
- **A page that names a cut and a multiplier** — the face-and-scale form kept as a special case
  beside the family. It is a second way to ask, and the size it asks for is never narrowed to the
  box the way the resolver's is, so a leaf can draw text wider than its cell and leave the clip to
  the view.

## Entry points it has to hold on

- **The rasteriser**: `Text`, `drawFixedGlyph`, `measureText`'s glyph walk and `resolve` in
  [shapes/Text.cpp](../../libraries/Gfx/src/shapes/Text.cpp:1). They keep a face and a scale; the
  change is above them.
- **The face tables** in [Fonts.cpp](../../libraries/Gfx/src/Fonts.cpp:1): the twelve shipped faces,
  the one-cut families built from them, the four shipped families, and the refusals both a face's
  name and a family's name are made of.
- **The rename** in `fonts/AdafruitFont.h` and `.cpp`, the README's paragraph and the two cases in
  [gfx_drawing.cpp](../../tools/unit/cases/gfx_drawing.cpp:43) that name `"builtin"`.
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

- `python tools/unit/check.py`: a name answering a one-cut family, `findFamily(nullptr)` answering
  whatever `setDefaultFamily` set, a `text` leaf filling a forty-five row band with `ttyp022` at two
  scales, and a scene built with the profont family drawing its title in it.
- `python tools/berry/check.py --tests`: `screen.text` with a family in a box, and the ink test's
  claim that a string draws inside the width `measure` answers becoming exact rather than loose,
  because both now come from the resolver.
- The pictures: every band the tree draws in is answered by the cut its page used to name by hand,
  so the panel is unchanged except where the pages that named no size grow into their bands.

## Steps

1. **The Adafruit rename.** `AdafruitFont` and `"adafruit"` through the tables, the fonts README and
   the cases that name it, with the default family still the Adafruit one.
2. **One default.** `defaultFamily`, `setDefaultFamily` and `familyOr` in place of the three face
   forms, and `findFamily(nullptr)` answering the same family they do.
3. **A name is a family.** One-cut families for the shipped faces, `find` and `registerFont` folded
   into `findFamily` and `registerFamily`, and the refusals and budgets that follow.
4. **One text leaf.** `text` fitting its box and taking a place; `borrowedText`, `fitted`, `glyph`
   and `drawGlyph` folded into it; the record's `font` and `scale` gone.
5. **Measuring a box.** `measureText(family, value, width, height)` and the scenes that size a box
   from it.
6. **The components take a family.** `duration`, `pressureReadout` and the seven scene constructors,
   with the eight-row bodies naming the Adafruit family and explaining why.
7. **The module and the pages.** `screen.text` with a family and a place, `screen.measure` with a
   box, the two kinds gone from the walk, and the pages naming their design through the one call.

## Notes

- The plan that rewrites the Berry drawing vocabulary is still open in this folder: it removes
  `centred`, renames `bar`, `cell` and `align`, and states the parity between `screen` and gfx. This
  plan deletes `centred` as well, so whichever lands second rebases and must not reintroduce it; the
  parity table belongs to that plan and gains this plan's single text call.
- `scale` is not deleted from the tree, only from what a caller writes: `resolve` answers one,
  `Text` draws one, and the rasteriser's loops are built on it.
- The eight-row bodies are the only places that name the Adafruit family after this; everything else
  names a design or inherits the default, and that is the measure of whether the change landed.
