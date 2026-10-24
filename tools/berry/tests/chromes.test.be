# chromes.test.be — each routine chrome as it draws today and a v2 reconstruction, side by side, so
# the two can be compared. Pixel-perfect reproduction is a non-goal. Frames land in
# tools/berry/artifacts/v2/.

suite = "chromes"

def save_v1(name)
  return harness.save("tools/berry/artifacts/v2/" + name + "-v1.bmp")
end

def save_v2(name)
  return harness.save("tools/berry/artifacts/v2/" + name + "-v2.bmp")
end

def readout_comparison()
  var s = vacuum.state()

  screen.draw_clear()
  __rootScene.chrome.draw(s)
  assert_true(save_v1("readout"), "cannot save the readout's v1 frame")

  screen.draw_clear()
  var row = LayoutV2()
  row.x = 0; row.y = 0; row.w = 128; row.h = 52
  var pressure = PressureDisplayV2()
  pressure.grow = true
  var target = PressureDisplayV2()
  target.grow = true
  row.add(pressure); row.add(target)
  row.place()
  pressure.draw(s.sensor_ok ? s.pressure_kpa : nil)
  target.draw(s.target > 0.0 ? s.target : nil)
  assert_true(save_v2("readout"), "cannot save the readout's v2 frame")
end

test("the readout renders both frameworks", readout_comparison)

def static_comparison()
  var s = vacuum.state()

  screen.draw_clear()
  var page = RampedStatic()
  page.chrome.draw(s)
  assert_true(save_v1("static"), "cannot save the ramped static v1 frame")

  screen.draw_clear()
  var pressure = PressureDisplayV2()
  pressure.x = 0; pressure.y = 8; pressure.w = 64; pressure.h = 32
  var row = LayoutV2()
  row.x = 0; row.y = 44; row.w = 128; row.h = 8
  var status = TextV2()
  status.value = "READY"
  status.align = "left"
  var bar = ProgressBarV2()
  bar.grow = true
  row.add(status); row.add(bar)
  row.place()
  pressure.draw(s.sensor_ok ? s.pressure_kpa : nil)
  status.draw("READY")
  bar.draw(50)
  assert_true(save_v2("static"), "cannot save the ramped static v2 frame")
end

test("the ramped static chrome renders both frameworks", static_comparison)

def interval_comparison()
  var s = vacuum.state()

  screen.draw_clear()
  var page = RampedInterval()
  page.chrome.draw(s)
  assert_true(save_v1("interval"), "cannot save the ramped interval v1 frame")

  screen.draw_clear()
  var pressure = PressureDisplayV2()
  pressure.x = 0; pressure.y = 8; pressure.w = 64; pressure.h = 32
  var bubbles = ProgressBubblesV2()
  bubbles.x = 64; bubbles.y = 0; bubbles.w = 64; bubbles.h = 38
  bubbles.set_count(4)
  var row = LayoutV2()
  row.x = 0; row.y = 44; row.w = 128; row.h = 8
  var status = TextV2()
  status.value = "READY"
  status.align = "left"
  var bar = ProgressBarV2()
  bar.grow = true
  row.add(status); row.add(bar)
  row.place()
  pressure.draw(s.sensor_ok ? s.pressure_kpa : nil)
  bubbles.draw(0)
  status.draw("READY")
  bar.draw(50)
  assert_true(save_v2("interval"), "cannot save the ramped interval v2 frame")
end

test("the ramped interval chrome renders both frameworks", interval_comparison)
