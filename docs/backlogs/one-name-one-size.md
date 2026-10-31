# One name for a face, one rule for a size

The steps of [the plan](../plans/one-name-one-size.md), in the order they land. One item is one
branch, one commit and a diff; every item leaves the tree building and the suites passing.

## The Adafruit name

- [x] Rename the built-in face to the design it came from — `AdafruitFont`, `fonts/AdafruitFont.h`
      and `.cpp`, the name `adafruit`, the family `AdafruitFamily`, the README and the cases that
      name it — so nothing in the tree calls one face the built-in one.

## One default

- [ ] Make the default a family — `defaultFamily`, `setDefaultFamily` and `familyOr` in place of the
      three face forms, and `findFamily(nullptr)` answering the same one — so a page that names
      nothing has one fallback.
- [ ] Set the device's default family to ttyp0 where the panel comes up, so a scene that names no
      family draws in the look the pages were composed with.

## A name is a family

- [ ] Answer a face's name as a family of one cut, and fold `find` into `findFamily`.
- [ ] Fold `registerFont` into `registerFamily`, so one table, one budget and one set of refusals
      hold every name a caller adds.

## One text leaf

- [ ] Fold `text`, `borrowedText` and `fitted` into one `text` that fits its box and takes a place,
      and drop the owning leaf.
- [ ] Fold `glyph` and `drawGlyph` into a one-byte string through the text leaf.
- [ ] Measure a box: `measureText(family, value, width, height)` answers the ink a fitted draw takes,
      and the scenes that size a box from ink ask it.
- [ ] Fold the record's kinds into one `text` carrying `family`, `horizontal` and `vertical`, with
      `font` and `scale` gone from the walk.

## The components take a family

- [ ] Give `duration` and `pressureReadout` a family, and let the readout divide its box into two
      cells instead of two scales.
- [ ] Give the scene constructors a family, with the eight-row bodies naming the Adafruit family and
      saying why.

## The module and the pages

- [ ] Give the module one text call and a measure that takes a box, and remove `centred` and
      `fitted` from it and from the walk.
- [ ] Name the design once in the pages and the chromes, and the panels are the pictures the
      resolver now answers.
