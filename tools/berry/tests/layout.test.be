# layout.test.be — the relative layout, and the box a component is given. An AutoLayout resolves
# each child's bounds against its own box; a component takes no box at construction, is given one
# through set_box(), and keeps it however its value changes.

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

def the_page_root_fills_the_page()
  var root = AutoLayout(bounds(nil, nil), [])
  assert_eq(root.pos.x, 0, "the root is not at the left of the page")
  assert_eq(root.pos.y, 0, "the root is not at the top of the page")
  assert_eq(root.dim.w, 128, "the root is not the page's width")
  assert_eq(root.dim.h, 52, "the root is not the page's height")
end

test("bounds(nil, nil) is the whole page", the_page_root_fills_the_page)

def a_layout_stacks_its_rows_down()
  var glyph_row = AutoLayout(bounds(1, 0.25), [])
  var readings_row = AutoLayout(bounds(1, 0.50), [])
  var status_row = AutoLayout(bounds(1, 0.25), [])
  AutoLayout(bounds(nil, nil), [glyph_row, readings_row, status_row])

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
  AutoLayout(bounds(nil, nil), [
    AutoLayout(bounds(1, 0.25), []),
    AutoLayout(bounds(1, 0.50), [left, right]),
    AutoLayout(bounds(1, 0.25), [])
  ])

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
  AutoLayout(bounds(nil, nil), [
    AutoLayout(bounds(1, 0.25), [glyph, title]),
    AutoLayout(bounds(1, 0.50), []),
    AutoLayout(bounds(1, 0.25), [])
  ])

  assert_eq(glyph.pos.x, 0, "the glyph column is not at the left")
  assert_eq(glyph.dim.w, 16, "an eighth of 128 is not 16")
  assert_eq(glyph.dim.h, 13, "the glyph column is not its row's height")
  assert_eq(title.pos.x, 16, "the title does not follow the glyph column")
  assert_eq(title.dim.w, 112, "the rest of the row is not 112")
end

test("a child is a fraction of the layout that places it", a_child_is_resolved_against_its_parent)

def a_leaf_fills_its_layout_box()
  var t = Text("x")
  AutoLayout(bounds(nil, nil), [AutoLayout(bounds(1, 0.5), [t])])
  assert_eq(t.pos.x, 0, "the leaf is not at the left of its layout")
  assert_eq(t.pos.y, 0, "the leaf is not at the top of its layout")
  assert_eq(t.dim.w, 128, "the leaf does not take the layout's width")
  assert_eq(t.dim.h, 26, "the leaf does not take the layout's height")
end

test("a component that declares no bounds fills its layout's box", a_leaf_fills_its_layout_box)

def auto_layout_walks_its_children_in_order()
  var log = []
  # The layouts place their leaves, so drawing walks both in the order they were added.
  var root = AutoLayout(bounds(nil, nil), [AutoLayout(bounds(1, 0.5), [
    RecordingChild("first", log)
  ]), AutoLayout(bounds(1, 0.5), [
    RecordingChild("second", log)
  ])])
  root.draw()
  assert_eq(log.size(), 2, "the layout did not draw both leaves")
  assert_eq(log[0], "first", "the first leaf was not drawn first")
  assert_eq(log[1], "second", "the second leaf was not drawn second")
end

test("an AutoLayout draws its children in order", auto_layout_walks_its_children_in_order)

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
