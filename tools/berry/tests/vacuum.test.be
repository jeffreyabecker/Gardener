# vacuum.test.be — the module's own answers: the bounds a target is held to before it is posted, the
# clamp a page steps within, and the reading they meet. These are answers across the Berry boundary,
# so tools/unit cannot see them: the module needs the interpreter to exist at all.

suite = "vacuum"

def a_target_past_the_floor_is_held_at_it()
  # A page has no channel to be refused on, so a depth the pump cannot reach lands on the deepest
  # one rather than going quiet and leaving the machine on its last target.
  vacuum.set_target_pressure(vacuum.get_deepest_pressure() + 10.0)
  harness.beat(1)
  assert_eq(vacuum.get_target_pressure(), vacuum.get_deepest_pressure(),
            "a target past the floor was not held at it")
end

test("a target past the floor is held at it", a_target_past_the_floor_is_held_at_it)

def a_target_below_ambient_is_the_release()
  # Below ambient is not a depth at all: it is the ambient target, which vents the chamber.
  vacuum.set_target_pressure(-5.0)
  harness.beat(1)
  assert_eq(vacuum.get_target_pressure(), 0.0, "a target below ambient is not the release")
end

test("a target below ambient is the release", a_target_below_ambient_is_the_release)

def a_target_within_the_bounds_is_taken_as_it_stands()
  # The bounds hold a value, they do not move one: what the editor can ask for arrives unchanged.
  vacuum.set_target_pressure(20.0)
  harness.beat(1)
  assert_eq(vacuum.get_target_pressure(), 20.0, "a target within the bounds did not arrive")
end

test("a target within the bounds is taken as it stands",
     a_target_within_the_bounds_is_taken_as_it_stands)

def the_clamp_is_the_editors_bounds_not_the_floor()
  # vacuum.clamp is what a page steps within, so the value it answers is off the ambient target even
  # when nothing was asked for — which is why set_target_pressure does not clamp its floor to it.
  assert_eq(vacuum.clamp(0.0), vacuum.get_shallowest_pressure(),
            "the clamp does not lift nothing to the shallowest target")
  assert_eq(vacuum.clamp(vacuum.get_deepest_pressure() + 10.0), vacuum.get_deepest_pressure(),
            "the clamp does not hold a target at the floor")
end

test("the clamp is the editor's bounds, not the floor",
     the_clamp_is_the_editors_bounds_not_the_floor)
