# settings.test.be — what the settings model promises: a default when nothing has been stored, a
# value held to its kind on the way in, and a set the settings scene repairs rather than hands out
# broken. The shipped routines exercise int, pressure, duration and choice; bool, real and every
# value past a bound are only covered here.

suite = "settings"

def missing_keys_read_their_default()
  var editor = IntEditor(1, 1, 20, false)
  assert_eq(editor.read("settings_test", "no_such_key", 4), 4, "a missing int is not its default")

  var choice = ChoiceEditor(["a", "b"], false)
  assert_eq(choice.read("settings_test", "no_choice", "b"), "b", "a missing choice is not its default")
end

test("a missing key reads as the setting's default", missing_keys_read_their_default)

def stored_values_are_held_to_their_kind()
  store.set("settings_test", "count", "99")
  var editor = IntEditor(1, 1, 20, false)
  assert_eq(editor.read("settings_test", "count", 4), 20, "a stored int above max is not held to it")

  store.set("settings_test", "mode", "sideways")
  var choice = ChoiceEditor(["off", "on"], false)
  assert_eq(choice.read("settings_test", "mode", "off"), "off", "a value outside the choices is not defaulted")

  var real = RealEditor(0.5, 0.0, 1.0, false)
  store.set("settings_test", "gain", "7.5")
  assert_eq(real.read("settings_test", "gain", 0.0), 1.0, "a stored real above max is not held to it")
end

test("a stored value is held to its kind", stored_values_are_held_to_their_kind)

def nothing_reads_as_nil_when_nullable()
  store.set("settings_test", "empty", "")
  var editor = IntEditor(1, 0, 10, true)
  assert_nil(editor.read("settings_test", "empty", 5), "an empty nullable int is not nil")
  assert_nil(editor.read("settings_test", "absent", 5), "an absent nullable int is not nil")

  store.set("settings_test", "zero", "0")
  assert_eq(editor.read("settings_test", "zero", 5), 0, "a stored zero is not a value")
end

test("a nullable key that holds nothing reads nil, not its default", nothing_reads_as_nil_when_nullable)

def a_switch_map_is_held_too()
  var s = Activation("interval", {"set_count": 99, "resume_behavior": "sometimes"})
  assert_eq(s.value("set_count"), 20, "the map's set count is not held to its bounds")
  assert_eq(s.value("resume_behavior"), "immediately", "the map's choice is not defaulted")

  var missing = Activation("interval", {"set_count": nil})
  assert_eq(missing.value("set_count"), 4, "a key missing from the map does not read its default")
  assert_eq(missing.value("work_duration"), 30, "a key missing from the map does not read its default")
end

test("a switch map is held to the kinds like a file", a_switch_map_is_held_too)

def the_rules_say_no()
  assert_eq(RampedIntervalSettings("x", nil).problem(
                {"initial_pressure": 10.0, "final_pressure": 5.0}),
            "FINAL < START", "the ladder rule does not refuse a final below the start")
  assert_nil(RampedIntervalSettings("x", nil).problem(
                {"initial_pressure": 5.0, "final_pressure": 10.0}),
            "the ladder rule refuses a valid pair")

  assert_eq(RampedStaticSettings("x", nil).problem(
                {"initial_duration": 60, "target_duration": 60, "total_duration": 100}),
            "TOTAL < HOLDS", "the profile rule does not refuse a total below the holds")
end

test("a routine's rule refuses a set that cannot run", the_rules_say_no)

def settings_note(target, key, value)
  target[key] = value
end

def a_repair_puts_a_set_back()
  var wrote = {}
  var scene = RampedStaticSettings("settings_test", / key, value -> settings_note(wrote, key, value))
  assert_true(scene.open(), "the settings scene did not open")
  assert_nil(scene.message, "a set of defaults was refused")

  # A total below the two holds is what lowering Total under a longer hold pair produces.
  var at = scene.index_of(scene.items[4])
  assert_true(at > 0, "the settings scene does not know its own rows")
  scene.values[at] = 30
  scene.repair(at)
  assert_eq(scene.message, "TOTAL < HOLDS", "the repair did not report the rule it applied")
  assert_eq(scene.values[at], scene.items[at].default, "the repair did not put the value back")
  assert_eq(wrote[scene.items[at].key], scene.items[at].default, "the repair was not written")
end

test("a repair puts a refused set back to its defaults", a_repair_puts_a_set_back)
