# Font families and the height resolver

The steps of [the plan](../plans/font-families.md), in the order they land. One item is one branch,
one commit and a diff; every item leaves the tree building and the suite passing.

## The line box

- [ ] Give `Font` its ascent and its descent in place of `yAdvance`, fill them for the built-in and
      the three ttyp0 faces, and have `from_u8g2.py` write both, so `lineHeight` answers without a
      string in hand.
- [ ] Place a text's baseline from the face's ascent for both kinds, so the same band puts two
      strings on one line, and move the case that pins a ttyp0 glyph's rows with it.

## The shipped faces

- [ ] Hold the shipped faces in a table the module writes and have `find` scan it first, so a
      registration is a caller's only and `RegisteredFaces` is a caller's budget.

## The families

- [ ] Add `Family` and `findFamily` with `builtin` and `ttyp0` as the two shipped families, so a
      page can name a design rather than a size.
- [ ] Add `registerFamily`, keeping its own copy of the cuts ordered by line height and refusing what
      a face registry refuses, and have `registerFont` refuse a name a family answers.

## The two designs

- [ ] Land ProFont and X11 as two more families: eight rungs converted with their licences and the
      README's paragraphs, so a page names `profont` or `x11` and the family answers a band.

## The resolver

- [ ] Add `Fit` and `resolve(family, height)` with the width form, so a height and a box answer a
      cut and a scale.
- [ ] Add the two by-name overloads, so a name resolves as the family it found does.

## The fitted leaf

- [ ] Add `gfx::fitted` with its record field, its walk branch and `screen.fitted`, so a page draws
      a string at the cut its box's height takes.

## The pages

- [ ] Name `ttyp0` in the editor's title, refusal and field text and in the home page's title and
      timer, and re-record the captures, so the panel's bands are filled by a real cut.
