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
  RampedIntervalChrome(IntervalIcon(Position.top_left(), Size(12, 8)), "INTERVAL", 4,
                       Position.top_left(), Size.page()).draw(2, "READY", 50)
  assert_true(save("interval"), "cannot save the ramped interval frame")
end

test("the ramped interval chrome draws", interval_draws)
