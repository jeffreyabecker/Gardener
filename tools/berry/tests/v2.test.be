# v2.test.be — the v2 components, each asserted at the level the item that adds it names.

suite = "v2"

def text_fits_compiled_first()
  var t = TextV2()
  t.w = 64; t.h = 52
  assert_eq(t._fit("42")[0], "sans24", "a short string in a tall box is not the largest face")
  t.w = 20; t.h = 8
  assert_nil(t._fit("42")[0], "a tiny box does not fall back to the built-in face")
end

test("text fits the compiled faces first", text_fits_compiled_first)

def text_truncates_with_an_ellipsis()
  var t = TextV2()
  t.w = 40; t.h = 8
  t.candidate = t._fit("ABCDEFGH")
  var cut = t._truncated("ABCDEFGH")
  var b = screen.text_bounds(cut, 1)
  assert_true(b.w <= 40, "the truncated string does not fit the box")
  assert_true(string.endswith(cut, "..."), "the truncated string has no ellipsis")

  t.w = 64
  t.candidate = t._fit("READY")
  assert_eq(t._truncated("READY"), "READY", "a string that fits is truncated")
end

test("text truncates below the built-in face", text_truncates_with_an_ellipsis)

def bold_builtin_draws_and_inverts()
  var t = TextV2()
  t.w = 20; t.h = 8
  t.style = "bold"
  t.draw("ABC")
  assert_nil(t.candidate[0], "a bold tiny box does not use the built-in face")
  t.style = "none"
  t.draw("ABC")
  assert_nil(t.candidate[0], "a plain tiny box does not use the built-in face")
end

test("bold built-in text draws and inverts", bold_builtin_draws_and_inverts)

def layout_places_measured_children()
  var l = LayoutV2()
  l.x = 0; l.y = 0; l.w = 64; l.h = 8
  l.spacing = 2
  var a = TextV2()
  a.value = "AB"
  var b = TextV2()
  b.value = "CD"
  l.add(a); l.add(b)
  l.place()
  assert_eq(a.x, 0, "the first child is not at the start")
  assert_eq(a.w, 12, "the first child is not its measured width")
  assert_eq(b.x, 14, "the second child is not after the first plus spacing")
end

test("a layout places measured children", layout_places_measured_children)

def layout_shares_growing_children()
  var l = LayoutV2()
  l.x = 0; l.y = 0; l.w = 64; l.h = 8
  var a = TextV2()
  a.value = "AB"
  var b = TextV2()
  b.value = "CD"
  b.grow = true
  l.add(a); l.add(b)
  l.place()
  assert_eq(a.w, 12, "the measured child is not its width")
  assert_eq(b.x, 12, "the growing child is not after the measured one")
  assert_eq(b.w, 52, "the growing child does not take the remainder")
  assert_eq(a.h, 8, "a child is not stretched to the cross size")
  assert_eq(b.h, 8, "a child is not stretched to the cross size")
end

test("a layout shares the remainder among growing children", layout_shares_growing_children)

def layout_makes_equal_columns()
  var l = LayoutV2()
  l.x = 0; l.y = 0; l.w = 64; l.h = 8
  var a = TextV2()
  a.grow = true
  var b = TextV2()
  b.grow = true
  l.add(a); l.add(b)
  l.place()
  assert_eq(a.w, 32, "the first growing child is not half")
  assert_eq(b.x, 32, "the second growing child is not after the first")
  assert_eq(b.w, 32, "the second growing child is not half")
end

test("two growing children make equal columns", layout_makes_equal_columns)

def layout_wraps_children()
  var l = LayoutV2()
  l.x = 0; l.y = 0; l.w = 24; l.h = 8
  l.wrap = true
  var a = TextV2(); a.value = "AB"
  var b = TextV2(); b.value = "AB"
  var c = TextV2(); c.value = "AB"
  l.add(a); l.add(b); l.add(c)
  l.place()
  assert_eq(a.x, 0, "the first child is not at the line start")
  assert_eq(b.x, 12, "the second child is not beside the first")
  assert_eq(c.x, 0, "the third child did not wrap to a new line")
  assert_eq(c.y, 8, "the second line is not below the first line's height")
  assert_eq(a.h, 8, "a child is not stretched to its line's height")
end

test("a wrapping layout carries children onto the next line", layout_wraps_children)

def layout_fills_right_to_left()
  var l = LayoutV2()
  l.x = 0; l.y = 0; l.w = 24; l.h = 8
  l.fill_direction = "rl-tb"
  var a = TextV2(); a.value = "AB"
  var b = TextV2(); b.value = "AB"
  l.add(a); l.add(b)
  l.place()
  assert_eq(a.x, 12, "the first child is not at the right")
  assert_eq(b.x, 0, "the second child is not left of the first")
end

test("a layout fills right to left", layout_fills_right_to_left)

def layout_fills_columns()
  var l = LayoutV2()
  l.x = 0; l.y = 0; l.w = 8; l.h = 16
  l.fill_direction = "tb-lr"
  var a = TextV2(); a.value = "A"
  var b = TextV2(); b.value = "A"
  l.add(a); l.add(b)
  l.place()
  assert_eq(a.y, 0, "the first child is not at the top")
  assert_eq(b.y, 8, "the second child is not below the first")

  var r = LayoutV2()
  r.x = 0; r.y = 0; r.w = 8; r.h = 16
  r.fill_direction = "bt-lr"
  r.add(a); r.add(b)
  r.place()
  assert_eq(a.y, 8, "the first child is not at the bottom")
  assert_eq(b.y, 0, "the second child is not above the first")
end

test("a layout fills columns top to bottom and bottom to top", layout_fills_columns)

def layout_measures_its_natural_size()
  var inner = LayoutV2()
  var a = TextV2(); a.value = "AB"
  var b = TextV2(); b.value = "AB"
  inner.add(a); inner.add(b)
  var m = inner.measure(8)
  assert_eq(m[0], 24, "the nested layout's natural width is not its children's sum")
  assert_eq(m[1], 8, "the nested layout's natural height is not its tallest child")
end

test("a layout reports its natural size", layout_measures_its_natural_size)

def bubble_draws_each_shape()
  var shapes = ["circle", "rect", "roundrect", "tri-up", "tri-down", "tri-left", "tri-right"]
  var b = BubbleV2()
  b.x = 0; b.y = 0; b.w = 8; b.h = 8
  for s : shapes
    b.shape = s
    b.filled = true
    b.draw()
    b.filled = false
    b.draw()
  end
  b.w = 2; b.h = 2
  b.filled = true
  b.draw()
end

test("a bubble draws each shape and the degenerate square", bubble_draws_each_shape)

def bubbles_fill_by_count()
  var p = ProgressBubblesV2()
  p.x = 0; p.y = 0; p.w = 64; p.h = 38
  p.set_count(4)
  p.draw(2)
  assert_eq(p.children.size(), 4, "the field does not have one bubble per set")
  assert_eq(p.children[0].w, 19, "a bubble is not sized to fill the box")
  assert_true(p.children[0].filled, "the first bubble is not filled")
  assert_true(p.children[1].filled, "the second bubble is not filled")
  assert_true(!p.children[2].filled, "the third bubble is filled")
end

test("progress bubbles fill by count", bubbles_fill_by_count)

def progress_bar_draws()
  var b = ProgressBarV2()
  b.x = 0; b.y = 0; b.w = 32; b.h = 8
  b.draw(0)
  b.draw(50)
  b.draw(100)
  b.draw(150)
end

test("a progress bar draws at its ends", progress_bar_draws)

def scrollbar_draws()
  var s = ScrollbarV2()
  s.x = 0; s.y = 0; s.w = 2; s.h = 48
  s.draw(0, 10, 5)
  s.draw(5, 10, 5)
  s.hollow = true
  s.draw(2, 10, 5)
end

test("a scrollbar draws its thumb", scrollbar_draws)

def triangle_draws()
  var t = TriangleV2()
  t.x = 0; t.y = 0; t.w = 8; t.h = 8
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
  var p = PressureDisplayV2()
  p.x = 0; p.y = 0; p.w = 64; p.h = 52
  p.draw(nil)
  p.draw(20.0)
  p.draw(0.0)
end

test("a pressure display draws a reading and dashes", pressure_display_draws)

def pressure_display_secondary()
  var p = PressureDisplayV2()
  p.x = 0; p.y = 0; p.w = 64; p.h = 52
  p.secondary_pressure = 10.0
  p.secondary_position = "br"
  p.size_hint = 0.5
  p.draw(20.0)
  assert_eq(p.secondary.align, "right", "a br secondary is not right-aligned")
  assert_eq(p.secondary.v_align, "bottom", "a br secondary is not bottom-aligned")

  p.size_hint = 1
  p.secondary_position = "tr"
  p.draw(20.0)
  assert_eq(p.number.x, 0, "the main is not in the left half")
  assert_eq(p.secondary.x, 32, "a right secondary is not in the right half")
end

test("a pressure display places its secondary reading", pressure_display_secondary)

def duration_formats()
  var d = DurationV2()
  d.format = "minutes"
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
  var p = PressureDisplayV2()
  p.x = 0; p.y = 0; p.w = 64; p.h = 52
  p.secondary_pressure = 10.0
  p.size_hint = 0.5
  for pos : ["tl", "tr", "bl", "br"]
    p.secondary_position = pos
    p.draw(20.0)
    var side = pos == "tl" || pos == "bl" ? "left" : "right"
    var row = pos == "tl" || pos == "tr" ? "top" : "bottom"
    assert_eq(p.secondary.align, side, "the secondary is on the wrong side")
    assert_eq(p.secondary.v_align, row, "the secondary is on the wrong row")
  end
end

test("a pressure display puts the secondary in each corner", secondary_in_each_corner)

def bubbles_at_every_count()
  var p = ProgressBubblesV2()
  p.x = 0; p.y = 0; p.w = 64; p.h = 38
  var n = 1
  while n <= 20
    p.set_count(n)
    p.draw(n)
    assert_eq(p.children.size(), n, "the field does not rebuild for the count")
    assert_true(p.children[0].w > 0, "a bubble has no size")
    n += 1
  end
end

test("progress bubbles fill every count", bubbles_at_every_count)

def capture_v2(name)
  return harness.save("tools/berry/artifacts/v2/" + name + ".bmp")
end

def v2_captures()
  screen.draw_clear()
  var t = TextV2()
  t.x = 0; t.y = 0; t.w = 64; t.h = 16
  t.align = "left"
  t.draw("12.5")
  assert_true(capture_v2("text"), "cannot save v2 text")

  screen.draw_clear()
  var p = PressureDisplayV2()
  p.x = 0; p.y = 0; p.w = 64; p.h = 52
  p.secondary_pressure = 10.0
  p.draw(20.0)
  assert_true(capture_v2("pressure"), "cannot save v2 pressure")

  screen.draw_clear()
  var b = ProgressBubblesV2()
  b.x = 0; b.y = 0; b.w = 64; b.h = 38
  b.set_count(12)
  b.draw(5)
  assert_true(capture_v2("bubbles"), "cannot save v2 bubbles")
end

test("the v2 components capture their frames", v2_captures)
