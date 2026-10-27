# layout.test.be — the relative layout, and the box a component is given. An AutoLayout resolves
# each child's bounds against its own box, which it is given through to_box(); a component takes no
# box at construction, is given one through set_box(), and keeps it however its value changes.

suite = "layout"

# A component that records the order its draw ran in, for pinning the layout's child walk.
class RecordingChild : Component
  var name
  var log

  def init(name, log)
    super(self).init()
    self.name = name
    self.log = log
  end

  def draw()
    self.log.push(self.name)
    super(self).draw()
  end
end

# --- the relative layout ---------------------------------------------------

def a_layout_takes_the_box_it_is_given()
  var root = AutoLayout(bounds(nil, nil), [])
  root.to_box(0, 0, 128, 52)
  assert_eq(root.pos.x, 0, "the layout is not at the left of its box")
  assert_eq(root.pos.y, 0, "the layout is not at the top of its box")
  assert_eq(root.dim.w, 128, "the layout is not the box's width")
  assert_eq(root.dim.h, 52, "the layout is not the box's height")
end

test("a layout takes the box it is given", a_layout_takes_the_box_it_is_given)

def a_layout_stacks_its_rows_down()
  var glyph_row = AutoLayout(bounds(1, 0.25), [])
  var readings_row = AutoLayout(bounds(1, 0.50), [])
  var status_row = AutoLayout(bounds(1, 0.25), [])
  var root = AutoLayout(bounds(nil, nil), [glyph_row, readings_row, status_row])
  root.to_box(0, 0, 128, 52)
  root.draw()

  assert_eq(glyph_row.pos.x, 0, "the first row is not at the left")
  assert_eq(glyph_row.pos.y, 0, "the first row is not at the top")
  assert_eq(glyph_row.dim.w, 128, "the first row is not the page's width")
  assert_eq(glyph_row.dim.h, 13, "a quarter of 52 is not 13")

  assert_eq(readings_row.pos.y, 13, "the second row does not follow the first")
  assert_eq(readings_row.dim.h, 26, "half of 52 is not 26")

  assert_eq(status_row.pos.y, 39, "the third row does not follow the second")
  assert_eq(status_row.dim.h, 13, "the last row is not a quarter")
end

test("a layout whose children are full width stacks them down", a_layout_stacks_its_rows_down)

def a_layout_stacks_its_children_across()
  var left = AutoLayout(bounds(0.5, 1), [])
  var right = AutoLayout(bounds(0.5, 1), [])
  var root = AutoLayout(bounds(nil, nil), [
    AutoLayout(bounds(1, 0.25), []),
    AutoLayout(bounds(1, 0.50), [left, right]),
    AutoLayout(bounds(1, 0.25), [])
  ])
  root.to_box(0, 0, 128, 52)
  root.draw()

  assert_eq(left.pos.x, 0, "the left half is not at the left")
  assert_eq(left.pos.y, 13, "the left half is not on the readings row")
  assert_eq(left.dim.w, 64, "half of 128 is not 64")
  assert_eq(left.dim.h, 26, "the left half is not the row's height")

  assert_eq(right.pos.x, 64, "the right half does not follow the left")
  assert_eq(right.dim.w, 64, "the right half is not a half")
end

test("a layout whose children are full height stacks them across",
     a_layout_stacks_its_children_across)

def a_child_is_resolved_against_its_parent()
  var glyph = AutoLayout(bounds(0.125, 1), [])
  var title = AutoLayout(bounds(0.875, 1), [])
  var root = AutoLayout(bounds(nil, nil), [
    AutoLayout(bounds(1, 0.25), [glyph, title]),
    AutoLayout(bounds(1, 0.50), []),
    AutoLayout(bounds(1, 0.25), [])
  ])
  root.to_box(0, 0, 128, 52)
  root.draw()

  assert_eq(glyph.pos.x, 0, "the glyph column is not at the left")
  assert_eq(glyph.dim.w, 16, "an eighth of 128 is not 16")
  assert_eq(glyph.dim.h, 13, "the glyph column is not its row's height")
  assert_eq(title.pos.x, 16, "the title does not follow the glyph column")
  assert_eq(title.dim.w, 112, "the rest of the row is not 112")
end

test("a child is a fraction of the layout that places it", a_child_is_resolved_against_its_parent)

def a_component_with_no_bounds_takes_its_cells()
  var fill = ProgressBar(50)
  var root = AutoLayout(bounds(nil, nil), [AutoLayout(bounds(1, 0.5), [fill])])
  root.to_box(0, 0, 128, 52)
  root.draw()
  assert_eq(fill.pos.x, 0, "the leaf is not at the left of its layout")
  assert_eq(fill.pos.y, 0, "the leaf is not at the top of its layout")
  assert_eq(fill.dim.w, 128, "the leaf does not take the layout's width")
  assert_eq(fill.dim.h, 26, "the leaf does not take the layout's height")
end

test("a component that declares no bounds takes the cell it is given",
     a_component_with_no_bounds_takes_its_cells)

def auto_layout_walks_its_children_in_order()
  var log = []
  # The layouts place their leaves, so drawing walks both in the order they were added.
  var root = AutoLayout(bounds(nil, nil), [AutoLayout(bounds(1, 0.5), [
    RecordingChild("first", log)
  ]), AutoLayout(bounds(1, 0.5), [
    RecordingChild("second", log)
  ])])
  root.to_box(0, 0, 128, 52)
  root.draw()
  assert_eq(log.size(), 2, "the layout did not draw both leaves")
  assert_eq(log[0], "first", "the first leaf was not drawn first")
  assert_eq(log[1], "second", "the second leaf was not drawn second")
end

test("an AutoLayout draws its children in order", auto_layout_walks_its_children_in_order)

# A span is the fraction rounded to the nearest pixel, so a row declared as a fraction of the height
# it was measured from comes back as the pixels it declares, and equal thirds of a row fill it.
def equal_thirds_fill_a_row()
  var cells = [
    AutoLayout(bounds(real(1) / real(3), 1), []),
    AutoLayout(bounds(real(1) / real(3), 1), []),
    AutoLayout(bounds(real(1) / real(3), 1), [])
  ]
  var root = AutoLayout(bounds(nil, nil), [AutoLayout(bounds(1, 1), cells)])
  root.to_box(0, 0, 96, 16)
  root.draw()

  assert_eq(cells[0].dim.w, 32, "a third of 96 is not 32")
  assert_eq(cells[1].pos.x, 32, "the second third does not follow the first")
  assert_eq(cells[2].pos.x, 64, "the third third does not follow the second")
  assert_eq(cells[2].dim.w, 32, "the thirds do not fill the row")
end

test("equal thirds of a row fill it", equal_thirds_fill_a_row)

def a_row_resolves_to_the_pixels_it_declares()
  var heights = [8, 2, 15, 2, 8]
  var rows = []
  for height : heights
    rows.push(AutoLayout(bounds(1, real(height) / real(35)), []))
  end
  var root = AutoLayout(bounds(nil, nil), rows)
  root.to_box(0, 0, 71, 35)
  root.draw()

  assert_eq(rows[0].dim.h, 8, "the label row is not eight pixels")
  assert_eq(rows[1].dim.h, 2, "the paper row is not two pixels")
  assert_eq(rows[2].pos.y, 10, "the body does not start ten pixels down")
  assert_eq(rows[2].dim.h, 15, "the body is not the height it was measured at")
  assert_eq(rows[4].pos.y, 27, "the hint row is not the content's last eight pixels")
  assert_eq(rows[4].dim.h, 8, "the hint row is not eight pixels")
end

test("a row resolves to the pixels it declares", a_row_resolves_to_the_pixels_it_declares)

def a_rounded_span_is_clamped_to_what_is_left()
  var left = AutoLayout(bounds(0.5, 1), [])
  var right = AutoLayout(bounds(0.5, 1), [])
  var root = AutoLayout(bounds(nil, nil), [AutoLayout(bounds(1, 1), [left, right])])
  root.to_box(0, 0, 77, 16)
  root.draw()

  assert_eq(left.dim.w, 39, "half of 77 does not round up")
  assert_eq(right.dim.w, 38, "the clamped half is not what is left")
end

test("a rounded span is clamped to what is left", a_rounded_span_is_clamped_to_what_is_left)

# The alignment is where a thing sits in the space it was given, at both levels the box resolves at:
# the stack of children inside the layout, and a child's own room inside its cell.
def a_layout_takes_an_alignment()
  var plain = AutoLayout(bounds(nil, nil), [])
  assert_eq(plain.alignment.align, "left", "a layout does not default to the left")
  assert_eq(plain.alignment.v_align, "top", "a layout does not default to the top")

  var aimed = AutoLayout(bounds(nil, nil), [], alignment("center", "bottom"))
  assert_eq(aimed.alignment.align, "center", "the layout does not keep the align it was given")
  assert_eq(aimed.alignment.v_align, "bottom", "the layout does not keep the v_align it was given")

  var bare = alignment(nil, nil)
  assert_eq(bare.align, "left", "a bare alignment is not left")
  assert_eq(bare.v_align, "top", "a bare alignment is not top")
end

test("a layout takes the alignment it is given", a_layout_takes_an_alignment)

# A string is narrower than the row it was given, and the alignment is what places it: the box a text
# is stamped with is its ink, so the pixel it lands on is the alignment's answer.
def a_text_sits_where_the_alignment_says()
  var centred = Text("x")
  var root = AutoLayout(bounds(nil, nil), [
    AutoLayout(bounds(1, 12), [centred], alignment("center", "center"))
  ])
  root.to_box(0, 0, 128, 12)
  root.draw()
  var ink = centred.bounds(centred.value, centred.candidate)
  assert_eq(centred.dim.w, ink[0], "the text's box is not the ink it draws")
  assert_eq(centred.dim.h, ink[1], "the text's box is not the ink it draws")
  assert_eq(centred.pos.x, (128 - ink[0]) / 2, "a centred text is not centred across its cell")
  assert_eq(centred.pos.y, (12 - ink[1]) / 2, "a centred text is not centred down its cell")

  var cornered = Text("x")
  var plain = AutoLayout(bounds(nil, nil), [AutoLayout(bounds(1, 12), [cornered])])
  plain.to_box(0, 0, 128, 12)
  plain.draw()
  assert_eq(cornered.pos.x, 0, "a text with no alignment does not start at the left")
  assert_eq(cornered.pos.y, 0, "a text with no alignment does not start at the top")

  var cornered_right = Text("x")
  var right = AutoLayout(bounds(nil, nil), [
    AutoLayout(bounds(1, 12), [cornered_right], alignment("right", "bottom"))
  ])
  right.to_box(0, 0, 128, 12)
  right.draw()
  assert_eq(cornered_right.pos.x, 128 - ink[0], "a right text does not end at the right")
  assert_eq(cornered_right.pos.y, 12 - ink[1], "a bottom text does not end at the bottom")
end

test("a text sits where the layout's alignment says", a_text_sits_where_the_alignment_says)

# A declaration above one is pixels, so a cell of a fixed size no longer has to know the parent's.
def a_declared_size_above_one_is_pixels()
  var glyph = AutoLayout(bounds(12, 1), [])
  var title = AutoLayout(bounds(nil, 1), [])
  var row = AutoLayout(bounds(nil, nil), [glyph, title])
  row.to_box(0, 0, 128, 8)
  row.draw()
  assert_eq(glyph.pos.x, 0, "the glyph cell is not at the left")
  assert_eq(glyph.dim.w, 12, "a declared 12 is not twelve pixels")
  assert_eq(title.pos.x, 12, "the title does not follow the glyph cell")
  assert_eq(title.dim.w, 116, "the rest of the row is not 116")

  var wide = AutoLayout(bounds(200, 1), [])
  var root = AutoLayout(bounds(nil, nil), [wide])
  root.to_box(0, 0, 128, 8)
  root.draw()
  assert_eq(wide.dim.w, 128, "a declared size is not clamped to the box it resolves against")
end

test("a declared size above one is pixels", a_declared_size_above_one_is_pixels)

# A layout resolves its children on every draw, so a box that changes is resolved again.
def a_layout_is_resolved_on_each_draw()
  var row = AutoLayout(bounds(1, 0.5), [])
  var root = AutoLayout(bounds(nil, nil), [row])
  root.to_box(0, 0, 128, 52)
  root.draw()
  assert_eq(row.dim.h, 26, "a half of 52 is not 26")
  root.to_box(0, 0, 128, 100)
  root.draw()
  assert_eq(row.dim.h, 50, "the box a layout is given again is not resolved again")

  var t = Text("1")
  var line = AutoLayout(bounds(nil, nil), [AutoLayout(bounds(1, 12), [t])])
  line.to_box(0, 0, 128, 12)
  line.draw()
  var narrow = t.dim.w
  t.value = "12345"
  line.draw()
  assert_true(t.dim.w > narrow, "the text's box did not follow the value it grew")
  assert_true(t.dim.w <= 128, "the text's box is wider than the cell it was given")
end

test("a layout is resolved on each draw", a_layout_is_resolved_on_each_draw)

# --- the box a component keeps ---------------------------------------------

def component_stores_its_box()
  var t = Text("x")
  assert_nil(t.pos, "a component is given a box before it is built")
  t.set_box(point(3, 7), bounds(40, 12))
  assert_eq(t.pos.x, 3, "the box's x is not stored")
  assert_eq(t.pos.y, 7, "the box's y is not stored")
  assert_eq(t.dim.w, 40, "the box's width is not stored")
  assert_eq(t.dim.h, 12, "the box's height is not stored")
end

test("a component stores the box it was given", component_stores_its_box)

def text_keeps_its_box()
  var t = Text("42")
  t.set_box(point(4, 5), bounds(60, 20))
  t.draw()
  t.value = "a much longer string than before"
  t.draw()
  assert_eq(t.pos.x, 4, "the text's x moved after a value change")
  assert_eq(t.pos.y, 5, "the text's y moved after a value change")
  assert_eq(t.dim.w, 60, "the text's width changed after a value change")
  assert_eq(t.dim.h, 20, "the text's height changed after a value change")
end

test("a text keeps its box across a value change", text_keeps_its_box)
