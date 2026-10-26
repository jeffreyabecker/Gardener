# components.test.be — the components, each asserted at the level the item that adds it names.

suite = "components"

def text_fits_compiled_first()
  var t = Text("")
  t.set_box(Position(0, 0), Size(64, 52))
  assert_eq(t._fit("42")[0], "sans24", "a short string in a tall box is not the largest face")
  t.dim = Size(20, 8)
  assert_nil(t._fit("42")[0], "a tiny box does not fall back to the built-in face")
end

test("text fits the compiled faces first", text_fits_compiled_first)

def text_truncates_with_an_ellipsis()
  var t = Text("")
  t.set_box(Position(0, 0), Size(40, 8))
  t.candidate = t._fit("ABCDEFGH")
  var cut = t._truncated("ABCDEFGH")
  var b = screen.text_bounds(cut, 1)
  assert_true(b.w <= 40, "the truncated string does not fit the box")
  assert_true(string.endswith(cut, "..."), "the truncated string has no ellipsis")

  t.dim = Size(64, 8)
  t.candidate = t._fit("READY")
  assert_eq(t._truncated("READY"), "READY", "a string that fits is truncated")
end

test("text truncates below the built-in face", text_truncates_with_an_ellipsis)

def bold_builtin_draws_and_inverts()
  var t = Text("ABC")
  t.set_box(Position(0, 0), Size(20, 8))
  t.style = "bold"
  t.draw()
  assert_nil(t.candidate[0], "a bold tiny box does not use the built-in face")
  t.style = "none"
  t.draw()
  assert_nil(t.candidate[0], "a plain tiny box does not use the built-in face")
end

test("bold built-in text draws and inverts", bold_builtin_draws_and_inverts)

# The layout's behaviour — resolving a child's bounds, stacking the siblings and keeping the box a
# component was given — is pinned in layout.test.be, so the layout cases live there and not here.

def progress_bar_draws()
  var b = ProgressBar(0)
  b.set_box(Position(0, 0), Size(32, 8))
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
  s.set_box(Position(0, 0), Size(2, 48))
  s.index = 0; s.count = 10; s.visible = 5
  s.draw()
  s.index = 5
  s.draw()
  s.hollow = true
  s.index = 2
  s.draw()
end

test("a scrollbar draws its thumb", scrollbar_draws)

def triangle_draws()
  var t = Triangle()
  t.set_box(Position(0, 0), Size(8, 8))
  for d : ["up", "down", "left", "right"]
    t.direction = d
    t.style = "filled"
    t.draw()
    t.style = "empty"
    t.draw()
  end
end

test("a triangle draws each direction", triangle_draws)

def pressure_display_draws()
  var p = PressureDisplay()
  p.set_box(Position(0, 0), Size(64, 52))
  p.draw()
  p.pressure_kpa = 20.0
  p.draw()
  p.pressure_kpa = 0.0
  p.draw()
end

test("a pressure display draws a reading and dashes", pressure_display_draws)

def pressure_display_secondary()
  var p = PressureDisplay()
  p.set_box(Position(0, 0), Size(64, 52))
  p.pressure_kpa = 20.0
  p.secondary_pressure = 10.0
  p.secondary_position = "br"
  p.size_hint = 0.5
  p.draw()
  assert_eq(p._secondary.align, "right", "a br secondary is not right-aligned")
  assert_eq(p._secondary.v_align, "bottom", "a br secondary is not bottom-aligned")

  p.size_hint = 1
  p.secondary_position = "tr"
  p.draw()
  assert_eq(p._number.pos.x, 0, "the main is not in the left half")
  assert_eq(p._secondary.pos.x, 32, "a right secondary is not in the right half")
end

test("a pressure display places its secondary reading", pressure_display_secondary)

def duration_formats()
  var d = Duration("minutes")
  d.set_box(Position(0, 0), Size(64, 16))
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
  p.set_box(Position(0, 0), Size(64, 52))
  p.pressure_kpa = 20.0
  p.secondary_pressure = 10.0
  p.size_hint = 0.5
  for pos : ["tl", "tr", "bl", "br"]
    p.secondary_position = pos
    p.draw()
    var side = pos == "tl" || pos == "bl" ? "left" : "right"
    var row = pos == "tl" || pos == "tr" ? "top" : "bottom"
    assert_eq(p._secondary.align, side, "the secondary is on the wrong side")
    assert_eq(p._secondary.v_align, row, "the secondary is on the wrong row")
  end
end

test("a pressure display puts the secondary in each corner", secondary_in_each_corner)

def capture(name)
  return harness.save("tools/berry/artifacts/components/" + name + ".gif")
end

def captures()
  screen.draw_clear()
  var t = Text("12.5")
  t.set_box(Position(0, 0), Size(64, 16))
  t.align = "left"
  t.draw()
  capture("text")

  screen.draw_clear()
  var p = PressureDisplay()
  p.set_box(Position(0, 0), Size(64, 52))
  p.pressure_kpa = 20.0
  p.secondary_pressure = 10.0
  p.draw()
  capture("pressure")
end

test("the components capture their frames", captures)
