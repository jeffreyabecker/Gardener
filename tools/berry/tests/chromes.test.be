# chromes.test.be — each chrome draws with live values and saves a frame, so a chrome can be seen
# without driving a whole page. Frames land in tools/berry/artifacts/components/.

suite = "chromes"

def save(name)
  return harness.save("tools/berry/artifacts/components/" + name + ".bmp")
end

def readout_draws()
  screen.draw_clear()
  ReadoutChrome(Position.top_left(), Size.page()).draw(50.0)
  assert_true(save("readout"), "cannot save the readout frame")
end

test("the readout chrome draws", readout_draws)

def static_draws()
  screen.draw_clear()
  RampedStaticChrome("STATIC", Position.top_left(), Size.page()).draw("READY", 300000, 50000)
  assert_true(save("static"), "cannot save the ramped static frame")
end

test("the ramped static chrome draws", static_draws)

def interval_draws()
  screen.draw_clear()
  var chrome = RampedIntervalChrome(IntervalIcon(Position.top_left(), Size(12, 8)), "INTERVAL",
                                    Position.top_left(), Size.page())
  chrome.draw(2, 4, "READY", 50)
  assert_true(save("interval"), "cannot save the ramped interval frame")
  assert_eq(chrome.sets.value, "2 / 4", "the chrome does not count the set under way")

  # A ready page has no set to count, so it shows what the run will make.
  chrome.draw(0, 4, "READY", 50)
  assert_eq(chrome.sets.value, "4 sets", "a ready chrome does not show the run's sets")
  chrome.draw(0, 1, "READY", 50)
  assert_eq(chrome.sets.value, "1 set", "a single set is not singular")
end

test("the ramped interval chrome draws", interval_draws)
