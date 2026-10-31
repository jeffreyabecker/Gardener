# One name for a face, one rule for a size

The steps of [the plan](../plans/one-name-one-size.md), in the order they land. One item is one
branch, one commit and a diff; every item leaves the tree building and the suites passing.

## The Adafruit name

- [x] Rename the built-in face to the design it came from — `AdafruitFont`, `fonts/AdafruitFont.h`
      and `.cpp`, the name `adafruit`, the family `AdafruitFamily`, the README and the cases that
      name it — so nothing in the tree calls one face the built-in one.

## A name is a family

- [x] Answer a face's name as a family of one cut, and fold `find` into `findFamily`.
- [x] Fold `registerFont` into `registerFamily`, so one table, one budget and one set of refusals
      hold every name a caller adds.

## One text leaf

- [ ] Fold `borrowedText` and `fitted` into one `text` that fits its box, takes a place and owns its
      value, and convert every leaf call in the scenes and the pages.
- [ ] Fold `glyph` and `drawGlyph` into a one-byte value through the text leaf, so the overlay's two
      glyph nodes are text.
- [ ] Measure a box: `measureText(family, value, width, height)` answers the ink a fitted draw
      takes, and the scenes that size a box from ink ask it.

## The components take a family

- [ ] Give `duration` and `pressureReadout` a family, and let the readout divide its box into two
      cells instead of two scales, dropping `size_hint`, `scale` and `secondary_scale`.
- [ ] Give the scene constructors a family and name `ttyp0` once where the panel builds them, so the
      design reaches a scene as an argument and the panel stops writing a global to reach it.

## The module

- [ ] Fold `centred` into `screen.text`, which takes a family and a place.
- [ ] Fold `fitted` into it as well and give `screen.measure` a box, so the record carries a family
      and two places and neither `font` nor a kind.

## The surface is a family, a lookup and a registration

- [ ] Delete `defaultFamily` and `setDefaultFamily` with the `chosenFamily` global they hold, now
      that nothing sets or reads them.
- [ ] Let `resolve` and `measureText` take a `const Family*` and answer the Adafruit family for no
      family, declare `AdafruitFamily` beside the face in `fonts/AdafruitFont.h`, and delete
      `familyOr`, `faceOr` and the face-taking `measureText` with the coalescing they hold.
- [ ] Answer `nullptr` from `findFamily` for a name nobody holds, and delete the two `resolve`
      overloads that take a family's name, so a name is looked up in one place and fitted in one.
- [ ] Delete `find` and the Adafruit row of the one-cut table, which repeats the design the lookup
      already answers with, so no caller names a face and every name answers a family.

## The pages

- [ ] Name the design in the pages and the chromes where they place a leaf, so no node draws in a
      design its page did not name.
