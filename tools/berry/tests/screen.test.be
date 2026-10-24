# screen.test.be — the screen module's size calls, so a layout can derive its boxes from the panel
# rather than a fixed number.

suite = "screen"

def reports_its_size()
  assert_eq(screen.width(), 128, "screen.width() is not the panel width")
  assert_eq(screen.height(), 64, "screen.height() is not the panel height")
end

test("the screen reports its size", reports_its_size)
