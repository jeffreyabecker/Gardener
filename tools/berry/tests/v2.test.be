# v2.test.be — the v2 components, each asserted at the level the item that adds it names.

suite = "v2"

def text_fits_compiled_first()
  var t = TextV2()
  t.w = 64; t.h = 52
  assert_eq(t.fit("42")[0], "sans24", "a short string in a tall box is not the largest face")
  t.w = 20; t.h = 8
  assert_nil(t.fit("42")[0], "a tiny box does not fall back to the built-in face")
end

test("text fits the compiled faces first", text_fits_compiled_first)

def text_truncates_with_an_ellipsis()
  var t = TextV2()
  t.w = 40; t.h = 8
  t.candidate = t.fit("ABCDEFGH")
  var cut = t.truncated("ABCDEFGH")
  var b = screen.text_bounds(cut, 1)
  assert_true(b.w <= 40, "the truncated string does not fit the box")
  assert_true(string.endswith(cut, "..."), "the truncated string has no ellipsis")

  t.w = 64
  t.candidate = t.fit("READY")
  assert_eq(t.truncated("READY"), "READY", "a string that fits is truncated")
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
