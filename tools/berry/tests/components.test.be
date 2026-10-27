# components.test.be — the components, each asserted at the level the item that adds it names.

suite = "components"

# The face is the caller's: what a text draws at is the name it was given, and the room it reports is
# that face's ink.
def text_draws_at_the_face_it_was_given()
  var t = Text("42", "sans12")
  var room = t.use(64, 52)
  var ink = screen.text_bounds("42", 1, "sans12")
  assert_eq(t.font, "sans12", "the text does not keep the face it was given")
  assert_eq(room.w, ink.w, "the text does not report the ink of its face")
  assert_eq(room.h, ink.h, "the text does not report the ink of its face")
  t.set_box(point(0, 0), bounds(64, 52))
  t.draw()
end

test("a text draws at the face it was given", text_draws_at_the_face_it_was_given)

# Nil, an empty name and "builtin" are the built-in 5x7, the face that carries the arrow glyphs a
# hint prints.
def a_text_with_no_face_is_the_system_face()
  var system = screen.text_bounds("42", 1)
  for font : [nil, "", "builtin"]
    var t = Text("42", font)
    var room = t.use(64, 52)
    assert_eq(room.w, system.w, "a text with no face is not the built-in one")
    assert_eq(room.h, system.h, "a text with no face is not the built-in one")
    t.set_box(point(0, 0), bounds(64, 52))
    t.draw()
  end
end

test("a text with no face is the system face", a_text_with_no_face_is_the_system_face)

# The scale is the size the face is drawn at, and the room a text reports is the ink at that size.
def a_text_scales_the_face_it_draws()
  var plain = Text("42", "sans12")
  var doubled = Text("42", "sans12", 2)
  var one = plain.use(128, 128)
  var two = doubled.use(128, 128)
  assert_eq(doubled.scale, 2, "the text does not keep the scale it was given")
  assert_eq(plain.scale, 1, "a text does not default to the designed size")
  assert_eq(two.h, one.h * 2, "a scale of two is not twice the room")
  assert_true(two.w > one.w, "a scale of two is not wider")
  doubled.set_box(point(0, 0), bounds(128, 128))
  doubled.draw()

  var system = Text("SYS", nil, 2)
  var alone = screen.text_bounds("SYS", 1)
  assert_eq(system.use(128, 128).h, alone.h * 2, "a scaled built-in line is not twice as tall")
  assert_eq(Text("SYS", nil, nil).scale, 1, "a nil scale is not the designed size")
  system.set_box(point(0, 0), bounds(128, 128))
  system.draw()
end

test("a text scales the face it draws", a_text_scales_the_face_it_draws)

# Nothing cuts or wraps a string: the room reported is clamped to the cell, and the string is drawn
# whole, so a caller that names a face too large for its cell sees it spill rather than lose letters.
def a_text_wider_than_its_cell_is_drawn_whole()
  var t = Text("ABCDEFGH", "sans24")
  var whole = t.use(128, 52)
  var clamped = t.use(10, 52)
  assert_eq(clamped.w, 10, "the room a text takes is not clamped to its cell")
  assert_true(whole.w > 10, "the string is not wider than the cell it was clamped to")
  t.set_box(point(0, 0), bounds(10, 52))
  t.draw()
  assert_eq(t.value, "ABCDEFGH", "the string was cut for its cell")
end

test("a text wider than its cell is drawn whole", a_text_wider_than_its_cell_is_drawn_whole)

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
