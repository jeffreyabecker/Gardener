# components.test.be — the components, each asserted at the level the item that adds it names.

suite = "components"

def text_fits_compiled_first()
  var t = Text("")
  t.set_box(point(0, 0), bounds(64, 52))
  assert_eq(t.fit("42", 64, 52)[0], "sans24", "a short string in a tall box is not the largest face")
  assert_nil(t.fit("42", 20, 8)[0], "a tiny box does not fall back to the built-in face")
end

test("text fits the compiled faces first", text_fits_compiled_first)

def text_truncates_with_an_ellipsis()
  var t = Text("")
  t.set_box(point(0, 0), bounds(40, 8))
  t.candidate = t.fit("ABCDEFGH", 40, 8)
  var cut = t._truncated("ABCDEFGH", 40)
  var b = screen.text_bounds(cut, 1)
  assert_true(b.w <= 40, "the truncated string does not fit the box")
  assert_true(string.endswith(cut, "..."), "the truncated string has no ellipsis")

  t.dim = bounds(64, 8)
  t.candidate = t.fit("READY", 64, 8)
  assert_eq(t._truncated("READY", 64), "READY", "a string that fits is truncated")
end

test("text truncates below the built-in face", text_truncates_with_an_ellipsis)

def bold_builtin_draws_and_inverts()
  var t = Text("ABC")
  t.set_box(point(0, 0), bounds(20, 8))
  t.style = "bold"
  t.draw()
  assert_nil(t.candidate[0], "a bold tiny box does not use the built-in face")
  t.style = "none"
  t.draw()
  assert_nil(t.candidate[0], "a plain tiny box does not use the built-in face")
end

test("bold built-in text draws and inverts", bold_builtin_draws_and_inverts)

# SysText is the system face whatever the box could hold, and it is not a Text: there is no face to
# fit and none to cache, which is what a label row and a hint row need. Its scale is the system size
# unless it is given another.
def system_text_ignores_the_box()
  var box = bounds(120, 40)
  var fitted = Text("SYS")
  fitted.set_box(point(0, 0), box)
  fitted.draw()
  assert_true(fitted.candidate[0] != nil, "a text in a large box did not fit a compiled face")

  var system = SysText("SYS")
  system.set_box(point(0, 0), box)
  system.draw()
  assert_true(!isinstance(system, Text), "the system text is a Text rather than a component")
  assert_eq(system.scale, 1, "a system text does not default to the system size")
  assert_eq(SysText("SYS", 2).scale, 2, "a system text does not keep the scale it was given")

  var large = SysText("SYS", 2)
  large.set_box(point(0, 0), bounds(128, 32))
  large.draw()
end

test("system text keeps the system face", system_text_ignores_the_box)

# The layout's behaviour — resolving a child's bounds, stacking the siblings and keeping the box a
# component was given — is pinned in layout.test.be, so the layout cases live there and not here.

def progress_bar_draws()
  var b = ProgressBar(0)
  b.set_box(point(0, 0), bounds(32, 8))
  b.value = 0
  b.draw()
  b.value = 50
  b.draw()
  b.value = 100
  b.draw()
  b.value = 150
  b.draw()
end

test("a progress bar draws at its ends", progress_bar_draws)

def scrollbar_draws()
  var s = Scrollbar()
  s.set_box(point(0, 0), bounds(2, 48))
  s.index = 0; s.count = 10; s.visible = 5
  s.draw()
  s.index = 5
  s.draw()
  s.hollow = true
  s.index = 2
  s.draw()
end

test("a scrollbar draws its thumb", scrollbar_draws)

def pressure_display_draws()
  var p = PressureDisplay()
  p.set_box(point(0, 0), bounds(64, 52))
  p.draw()
  p.pressure_kpa = 20.0
  p.draw()
  p.pressure_kpa = 0.0
  p.draw()
end

test("a pressure display draws a reading and dashes", pressure_display_draws)

def pressure_display_secondary()
  var p = PressureDisplay()
  p.set_box(point(0, 0), bounds(64, 52))
  p.pressure_kpa = 20.0
  p.secondary_pressure = 10.0
  p.secondary_position = "br"
  p.size_hint = 0.5
  p.draw()
  assert_eq(p._corner.alignment.align, "right", "a br secondary is not right-aligned")
  assert_eq(p._corner.alignment.v_align, "bottom", "a br secondary is not bottom-aligned")

  p.size_hint = 1
  p.secondary_position = "tr"
  p.draw()
  assert_eq(p._left.pos.x, 0, "the left half is not at the left")
  assert_eq(p._left.dim.w, 32, "the left half is not half the box")
  assert_eq(p._left.children[0], p._number, "a right secondary does not put the main on the left")
  assert_eq(p._right.pos.x, 32, "the right half does not follow the left")
  assert_eq(p._right.children[0], p._secondary, "the secondary is not in the right half")
end

test("a pressure display places its secondary reading", pressure_display_secondary)

def duration_formats()
  var d = Duration("minutes")
  d.set_box(point(0, 0), bounds(64, 16))
  assert_eq(d.text(45), "45s", "minutes: under a minute")
  assert_eq(d.text(60), "1m", "minutes: a whole minute")
  assert_eq(d.text(150), "2m30s", "minutes: minutes and seconds")
  d.format = "seconds"
  assert_eq(d.text(150), "150s", "seconds: seconds only")
  d.format = "hours"
  assert_eq(d.text(3725), "1h2m5s", "hours: hours minutes seconds")
  assert_eq(d.text(125), "2m5s", "hours: minutes seconds")
  assert_eq(d.text(0), "0s", "zero is not 0s")
end

test("a duration formats each unit", duration_formats)

def secondary_in_each_corner()
  var p = PressureDisplay()
  p.set_box(point(0, 0), bounds(64, 52))
  p.pressure_kpa = 20.0
  p.secondary_pressure = 10.0
  p.size_hint = 0.5
  for pos : ["tl", "tr", "bl", "br"]
    p.secondary_position = pos
    p.draw()
    var side = pos == "tl" || pos == "bl" ? "left" : "right"
    var row = pos == "tl" || pos == "tr" ? "top" : "bottom"
    assert_eq(p._corner.alignment.align, side, "the secondary is on the wrong side")
    assert_eq(p._corner.alignment.v_align, row, "the secondary is on the wrong row")
    assert_true(p._secondary.pos.x >= p.pos.x, "the secondary starts left of the display")
    assert_true(p._secondary.pos.y >= p.pos.y, "the secondary starts above the display")
  end
end

test("a pressure display puts the secondary in each corner", secondary_in_each_corner)

def capture(name)
  return harness.save("tools/berry/artifacts/components/" + name + ".gif")
end

def captures()
  screen.draw_clear()
  var t = Text("12.5")
  t.set_box(point(0, 0), bounds(64, 16))
  t.draw()
  capture("text")

  screen.draw_clear()
  var p = PressureDisplay()
  p.set_box(point(0, 0), bounds(64, 52))
  p.pressure_kpa = 20.0
  p.secondary_pressure = 10.0
  p.draw()
  capture("pressure")
end

test("the components capture their frames", captures)
