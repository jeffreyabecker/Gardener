# screen.test.be — the screen module's size calls, so a layout can derive its boxes from the panel
# rather than a fixed number, and the colour its drawing calls take.

suite = "screen"

def reports_its_size()
  assert_eq(screen.width(), 128, "screen.width() is not the panel width")
  assert_eq(screen.height(), 64, "screen.height() is not the panel height")
  assert_eq(screen.page_width(), 128, "screen.page_width() is not the panel width")
  assert_eq(screen.page_height(), 64, "screen.page_height() is not the panel height")
  # The band is the ux component's: Size.page() is the area above it.
  assert_eq(Size.page().h, 52, "Size.page() does not leave the band out")
end

test("the screen reports its size", reports_its_size)

# The colour is the last argument, 1 for ink and 0 for paper, and a call without one draws ink. The
# frame is the check: an ink page with paper shapes knocked out of it.
class ColorScene : Scene
  def init()
    super(self).init(Position.top_left(), Size.page())
  end

  def draw_page()
    screen.draw_fillrect(0, 0, screen.page_width(), screen.page_height())
    screen.draw_fillrect(8, 8, 48, 20, 0)
    screen.draw_rect(72, 8, 48, 20, 0)
    screen.draw_fillcircle(96, 36, 6, 0)
    screen.draw_line(8, 38, 56, 38, 0)
  end
end

def a_drawing_call_takes_a_colour()
  var scene = ColorScene()
  assert_true(ux.push(scene), "the colour scene would not push")
  harness.beat(1)
end

test("a drawing call takes a colour", a_drawing_call_takes_a_colour)

def a_third_colour_is_refused()
  var raised = false
  try
    screen.draw_fillrect(0, 0, 1, 1, 2)
  except .. as e, m
    raised = true
  end
  assert_true(raised, "a colour other than 0 or 1 was accepted")
end

test("a colour other than 0 or 1 is refused", a_third_colour_is_refused)
