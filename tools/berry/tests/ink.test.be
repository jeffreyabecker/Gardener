# ink.test.be — what a page's drawing put on the glass. A page answers a tree; the walk from that tree to
# paints is DrawTree's, and the two things only the walk decides — a node that names no size taking the
# cell the layout gave it, and an inverted node flipping the whole cell its parent resolved — are read
# here as the ink a rectangle of the frame holds.

suite = "ink"

# The page area the overlay does not take, and the ink the frame holds of it.
def page_rows()
  return ux.overlay_start()
end

def page_area()
  return screen.width() * page_rows()
end

# A page that draws the tree a case hands it, pushed for a couple of beats so the walk paints it: the
# runner resets the stack before the next case, so a case leaves its page showing.
class InkPage
  var tree

  def init(tree)
    self.tree = tree
  end

  def draw_page()
    return self.tree
  end
end

def drew(tree)
  assert_true(ux.push(InkPage(tree)), "the page would not push")
  harness.beat(2)
end

def an_inverted_leaf_flips_the_whole_cell()
  var mark = screen.text(0, 0, 16, 8, "A")
  drew(screen.column(nil, [[bounds(1, 1), mark]]))
  var plain = harness.ink(0, 0, screen.width(), page_rows())
  assert_true(plain > 0, "the mark drew no ink")
  ux.pop()

  # The same cell flipped: ink where the mark is not, and the mark's own pixels left as paper, which is
  # the whole area counted twice over.
  drew(screen.column(nil, [[bounds(1, 1), screen.inverted(mark)]]))
  var flipped = harness.ink(0, 0, screen.width(), page_rows())
  assert_eq(plain + flipped, page_area(), "the flip did not cover the cell the mark sits in")
  assert_true(flipped > plain, "the flipped cell is not mostly ink")
end

test("an inverted leaf flips the whole cell", an_inverted_leaf_flips_the_whole_cell)

def a_child_that_names_no_size_fills_its_cell()
  # A bar with no box of its own takes the half of the row the layout gave it, and the two halves are
  # read against each other: the one filled to its end holds more ink than the one left empty.
  drew(screen.column(nil, [
    [bounds(1, 1), screen.row(nil, [
      [bounds(0.5, 1), screen.bar(nil, nil, nil, nil, 100.0)],
      [bounds(0.5, 1), screen.bar(nil, nil, nil, nil, 0.0)]
    ])]
  ]))

  var half = screen.width() / 2
  var filled = harness.ink(0, 0, half, page_rows())
  var empty = harness.ink(half, 0, half, page_rows())
  assert_true(filled > 0 && empty > 0, "a bar drew no frame at all")
  assert_true(filled > empty, "the filled bar is not the fuller of the two")
end

test("a child that names no size fills its cell", a_child_that_names_no_size_fills_its_cell)

def a_text_node_draws_inside_the_width_measure_answers()
  var string = "12.5"
  var box = screen.measure(string)
  drew(screen.column(nil, [[bounds(1, 1), screen.text(nil, nil, nil, nil, string)]]))

  var inside = harness.ink(0, 0, box.w, page_rows())
  assert_true(inside > 0, "the string drew no ink at all")
  assert_eq(harness.ink(box.w, 0, screen.width() - box.w, page_rows()), 0,
            "the string draws past the width measure answers")
  assert_true(inside < box.w * page_rows(), "the string filled the box measure answers")
end

test("a text node draws inside the width measure answers",
     a_text_node_draws_inside_the_width_measure_answers)
