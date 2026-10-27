# chromes.test.be — each chrome draws with live values and saves a frame, so a chrome can be seen
# without driving a whole page. Frames land in tools/berry/artifacts/components/.

suite = "chromes"

def save(name)
  return harness.save("tools/berry/artifacts/components/" + name + ".gif")
end

# A chrome is the page's layout, so it is given the page before it draws, as ux gives it to the scene
# that holds it.
def page(component)
  component.to_box(0, 0, screen.page_width(), screen.page_height() - overlay_band_height)
  return component
end

def home_draws()
  screen.draw_clear()
  var chrome = page(HomeChrome())
  chrome.draw(50.0)
  assert_eq(chrome.pressure.pos.x, 0, "the home pressure is not in the left half")
  assert_eq(chrome.pressure.dim.w, 64, "the home pressure is not half the page")
  assert_eq(chrome.pressure.dim.h, 52, "the home pressure is not the page's height")
  assert_eq(chrome.timer.pos.x, 64 + (64 - chrome.timer.dim.w) / 2,
            "the home timer is not centred in the right half")
  assert_eq(chrome.timer.pos.y, (52 - chrome.timer.dim.h) / 2,
            "the home timer is not centred down the page")
  save("home")
end

test("the home chrome draws", home_draws)

def static_draws()
  screen.draw_clear()
  var chrome = page(RampedStaticChrome("STATIC"))
  chrome.draw("READY", 300000, 50000)
  assert_eq(chrome.icon.pos.x, 0, "the static glyph is not at the left")
  assert_eq(chrome.icon.dim.w, 16, "the static glyph column is not an eighth of the page")
  assert_eq(chrome.icon.dim.h, 13, "the static glyph column is not its row's height")
  assert_eq(chrome.title.pos.x, 16 + (112 - chrome.title.dim.w) / 2,
            "the static title is not centred in the rest of the row")
  assert_eq(chrome.title.pos.y, (13 - chrome.title.dim.h) / 2,
            "the static title is not centred down its row")
  assert_eq(chrome.timer.pos.x, 64 + (64 - chrome.timer.dim.w) / 2,
            "the static duration is not centred in the right half")
  assert_eq(chrome.timer.pos.y, 13 + (26 - chrome.timer.dim.h) / 2,
            "the static duration is not centred on the readings row")
  assert_eq(chrome.bar.pos.x, 64, "the static bar is not in the right half")
  assert_eq(chrome.bar.pos.y, 39, "the static bar is not on the status row")
  assert_eq(chrome.bar.dim.w, 64, "the static bar is not half the page")
  save("static")
end

test("the ramped static chrome draws", static_draws)

def interval_draws()
  screen.draw_clear()
  var chrome = page(RampedIntervalChrome(IntervalIcon(), "INTERVAL"))
  chrome.draw(2, 4, "READY", 50)
  assert_eq(chrome.icon.pos.x, 0, "the interval glyph is not at the left")
  assert_eq(chrome.icon.dim.w, 16, "the interval glyph column is not an eighth of the page")
  assert_eq(chrome.icon.dim.h, 13, "the interval glyph column is not its row's height")
  assert_eq(chrome.title.pos.x, 16 + (112 - chrome.title.dim.w) / 2,
            "the interval title is not centred in the rest of the row")
  assert_eq(chrome.title.pos.y, (13 - chrome.title.dim.h) / 2,
            "the interval title is not centred down its row")
  assert_eq(chrome.sets.pos.x, 64 + (64 - chrome.sets.dim.w) / 2,
            "the interval set count is not centred in the right half")
  assert_eq(chrome.sets.pos.y, 13 + (26 - chrome.sets.dim.h) / 2,
            "the interval set count is not centred on the readings row")
  assert_eq(chrome.bar.pos.x, 64, "the interval bar is not in the right half")
  assert_eq(chrome.bar.pos.y, 39, "the interval bar is not on the status row")
  assert_eq(chrome.bar.dim.w, 64, "the interval bar is not half the page")
  save("interval")
  assert_eq(chrome.sets.value, "2 / 4", "the chrome does not count the set under way")

  # A ready page has no set to count, so it shows what the run will make.
  chrome.draw(0, 4, "READY", 50)
  assert_eq(chrome.sets.value, "4 sets", "a ready chrome does not show the run's sets")
  chrome.draw(0, 1, "READY", 50)
  assert_eq(chrome.sets.value, "1 set", "a single set is not singular")
end

test("the ramped interval chrome draws", interval_draws)
