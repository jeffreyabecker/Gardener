# layout.test.be — the flat layout and the box-at-init contract. A layout draws its children in
# order and nothing more; a component keeps the box it was given, however its value changes.

suite = "layout"

# A component that records the order its draw ran in, for pinning the layout's child walk.
class RecordingChild : Component
  var name
  var log

  def init(pos, dim, name, log)
    super(self).init(pos, dim)
    self.name = name
    self.log = log
  end

  def draw()
    self.log.push(self.name)
    super(self).draw()
  end
end

def component_stores_its_box()
  var t = Text(Position(3, 7), Size(40, 12), "x")
  assert_eq(t.pos.x, 3, "the box's x is not stored")
  assert_eq(t.pos.y, 7, "the box's y is not stored")
  assert_eq(t.dim.w, 40, "the box's width is not stored")
  assert_eq(t.dim.h, 12, "the box's height is not stored")
end

test("a component stores the box it was given", component_stores_its_box)

def layout_draws_children_in_order()
  var log = []
  var l = Layout([
    RecordingChild(Position(0, 0), Size(1, 1), "first", log),
    RecordingChild(Position(0, 0), Size(1, 1), "second", log)
  ])
  l.draw()
  assert_eq(log.size(), 2, "the layout did not draw both children")
  assert_eq(log[0], "first", "the first child was not drawn first")
  assert_eq(log[1], "second", "the second child was not drawn second")
end

test("a layout draws its children in order", layout_draws_children_in_order)

def layout_add_grows_the_list()
  var log = []
  var l = Layout([])
  assert_eq(l.children.size(), 0, "a new layout is not empty")
  l.add(RecordingChild(Position(0, 0), Size(1, 1), "added", log))
  assert_eq(l.children.size(), 1, "add did not grow the list")
  l.draw()
  assert_eq(log.size(), 1, "the added child was not drawn")
  assert_eq(log[0], "added", "the added child was not drawn")
end

test("add grows the flat list", layout_add_grows_the_list)

def text_keeps_its_box()
  var t = Text(Position(4, 5), Size(60, 20), "42")
  t.draw()
  t.value = "a much longer string than before"
  t.draw()
  assert_eq(t.pos.x, 4, "the text's x moved after a value change")
  assert_eq(t.pos.y, 5, "the text's y moved after a value change")
  assert_eq(t.dim.w, 60, "the text's width changed after a value change")
  assert_eq(t.dim.h, 20, "the text's height changed after a value change")
end

test("a text keeps its box across a value change", text_keeps_its_box)
