# list.test.be — the row list the image builds: the model the wrapper holds, the words it refuses,
# and the cell a drawn list inverts. These are answers across the Berry boundary, so tools/unit cannot
# see them: the class needs the interpreter to exist at all.

suite = "list"

# A page that hands back the tree a case gives it, pushed for a couple of beats so the walk paints it:
# the runner resets the stack before the next case, so a case leaves its page showing.
class ListPage
  var tree

  def init(tree)
    self.tree = tree
  end

  def draw_page()
    return self.tree
  end
end

def drew(tree)
  assert_true(ux.push(ListPage(tree)), "the page would not push")
  harness.beat(2)
end

# The three rows every model case steps through, as a fresh list so one case's keys are not another's.
def three_rows()
  return [["a", "Alpha"], ["b", "Beta"], ["c", "Gamma"]]
end

def the_model_rings_where_it_wraps_and_stops_where_it_clamps()
  var wrapping = RowList(8, three_rows(), "a", "wrap")
  assert_eq(wrapping.getSelectedValue(), "a", "the list did not open on the key it was given")
  assert_eq(wrapping.nudge(-1), true, "a ring did not step back off its first row")
  assert_eq(wrapping.getSelectedValue(), "c", "a ring did not wrap to its last row")
  assert_eq(wrapping.nudge(1), true, "a ring did not step forward off its last row")
  assert_eq(wrapping.getSelectedValue(), "a", "a ring did not wrap to its first row")
  assert_eq(wrapping.nudge(1), true, "a ring did not step to its second row")
  assert_eq(wrapping.getSelectedValue(), "b", "a ring stepped to the wrong row")

  var clamped = RowList(8, three_rows(), "a", "clamp")
  assert_eq(clamped.nudge(-1), false, "a clamp stepped back off its first row")
  assert_eq(clamped.getSelectedValue(), "a", "a clamp moved off its first row")
  var last = RowList(8, three_rows(), "c", "clamp")
  assert_eq(last.nudge(1), false, "a clamp stepped forward off its last row")
  assert_eq(last.getSelectedValue(), "c", "a clamp moved off its last row")
end

test("the model rings where it wraps and stops where it clamps",
     the_model_rings_where_it_wraps_and_stops_where_it_clamps)

def a_nil_selection_answers_nil_and_enters_at_the_first_row()
  var list = RowList(8, three_rows(), nil, "wrap")
  assert_nil(list.getSelectedValue(), "a list with no selection answered a value")
  assert_eq(list.nudge(1), true, "a list with no selection did not enter at its first row")
  assert_eq(list.getSelectedValue(), "a", "a list with no selection entered somewhere else")

  # A key no row holds is a selection the model cannot seat, which reads the same way.
  var missing = RowList(8, three_rows(), "gone", "wrap")
  assert_nil(missing.getSelectedValue(), "a key no row holds answered a value")
  assert_eq(missing.nudge(-1), true, "a key no row holds did not step")
  assert_eq(missing.getSelectedValue(), "c", "a key no row holds stepped somewhere else")
end

test("a nil selection answers nil and enters at the first row",
     a_nil_selection_answers_nil_and_enters_at_the_first_row)

# The words the class refuses at the call, each read through the error it raises.
def refused(kind, message, body)
  var raised = nil
  try
    body()
  except .. as raised_kind, raised_message
    raised = raised_kind
  end
  assert_true(raised != nil, message + " was accepted")
  assert_eq(raised, kind, message + " was refused as " + raised + " rather than " + kind)
end

def the_class_refuses_what_it_cannot_draw()
  refused("type_error", "a row that is not a pair", / -> RowList(8, ["a"], nil, "wrap"))
  refused("type_error", "a title that is not a string",
          / -> RowList(8, [["a", 7]], nil, "wrap"))
  refused("type_error", "a bitmap that is not a string",
          / -> RowList(8, [["a", "A", 7]], nil, "wrap"))
  refused("value_error", "an alignment word it does not hold",
          / -> RowList(8, [["a", "A", nil, "sideways"]], nil, "wrap"))
  refused("value_error", "a wrap word it does not hold", / -> RowList(8, [], nil, "ring"))
  refused("value_error", "a row height below one", / -> RowList(0, three_rows(), nil, "wrap"))
end

test("the class refuses what it cannot draw", the_class_refuses_what_it_cannot_draw)

def a_node_names_the_instance_it_draws()
  var list = RowList(8, three_rows(), "b", "wrap")
  var node = list.draw()
  assert_eq(node.kind, "row_list", "draw() does not answer a row list node")
  assert_eq(node.list, list, "the node draws another instance")
end

test("a node names the instance it draws", a_node_names_the_instance_it_draws)

def the_selected_row_is_the_cell_that_is_inverted()
  var list = RowList(8, three_rows(), "b", "wrap")
  drew(list.draw())

  # A row is the pitch the list was built at: eight rows here, so the three entries take the first
  # three of them. The current row is filled from edge to edge but for its glyphs, and the two beside
  # it are paper but for theirs.
  var cell = screen.width() * 8
  var above = harness.ink(0, 0, screen.width(), 8)
  var selected = harness.ink(0, 8, screen.width(), 8)
  var below = harness.ink(0, 16, screen.width(), 8)
  assert_true(selected > cell / 2, "the selected row is not the inverted cell")
  assert_true(above < cell / 4, "the row above the selection is inverted")
  assert_true(below < cell / 4, "the row below the selection is inverted")
end

test("the selected row is the cell that is inverted", the_selected_row_is_the_cell_that_is_inverted)

def the_window_scrolls_to_keep_the_current_row_in_view()
  # Ten rows at eight pixels outgrow the page, so the list shows the last six and puts the current
  # row at the bottom of them, with the bar beside the rows it cannot show.
  var rows = []
  var i = 0
  while i < 10
    rows.push(["r" + str(i), "R" + str(i)])
    i += 1
  end
  var page = ux.overlay_start()
  assert_true(page / 8 < 10, "the page is tall enough to show every row")
  drew(RowList(8, rows, "r9", "wrap").draw())

  var cell = screen.width() * 8
  var bottom = (page / 8 - 1) * 8
  assert_true(harness.ink(0, bottom, screen.width(), 8) > cell / 2,
              "the current row is not the last the window shows")
  assert_true(harness.ink(0, 0, screen.width(), bottom) < bottom * screen.width() / 4,
              "the window did not scroll to the current row")
  assert_true(harness.ink(screen.width() - 2, 0, 2, page) > 0,
              "a list that outgrows the page draws no bar")
end

test("the window scrolls to keep the current row in view",
     the_window_scrolls_to_keep_the_current_row_in_view)

def a_title_row_draws_its_title_and_a_mark_row_its_mark()
  # A title row is the string across the whole cell: the letter lands in the middle of the row and
  # nothing inks its left edge.
  drew(RowList(8, [["a", "A"]], nil, "clamp").draw())
  assert_true(harness.ink(screen.width() / 2 - 8, 0, 16, 8) > 0, "the title is not centred")
  assert_eq(harness.ink(0, 0, 8, 8), 0, "a title row inks the left edge of its cell")
end

test("a title row draws its title", a_title_row_draws_its_title_and_a_mark_row_its_mark)

def a_mark_sits_in_its_own_columns_before_the_title()
  # The bitmap is four by four with one ink pixel in its top-left corner, centred down the row: it
  # takes the first four columns, and the title the columns after it.
  drew(RowList(8, [["a", "A", "04040080000000"]], nil, "clamp").draw())
  assert_eq(harness.ink(0, 2, 4, 4), 1, "the mark is not a pixel of its own columns")
  assert_eq(harness.ink(0, 0, 4, 2), 0, "the mark is not centred down the row")
  assert_true(harness.ink(4, 0, screen.width() - 4, 8) > 0, "the title is not after the mark")
end

test("a mark sits in its own columns before the title",
     a_mark_sits_in_its_own_columns_before_the_title)

def a_bitmap_that_names_no_mark_leaves_the_title_alone()
  drew(RowList(8, [["a", "A", "not a bitmap"]], nil, "clamp").draw())
  assert_true(harness.ink(screen.width() / 2 - 8, 0, 16, 8) > 0,
              "the title is not in the cell the mark left it")
  assert_eq(harness.ink(0, 0, 8, 8), 0, "a mark that names nothing inks the row")
end

test("a bitmap that names no mark leaves the title alone",
     a_bitmap_that_names_no_mark_leaves_the_title_alone)
