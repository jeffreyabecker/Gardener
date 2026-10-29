# screen.test.be — the drawing module a page describes its tree with: the node a call answers, the ink a
# string takes, the string a clock reads and the glyph a call refuses. These are answers across the Berry
# boundary, so tools/unit cannot see them: the module needs the interpreter to exist at all.

suite = "screen"

def a_node_is_the_kind_it_names_with_its_fields()
  var node = screen.text(nil, nil, nil, nil, "12.5")
  assert_eq(node.kind, "text", "screen.text does not answer a text node")
  assert_eq(node.value, "12.5", "the text node does not carry its string")
  assert_nil(node.x, "a node that names no origin carries a box")
  assert_nil(node.w, "a node that names no size carries a box")

  var boxed = screen.inverted(screen.text(3, 4, 20, 8, "hi"))
  assert_eq(boxed.kind, "inverted", "screen.inverted does not answer an inverted node")
  assert_eq(boxed.child.kind, "text", "the inverted node does not hold the draw it flips")
  # The flip is the whole of what the call adds: the box the draw names stays the draw's own.
  assert_eq(boxed.child.x, 3, "the box the call was given is not the node's")
  assert_eq(boxed.child.h, 8, "the box the call was given is not the node's")
end

test("a node is the kind it names with its fields", a_node_is_the_kind_it_names_with_its_fields)

def measure_answers_the_ink_a_string_takes()
  var short = screen.measure("1")
  var long = screen.measure("12.5")
  assert_true(short.w > 0, "a measured string takes no ink")
  assert_true(long.w > short.w, "a longer string does not measure wider")
  assert_true(long.h > 0, "a measured string has no height")

  # The face and the scale are the measurer's, as they are the renderer's: twice the scale is twice the
  # ink, and a face that is twice as large shows in the same call.
  var scaled = screen.measure("12.5", nil, 2)
  assert_eq(scaled.w, long.w * 2, "a doubled scale does not measure twice as wide")
end

test("measure answers the ink a string takes", measure_answers_the_ink_a_string_takes)

def duration_text_answers_the_clock_a_page_reads()
  assert_eq(screen.duration_text("minutes", 65), screen.duration_text("minutes", 65),
            "the same clock reads two strings")
  assert_eq(screen.duration_text("minutes", 0, false), "--",
            "a clock that is not running does not read --")
  assert_true(screen.duration_text("minutes", 65) != screen.duration_text("minutes", 66),
              "two clocks read the same string")
end

test("duration_text answers the clock a page reads", duration_text_answers_the_clock_a_page_reads)

def icon_refuses_a_string_that_is_not_a_bitmap()
  var good = "0c080000f3c012401240124012401e700000"
  var node = screen.icon(nil, nil, nil, nil, good)
  assert_eq(node.kind, "icon", "screen.icon does not answer an icon node")
  assert_eq(node.glyph, good, "the icon node does not carry its bitmap")

  var refused = false
  try
    screen.icon(nil, nil, nil, nil, "not a bitmap")
  except .. as kind, message
    refused = true
    assert_eq(kind, "value_error", "a bad bitmap is refused as " + kind)
  end
  assert_true(refused, "a string that is not a bitmap was drawn as nothing")
end

test("icon refuses a string that is not a bitmap", icon_refuses_a_string_that_is_not_a_bitmap)
