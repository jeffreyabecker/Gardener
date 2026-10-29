# chromes.test.be — the page chromes as the trees their pure functions answer. Each is driven from
# the values a page would hand it, so the layout is reached without a machine or a page.

suite = "chromes"

def home_chrome_draws_the_title_the_readout_and_the_clock()
  var tree = home_chrome(65000)
  assert_eq(tree.kind, "column", "the home chrome is not a column")
  assert_eq(tree.children.size(), 2, "the home chrome is not a title and a body")

  var title = child_at(tree, 0)
  assert_eq(title.kind, "centred", "the home title is not centred")
  assert_eq(title.value, "Gardener", "the home title is not the device's name")

  var body = child_at(tree, 1)
  assert_eq(body.kind, "row", "the home body is not a row")
  assert_eq(child_at(body, 0).kind, "pressure_readout", "the home body does not hold the readout")
  assert_eq(child_at(body, 1).value, screen.duration_text("minutes", 65),
            "the home clock is not the run's elapsed time")

  var idle = home_chrome(nil)
  assert_eq(child_at(child_at(idle, 1), 1).value, "--", "an idle clock does not read --")
end

test("the home chrome draws the title, the readout and the clock",
     home_chrome_draws_the_title_the_readout_and_the_clock)

def ramped_static_chrome_draws_the_status_the_clock_and_the_bar()
  var glyph = "0c080000f3c012401240124012401e700000"
  var tree = ramped_static_chrome(glyph, "STATIC", "READY", 300000, 50000)
  assert_eq(tree.kind, "column", "the static chrome is not a column")
  assert_eq(tree.children.size(), 3, "the static chrome is not three rows")

  var top = child_at(tree, 0)
  assert_eq(child_at(top, 0).glyph, glyph, "the static chrome does not carry its glyph")
  assert_eq(child_at(top, 1).value, "STATIC", "the static title is not the routine's title")

  var middle = child_at(tree, 1)
  assert_eq(child_at(middle, 0).kind, "pressure_readout",
            "the static body does not hold the readout")
  assert_eq(child_at(middle, 1).value, screen.duration_text("minutes", 50),
            "the static clock is not the run's elapsed time")

  var bottom = child_at(tree, 2)
  assert_eq(child_at(bottom, 0).value, "READY", "the static status is not the machine's")
  var percent = child_at(bottom, 1).percent
  assert_true(percent > 16.6 && percent < 16.7, "the static bar is not the run's share")
end

test("the static chrome draws the status, the clock and the bar",
     ramped_static_chrome_draws_the_status_the_clock_and_the_bar)

def ramped_interval_chrome_counts_the_sets()
  var glyph = "0c080000f3c012401240124012401e700000"
  var running = ramped_interval_chrome(glyph, "INTERVAL", 2, 4, "WORK", 50.0)
  assert_eq(child_at(child_at(running, 1), 1).value, "2 / 4",
            "a running set is not counted out of the total")
  assert_eq(child_at(child_at(running, 2), 0).value, "WORK",
            "the interval status is not the machine's")
  assert_eq(child_at(child_at(running, 2), 1).percent, 50.0,
            "the interval bar is not the hold left")

  var ready = ramped_interval_chrome(glyph, "INTERVAL", 0, 4, "READY", nil)
  assert_eq(child_at(child_at(ready, 1), 1).value, "4 sets",
            "a ready page does not show the run's sets")
  assert_eq(child_at(child_at(ready, 2), 1).percent, 0.0,
            "a ready page shows hold time")

  var one = ramped_interval_chrome(glyph, "INTERVAL", 0, 1, "READY", 100.0)
  assert_eq(child_at(child_at(one, 1), 1).value, "1 set", "a single set is not singular")
end

test("the interval chrome counts the sets it will run", ramped_interval_chrome_counts_the_sets)

# The rows a browser chrome is given: the glyph, the title and the entry it stands for, which the
# chrome draws from.
class ChromeRow
  var icon, title

  def init(icon, title)
    self.icon = icon
    self.title = title
  end
end

def browser_chrome_draws_the_window_and_the_marker()
  var glyph = "0c080000f3c012401240124012401e700000"
  var rows = [ChromeRow(glyph, "Alpha"), ChromeRow(glyph, "Beta"), ChromeRow(glyph, "Gamma")]
  var tree = browser_chrome(rows, 1, 0, 2, 128)
  assert_eq(tree.kind, "row", "the browser chrome is not a row")
  assert_eq(tree.children.size(), 2, "the browser chrome is not the list and the scrollbar")

  var list = child_at(tree, 0)
  assert_eq(list.kind, "column", "the browser rows are not a column")
  assert_eq(list.children.size(), 2, "the browser does not show its window")
  var first = child_at(list, 0)
  assert_eq(first.kind, "row", "the first row is not a row")
  assert_eq(child_at(first, 0).glyph, glyph, "the first row does not carry its glyph")
  assert_eq(child_at(first, 1).value, "Alpha", "the first row does not carry its title")

  var selected = child_at(list, 1)
  assert_eq(selected.kind, "inverted", "the selected row is not inverted")
  assert_eq(child_at(selected.child, 1).value, "Beta",
            "the selected row is not the row it stands for")

  var bar = child_at(tree, 1)
  assert_eq(bar.kind, "scrollbar", "the browser has no scrollbar")
  assert_eq(bar.index, 0, "the scrollbar is not at the window's first row")
  assert_eq(bar.count, 3, "the scrollbar does not count every row")
  assert_eq(bar.visible, 2, "the scrollbar does not show the window's height")
end

test("the browser chrome draws the window and the marker",
     browser_chrome_draws_the_window_and_the_marker)
