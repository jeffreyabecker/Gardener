# settings.test.be — what the settings model promises: a default when nothing has been stored, a
# value held to its kind on the way in, and a set the settings scene repairs rather than hands out
# broken. The shipped routines exercise int, pressure, duration and choice; bool, real and every
# value past a bound are only covered here.

suite = "settings"

def missing_keys_read_their_default()
  var count = IntSettingCfg("no_such_key", 4, "Count", nil, 1, 1, 20, false)
  assert_eq(count.read("settings_test"), 4, "a missing int is not its default")

  var mode = ChoiceSettingCfg("no_choice", "b", "Mode", nil, ["a", "b"], false)
  assert_eq(mode.read("settings_test"), "b", "a missing choice is not its default")
end

test("a missing key reads as the setting's default", missing_keys_read_their_default)

def stored_values_are_held_to_their_kind()
  store.set("settings_test", "count", "99")
  var count = IntSettingCfg("count", 4, "Count", nil, 1, 1, 20, false)
  assert_eq(count.read("settings_test"), 20, "a stored int above max is not held to it")

  store.set("settings_test", "mode", "sideways")
  var mode = ChoiceSettingCfg("mode", "off", "Mode", nil, ["off", "on"], false)
  assert_eq(mode.read("settings_test"), "off", "a value outside the choices is not defaulted")

  store.set("settings_test", "gain", "7.5")
  var gain = RealSettingCfg("gain", 0.0, "Gain", nil, 0.5, 0.0, 1.0, false)
  assert_eq(gain.read("settings_test"), 1.0, "a stored real above max is not held to it")
end

test("a stored value is held to its kind", stored_values_are_held_to_their_kind)

def nothing_reads_as_nil_when_nullable()
  store.set("settings_test", "empty", "")
  var empty = IntSettingCfg("empty", 5, "Count", nil, 1, 0, 10, true)
  assert_nil(empty.read("settings_test"), "an empty nullable int is not nil")

  var absent = IntSettingCfg("absent", 5, "Count", nil, 1, 0, 10, true)
  assert_nil(absent.read("settings_test"), "an absent nullable int is not nil")

  store.set("settings_test", "zero", "0")
  var zero = IntSettingCfg("zero", 5, "Count", nil, 1, 0, 10, true)
  assert_eq(zero.read("settings_test"), 0, "a stored zero is not a value")
end

test("a nullable key that holds nothing reads nil, not its default", nothing_reads_as_nil_when_nullable)

def every_kind_builds()
  var flags = BoolSettingCfg("flags", true, "Flags", nil, false)
  assert_eq(flags.text(true), "ON", "a bool does not print ON")
  assert_eq(flags.text(false), "OFF", "a bool does not print OFF")
  assert_eq(flags.lowest(), false, "a bool's lowest is not false")

  var count = IntSettingCfg("count", 4, "Count", nil, 1, 1, 20, false)
  assert_eq(count.text(7), "7", "an int does not print its number")

  var gain = RealSettingCfg("gain", 0.5, "Gain", nil, 0.5, 0.0, 1.0, false)
  assert_eq(gain.text(0.5), "0.5", "a real does not print its number")

  var mode = ChoiceSettingCfg("mode", "a", "Mode", nil, ["a", "b"], false)
  assert_eq(mode.text("b"), "b", "a choice does not print its value")

  var rest = PressureSettingCfg("rest", nil, "Rest", nil, 1, true)
  assert_nil(rest.default_value, "a nullable pressure does not hold a nil default")

  var hold = DurationSettingCfg("hold", 90, "Hold", nil, 1, 0, 7200, false)
  assert_eq(hold.text(90), "1m30s", "a duration does not print minutes and seconds")
end

test("every kind builds and prints its value", every_kind_builds)

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
  assert_eq(scene.values[at], scene.items[at].default_value, "the repair did not put the value back")
  assert_eq(wrote[scene.items[at].key], scene.items[at].default_value, "the repair was not written")
end

test("a repair puts a refused set back to its defaults", a_repair_puts_a_set_back)
