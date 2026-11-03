# browser.test.be — the routine browser: the rows it hands the list, the keys that move it, and the
# three ways a row is used — launched, edited and left. The browser is the only page that launches a
# routine, so the stack it moves is read here rather than in a page's suite.

suite = "browser"

# The browser on the stack over the home page, as pressing Forward twice on home would put it there,
# and a beat for its activate to read the registry.
def opened()
  ux.push(BrowserScene())
  harness.beat(1)
  return ux.top()
end

# The title a row's draw carries: the last of the cells the row was built from, the icon's if it has
# one and the title's own otherwise.
def title_of(row)
  return child_at(row, row.children.size() - 1).value
end

def the_rows_are_the_registrys()
  var browser = opened()
  assert_eq(classname(browser), "BrowserScene", "Forward twice did not open the browser")

  var listed = routines.rows()
  assert_eq(browser.rows.size(), listed.size(), "the browser does not show every row")
  assert_eq(browser.list.rows.size(), listed.size(), "the list does not hold every row")
  var i = 0
  while i < listed.size()
    var pair = browser.list.rows[i]
    assert_eq(pair[0], listed[i].value, "a list row stands for another routine")
    assert_eq(pair[1].kind, "row", "a list row is not drawn as a row")
    assert_eq(title_of(pair[1]), listed[i].title, "a list row carries another title")
    i += 1
  end
end

test("the browser shows the rows the registry answers", the_rows_are_the_registrys)

def the_keys_move_the_row_the_list_shows()
  var browser = opened()
  var first = browser.list.getSelectedValue()
  assert_eq(first, browser.rows[0].value, "the list did not open on the first row")

  harness.press("down:single")
  harness.beat(1)
  assert_eq(browser.list.getSelectedValue(), browser.rows[1].value,
            "Down did not move the list to the second row")
  assert_eq(browser.selected_row().value, browser.rows[1].value,
            "the row the browser reads is not the one the list shows")

  harness.press("up:single")
  harness.beat(1)
  assert_eq(browser.list.getSelectedValue(), first, "Up did not move the list back")

  # The page is the list's node, naming the instance the browser builds it with.
  var tree = browser.draw_page()
  assert_eq(tree.kind, "row_list", "the browser does not draw a row list")
  assert_eq(tree.list, browser.list, "the browser draws another list")
end

test("the keys move the row the list shows", the_keys_move_the_row_the_list_shows)

def ok_launches_the_row_it_stands_on()
  var browser = opened()
  var row = browser.selected_row()

  harness.press("ok:single")
  harness.beat(2)
  assert_eq(ux.depth(), 3, "OK did not launch over the browser")
  var page = ux.top()
  assert_true(page != nil, "OK left no page showing")
  assert_eq(page.routine_key, row.value, "OK launched another routine")
end

test("OK launches the record its row stands on", ok_launches_the_row_it_stands_on)

def forward_opens_the_list_and_launches_once_it_settles()
  var browser = opened()
  var row = browser.selected_row()

  harness.press("forward:single")
  harness.beat(1)
  assert_eq(ux.depth(), 3, "Forward did not open a list over the browser")
  # The list is the settings module's own scene, so it answers no page: nothing the image wrote is
  # under it, which is what a nil the stack reports means.
  assert_nil(ux.top(), "the settings list answered a page")

  # OK closes the list on the set it was opened with, which is what launches the routine.
  harness.press("ok:single")
  harness.beat(2)
  assert_eq(ux.depth(), 3, "the settled list did not launch over the browser")
  var page = ux.top()
  assert_true(page != nil, "the settled list launched no page")
  assert_eq(page.routine_key, row.value, "the settled list launched another routine")
end

test("Forward opens the list and launches on its answer",
     forward_opens_the_list_and_launches_once_it_settles)

def back_leaves_the_browser()
  var browser = opened()
  harness.press("back:single")
  harness.beat(1)
  assert_eq(ux.depth(), 1, "Back did not leave the browser")
  assert_eq(classname(ux.top()), "HomeScene", "the page under the browser is not home")
end

test("Back leaves the browser for the page it was pushed from", back_leaves_the_browser)
