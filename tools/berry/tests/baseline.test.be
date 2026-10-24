# baseline.test.be — the v1 components, driven directly: what each one does now, asserted and
# captured, so the v2 replacements have a record to compare against. Frames land in
# tools/berry/artifacts/baseline/.

suite = "baseline"

def capture(name)
  return harness.save("tools/berry/artifacts/baseline/" + name + ".bmp")
end

def text_fits()
  var t = Text()
  t.x = 0; t.y = 0; t.w = 64; t.h = 52
  var c = t.fit("42")
  assert_true(c != nil && c.size() == 2, "v1 text fit does not answer a face pair")

  screen.draw_clear()
  var d = DynamicText()
  d.x = 0; d.y = 0; d.w = 64; d.h = 16
  d.align = "left"
  d.draw("12.5")
  assert_true(capture("text"), "cannot save baseline text")
end

test("v1 text fits and draws", text_fits)

def fixed_text_draws()
  screen.draw_clear()
  var t = FixedText()
  t.x = 0; t.y = 0; t.w = 64; t.h = 16
  t.value = "READY"
  t.draw()
  assert_true(capture("fixed-text"), "cannot save baseline fixed text")
end

test("v1 fixed text draws", fixed_text_draws)

def pressure_draws()
  screen.draw_clear()
  var p = PressureDisplay()
  p.x = 0; p.y = 0; p.w = 64; p.h = 52
  p.draw(20.0)
  assert_true(capture("pressure"), "cannot save baseline pressure")
end

test("v1 pressure display draws", pressure_draws)

def bar_draws()
  screen.draw_clear()
  var b = ProgressBar()
  b.x = 0; b.y = 0; b.w = 96; b.h = 8
  b.border = 1
  b.draw(50)
  assert_true(capture("bar"), "cannot save baseline bar")
end

test("v1 progress bar draws", bar_draws)

def bubbles_layout()
  var b = ProgressBubbles()
  b.x = 0; b.y = 0; b.w = 64; b.h = 38
  var counts = [4, 12, 20]
  for n : counts
    b.max_count = n
    b.layout = nil
    assert_eq(b.compute().size(), n, "v1 bubbles do not lay out one bubble per set")
  end

  screen.draw_clear()
  b.max_count = 12
  b.draw(5)
  assert_true(capture("bubbles"), "cannot save baseline bubbles")
end

test("v1 progress bubbles lay out and draw", bubbles_layout)

def scrollbar_draws()
  screen.draw_clear()
  var s = Scrollbar()
  s.x = 0; s.y = 0; s.w = 2; s.h = 48
  s.draw(2, 10, 5)
  assert_true(capture("scrollbar"), "cannot save baseline scrollbar")
end

test("v1 scrollbar draws", scrollbar_draws)

def triangle_draws()
  screen.draw_clear()
  var t = Triangle()
  t.x = 0; t.y = 0; t.w = 12; t.h = 12
  t.direction = "up"
  t.draw()
  assert_true(capture("triangle"), "cannot save baseline triangle")
end

test("v1 triangle draws", triangle_draws)

def icon_draws()
  screen.draw_clear()
  var i = RampedStaticIcon()
  assert_eq(i.w, 12, "v1 icon has no intrinsic width")
  assert_eq(i.h, 8, "v1 icon has no intrinsic height")
  i.draw(0, 0)
  assert_true(capture("icon"), "cannot save baseline icon")
end

test("v1 icon has an intrinsic size and draws", icon_draws)
