# list.test.be — the rows a list draws: a row is a ListRow, the pair the list took before rows were
# records still normalizes into one, the value a row stands for is the caller's and is never drawn,
# and a drawn list boxes each visible row and fills the one the index names.

suite = "list"

def rows_are_records()
  var icon = Icon()
  icon.set_box(point(0, 0), bounds(4, 4))
  var list = List(4)
  list.set([ListRow(icon, "Alpha", "a"), [nil, "Beta"], "Gamma"])

  assert_eq(list.rows.size(), 3, "the list does not hold a row for every entry")
  assert_eq(list.rows[0].title, "Alpha", "a ListRow's title did not survive the list")
  assert_eq(list.rows[0].value, "a", "a ListRow did not keep the value it stands for")
  assert_eq(list.rows[0].icon, icon, "a ListRow did not keep its icon")
  assert_eq(list.rows[1].title, "Beta", "a [icon, title] pair did not become a row")
  assert_nil(list.rows[1].icon, "a pair with no icon was given one")
  assert_nil(list.rows[1].value, "a pair was given a value")
  assert_eq(list.rows[2].title, "Gamma", "a bare title did not become a row")
end

test("rows are records", rows_are_records)

def the_selected_row_is_the_one_showing()
  var list = List(4)
  assert_nil(list.selected_row(), "an empty list named a row")

  list.set([ListRow(nil, "Alpha", "a"), ListRow(nil, "Beta", "b")])
  assert_eq(list.selected_row().value, "a", "the list does not open on its first row")

  list.move(1)
  assert_eq(list.selected_row().value, "b", "the selected row did not follow the index")
  list.move(1)
  assert_eq(list.selected_row().value, "a", "the list does not wrap at the end")
end

test("the selected row is the one showing", the_selected_row_is_the_one_showing)

# A drawn list boxes the rows it shows — its own width, one row pitch tall, the icon at the row's
# corner — and fills the band of the one the index names and no other.
def a_drawn_list_fills_the_selected_row()
  var icon = Icon()
  var list = List(2)
  list.set([ListRow(icon, "Alpha", "a"), ListRow(nil, "Beta", "b")])
  list.move(1)
  list.to_box(3, 5, 40, 2 * List.row)
  list.draw()

  assert_eq(list.rows[1].pos.x, 3, "a row is not drawn at the list's left")
  assert_eq(list.rows[1].pos.y, 5 + List.row, "a row is not drawn at the row pitch")
  assert_eq(list.rows[1].dim.w, 40, "a row does not take the list's width")
  assert_eq(list.rows[1].dim.h, List.row, "a row is not one row tall")
  assert_true(list.rows[1].inverted, "the selected row is not the one filled")
  assert_eq(list.rows[0].inverted, false, "a row that is not selected is filled")

  assert_eq(icon.pos.x, 3, "the icon is not drawn at the row's left")
  assert_eq(icon.pos.y, 5, "the icon is not drawn on its own row")
end

test("a drawn list fills the selected row", a_drawn_list_fills_the_selected_row)
