# chromes.test.be — each routine chrome as it draws today and its v2 twin, side by side, so the
# two can be compared. Pixel-perfect reproduction is a non-goal. Frames land in
# tools/berry/artifacts/v2/.

suite = "chromes"

def save_v1(name)
  return harness.save("tools/berry/artifacts/v2/" + name + "-v1.bmp")
end

def save_v2(name)
  return harness.save("tools/berry/artifacts/v2/" + name + "-v2.bmp")
end

# A page owner with the fields each chrome reads and values that draw everything that can be drawn:
# a run in progress (non-zero time left) and a ready interval with its sets built.
class FakeOwner
  var editing
  var count
  var phase
  var set

  def init()
    self.editing = false
    self.count = 4
    self.phase = IntervalPhase.READY
    self.set = 0
  end

  def status_text()
    return "READY"
  end

  def run_left()
    return 50.0
  end

  def hold_left()
    return 50.0
  end
end

# A reading with a live pressure and a target, so both numbers render.
def fake_reading()
  var s = reading()
  s.pressure_kpa = 20.0
  s.target = 12.0
  s.sensor_ok = true
  return s
end

def readout_comparison()
  var s = fake_reading()
  var owner = FakeOwner()

  screen.draw_clear()
  ReadoutChrome(owner).draw(s)
  assert_true(save_v1("readout"), "cannot save the readout's v1 frame")

  screen.draw_clear()
  ReadoutChromeV2(owner).draw(s)
  assert_true(save_v2("readout"), "cannot save the readout's v2 frame")
end

test("the readout renders both frameworks", readout_comparison)

def static_comparison()
  var s = fake_reading()
  var owner = FakeOwner()

  screen.draw_clear()
  RampedStaticChrome(owner).draw(s)
  assert_true(save_v1("static"), "cannot save the ramped static v1 frame")

  screen.draw_clear()
  RampedStaticChromeV2(owner).draw(s)
  assert_true(save_v2("static"), "cannot save the ramped static v2 frame")
end

test("the ramped static chrome renders both frameworks", static_comparison)

def interval_comparison()
  var s = fake_reading()
  var owner = FakeOwner()

  screen.draw_clear()
  RampedIntervalChrome(owner).draw(s)
  assert_true(save_v1("interval"), "cannot save the ramped interval v1 frame")

  screen.draw_clear()
  RampedIntervalChromeV2(owner).draw(s)
  assert_true(save_v2("interval"), "cannot save the ramped interval v2 frame")
end

test("the ramped interval chrome renders both frameworks", interval_comparison)
