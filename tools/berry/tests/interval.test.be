# interval.test.be — the interval routine's own page: the declarations it registers, the glyph it draws
# with and the flat ladder it is the one routine allowed to hold. The page it extends is checked in
# ramped_interval.test.be and the cycle it drives in ramped_interval_machine.test.be, so what is here is
# what is the interval's rather than the ramp's.

suite = "interval"

def activated()
  assert_true(harness_activate("interval", nil), "interval: activate refused")
  return ux.top()
end

def the_declarations_are_one_target_not_a_ladder()
  var cfgs = routines.settings_cfgs("interval")
  assert_not_nil(cfgs, "the interval declares nothing")

  var keys = []
  for cfg : cfgs
    keys.push(cfg.key)
  end
  assert_eq(keys.size(), 6, "the interval does not declare six settings")
  assert_eq(keys[0], "set_count", "the first declaration is not the set count")
  assert_eq(keys[1], "target_pressure", "the second declaration is not the target")
  assert_eq(keys[2], "work_duration", "the third declaration is not the work duration")
  assert_eq(keys[3], "rest_pressure", "the fourth declaration is not the rest pressure")
  assert_eq(keys[4], "rest_duration", "the fifth declaration is not the rest duration")
  assert_eq(keys[5], "resume_behavior", "the sixth declaration is not the resume rule")
end

test("the interval declares one target rather than a ladder",
     the_declarations_are_one_target_not_a_ladder)

def the_ladder_is_the_target_repeated()
  var page = activated()
  assert_eq(page.count, routines.load_settings("interval")["set_count"],
            "the page is holding another set count")

  var plan = page.target_list()
  assert_eq(plan.size(), page.count, "the ladder is not one target per set")
  var i = 0
  while i < plan.size()
    assert_eq(plan[i], vacuum.clamp(page.initial), "a set in the flat ladder is another target")
    i += 1
  end
end

test("the ladder is the one target repeated", the_ladder_is_the_target_repeated)

def the_chrome_is_the_ramps_with_its_own_glyph()
  var page = activated()
  var tree = page.draw_page()
  assert_eq(tree.kind, "column", "the interval's page does not draw a column")
  assert_eq(tree.children.size(), 3, "the interval's page is not three rows")

  # The title row the ramp draws, carrying the interval's own glyph and title.
  var top = child_at(tree, 0)
  assert_eq(child_at(top, 0).glyph, interval_glyph, "the interval's page draws another glyph")
  assert_eq(child_at(top, 1).value, routines.title("interval"),
            "the interval's page does not draw its title")

  # A ready page shows the sets it will run rather than a set it is on.
  var sets = child_at(child_at(tree, 1), 1).value
  var expected = page.count == 1 ? "1 set" : str(page.count) + " sets"
  assert_eq(sets, expected, "a ready page does not draw the sets it will run")
end

test("the chrome is the ramp's with the interval's glyph",
     the_chrome_is_the_ramps_with_its_own_glyph)

def the_interval_is_what_the_ramp_is_not()
  # The ramp refuses a ladder it cannot build; the interval's ladder has no step to get wrong.
  var keys = []
  var cfgs = routines.settings_cfgs("interval")
  for cfg : cfgs
    keys.push(cfg.key)
  end
  for key : ["initial_pressure", "final_pressure", "step_pressure"]
    var held = false
    for listed : keys
      if listed == key
        held = true
      end
    end
    assert_true(!held, "the interval declares the ramp's " + key)
  end
end

test("the interval registers none of the ramp's ladder keys", the_interval_is_what_the_ramp_is_not)
