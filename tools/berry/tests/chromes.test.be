# chromes.test.be — each chrome draws with live values and saves a frame, so a chrome can be seen
# without driving a whole page. Frames land in tools/berry/artifacts/components/.

suite = "chromes"

def save(name)
  return harness.save("tools/berry/artifacts/components/" + name + ".gif")
end

def home_draws()
  screen.draw_clear()
  var chrome = HomeChrome()
  chrome.draw(50.0)
  assert_eq(chrome.pressure.pos.x, 0, "the home pressure is not in the left half")
  assert_eq(chrome.pressure.dim.w, 64, "the home pressure is not half the page")
  assert_eq(chrome.pressure.dim.h, 52, "the home pressure is not the page's height")
  assert_eq(chrome.timer.pos.x, 64, "the home timer is not in the right half")
  assert_eq(chrome.timer.dim.w, 64, "the home timer is not half the page")
  save("home")
end

test("the home chrome draws", home_draws)

def static_draws()
  screen.draw_clear()
  var chrome = RampedStaticChrome("STATIC")
  chrome.draw("READY", 300000, 50000)
  assert_eq(chrome.icon.pos.x, 0, "the static glyph is not at the left")
  assert_eq(chrome.icon.dim.w, 16, "the static glyph column is not an eighth of the page")
  assert_eq(chrome.icon.dim.h, 13, "the static glyph column is not its row's height")
  assert_eq(chrome.title.pos.x, 16, "the static title does not follow the glyph column")
  assert_eq(chrome.title.dim.w, 112, "the static title is not the rest of the row")
  assert_eq(chrome.timer.pos.x, 64, "the static duration is not in the right half")
  assert_eq(chrome.timer.pos.y, 13, "the static duration is not on the readings row")
  assert_eq(chrome.bar.pos.x, 64, "the static bar is not in the right half")
  assert_eq(chrome.bar.pos.y, 39, "the static bar is not on the status row")
  assert_eq(chrome.bar.dim.w, 64, "the static bar is not half the page")
  save("static")
end

test("the ramped static chrome draws", static_draws)

def interval_draws()
  screen.draw_clear()
  var chrome = RampedIntervalChrome(IntervalIcon(Position.top_left(), Size(12, 8)), "INTERVAL",
                                    Position.top_left(), Size.page())
  chrome.draw(2, 4, "READY", 50)
  save("interval")
  assert_eq(chrome.sets.value, "2 / 4", "the chrome does not count the set under way")

  # A ready page has no set to count, so it shows what the run will make.
  chrome.draw(0, 4, "READY", 50)
  assert_eq(chrome.sets.value, "4 sets", "a ready chrome does not show the run's sets")
  chrome.draw(0, 1, "READY", 50)
  assert_eq(chrome.sets.value, "1 set", "a single set is not singular")
end

test("the ramped interval chrome draws", interval_draws)
