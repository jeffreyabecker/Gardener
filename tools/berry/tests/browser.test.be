# browser.test.be — the routine browser: the rows it reads from the registry, the window it shows them
# through, and the three ways a row is used — launched, edited and left. The browser is the only page
# that launches a routine, so the stack it moves is read here rather than in a page's suite.

suite = "browser"

# The browser on the stack over the home page, as pressing Up twice on home would put it there, and a
# beat for its activate to read the registry.
def opened()
  ux.push(BrowserScene())
  harness.beat(1)
  return ux.top()
end

# The rows the browser is showing, in the order the window draws them: the list is the chrome's first
# node and every line of it is a [bounds, draw] pair.
def shown_rows(browser)
  var list = child_at(browser.draw_page(), 0)
  var rows = []
  for line : list.children
    rows.push(line[1])
  end
  return rows
end

def the_rows_are_the_registrys()
  var browser = opened()
  assert_eq(classname(browser), "BrowserScene", "Up twice did not open the browser")

  var listed = routines.rows()
  assert_eq(browser.rows.size(), listed.size(), "the browser does not show every row")
  var i = 0
  while i < listed.size()
    assert_eq(browser.rows[i].value, listed[i].value, "a browser row stands for another routine")
    assert_eq(browser.rows[i].title, listed[i].title, "a browser row carries another title")
    i += 1
  end
end

test("the browser shows the rows the registry answers", the_rows_are_the_registrys)

def only_the_selected_row_is_inverted()
  var browser = opened()
  harness.press("down:single")
  harness.beat(1)

  var rows = shown_rows(browser)
  var i = 0
  while i < rows.size()
    var expected = i == browser.selected ? "inverted" : "row"
    assert_eq(rows[i].kind, expected, "row " + str(i) + " is drawn " + rows[i].kind)
    i += 1
  end
end

test("the selected row is inverted and no other", only_the_selected_row_is_inverted)

def the_window_carries_its_first_row_past_the_bottom_edge()
  var browser = opened()
  # The selection on the first row the window cannot show, which is where the window has to move. It is
  # set rather than pressed to: the image registers fewer routines than the window is tall.
  browser.selected = BrowserScene.VISIBLE
  harness.beat(1)
  assert_eq(browser.first(), 1, "the window did not carry its first row")

  var rows = shown_rows(browser)
  var top = rows[0]
  var drawn = top.kind == "inverted" ? child_at(top.child, 1) : child_at(top, 1)
  assert_eq(drawn.value, browser.rows[1].title, "the window's first row is not the second routine")
end

test("the window carries its first row past the bottom edge",
     the_window_carries_its_first_row_past_the_bottom_edge)

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
