# list.test.be — the rows a list draws: a row is a ListRow, the pair the list took before rows were
# records still normalizes into one, and the value a row stands for is the caller's and is never
# drawn.

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
