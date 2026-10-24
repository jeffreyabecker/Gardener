# v2.test.be — the v2 components, each asserted at the level the item that adds it names.

suite = "v2"

def text_fits_compiled_first()
  var t = TextV2(Position(0, 0), Size(64, 52), "")
  assert_eq(t._fit("42")[0], "sans24", "a short string in a tall box is not the largest face")
  t.dim = Size(20, 8)
  assert_nil(t._fit("42")[0], "a tiny box does not fall back to the built-in face")
end

test("text fits the compiled faces first", text_fits_compiled_first)

def text_truncates_with_an_ellipsis()
  var t = TextV2(Position(0, 0), Size(40, 8), "")
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
  var t = TextV2(Position(0, 0), Size(20, 8), "ABC")
  t.style = "bold"
  t.draw()
  assert_nil(t.candidate[0], "a bold tiny box does not use the built-in face")
  t.style = "none"
  t.draw()
  assert_nil(t.candidate[0], "a plain tiny box does not use the built-in face")
end

test("bold built-in text draws and inverts", bold_builtin_draws_and_inverts)

# The flat layout's behaviour — drawing its children in order and keeping their boxes — is pinned in
# layoutv2.test.be, so the layout cases live there and not here.

def bubble_draws_each_shape()
  var shapes = ["circle", "rect", "roundrect", "tri-up", "tri-down", "tri-left", "tri-right"]
  var b = BubbleV2(Position(0, 0), Size(8, 8), "circle", "unfilled")
  for s : shapes
    b.shape = s
    b.fill_state = "filled"
    b.draw()
    b.fill_state = "unfilled"
    b.draw()
  end
  b.dim = Size(2, 2)
  b.fill_state = "filled"
  b.draw()
end

test("a bubble draws each shape and the degenerate square", bubble_draws_each_shape)

def bubbles_fill_by_count()
  var p = ProgressBubblesV2(Position(0, 0), Size(64, 38), 4, "circle", "lr-tb")
  p.count = 2
  p.draw()
  assert_eq(p.children.size(), 4, "the field does not have one bubble per set")
  assert_eq(p.children[0].dim.w, 19, "a bubble is not sized to fill the box")
  assert_eq(p.children[0].fill_state, "filled", "the first bubble is not filled")
  assert_eq(p.children[1].fill_state, "filled", "the second bubble is not filled")
  assert_eq(p.children[2].fill_state, "unfilled", "the third bubble is filled")
end

test("progress bubbles fill by count", bubbles_fill_by_count)

def progress_bar_draws()
  var b = ProgressBarV2(Position(0, 0), Size(32, 8), 0)
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
  var s = ScrollbarV2(Position(0, 0), Size(2, 48))
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
  var t = TriangleV2(Position(0, 0), Size(8, 8))
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
  var p = PressureDisplayV2(Position(0, 0), Size(64, 52))
  p.draw(nil)
  p.draw(20.0)
  p.draw(0.0)
end

test("a pressure display draws a reading and dashes", pressure_display_draws)

def pressure_display_secondary()
  var p = PressureDisplayV2(Position(0, 0), Size(64, 52))
  p.secondary_pressure = 10.0
  p.secondary_position = "br"
  p.size_hint = 0.5
  p.draw(20.0)
  assert_eq(p._secondary.align, "right", "a br secondary is not right-aligned")
  assert_eq(p._secondary.v_align, "bottom", "a br secondary is not bottom-aligned")

  p.size_hint = 1
  p.secondary_position = "tr"
  p.draw(20.0)
  assert_eq(p._number.pos.x, 0, "the main is not in the left half")
  assert_eq(p._secondary.pos.x, 32, "a right secondary is not in the right half")
end

test("a pressure display places its secondary reading", pressure_display_secondary)

def duration_formats()
  var d = DurationV2(Position(0, 0), Size(64, 16), "minutes")
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
  var p = PressureDisplayV2(Position(0, 0), Size(64, 52))
  p.secondary_pressure = 10.0
  p.size_hint = 0.5
  for pos : ["tl", "tr", "bl", "br"]
    p.secondary_position = pos
    p.draw(20.0)
    var side = pos == "tl" || pos == "bl" ? "left" : "right"
    var row = pos == "tl" || pos == "tr" ? "top" : "bottom"
    assert_eq(p._secondary.align, side, "the secondary is on the wrong side")
    assert_eq(p._secondary.v_align, row, "the secondary is on the wrong row")
  end
end

test("a pressure display puts the secondary in each corner", secondary_in_each_corner)

def bubbles_at_every_count()
  var p = ProgressBubblesV2(Position(0, 0), Size(64, 38), 1, "circle", "lr-tb")
  var n = 1
  while n <= 20
    p.set_count(n)
    p.count = n
    p.draw()
    assert_eq(p.children.size(), n, "the field does not rebuild for the count")
    assert_true(p.children[0].dim.w > 0, "a bubble has no size")
    n += 1
  end
end

test("progress bubbles fill every count", bubbles_at_every_count)

def capture_v2(name)
  return harness.save("tools/berry/artifacts/v2/" + name + ".bmp")
end

def v2_captures()
  screen.draw_clear()
  var t = TextV2(Position(0, 0), Size(64, 16), "12.5")
  t.align = "left"
  t.draw()
  assert_true(capture_v2("text"), "cannot save v2 text")

  screen.draw_clear()
  var p = PressureDisplayV2(Position(0, 0), Size(64, 52))
  p.secondary_pressure = 10.0
  p.draw(20.0)
  assert_true(capture_v2("pressure"), "cannot save v2 pressure")

  screen.draw_clear()
  var b = ProgressBubblesV2(Position(0, 0), Size(64, 38), 12, "circle", "lr-tb")
  b.count = 5
  b.draw()
  assert_true(capture_v2("bubbles"), "cannot save v2 bubbles")
end

test("the v2 components capture their frames", v2_captures)
