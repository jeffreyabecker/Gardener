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
