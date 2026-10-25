# settings.test.be — what the settings model promises: a default when nothing has been stored, a
# value held to its kind on the way in, a set the caller repairs rather than hands on broken, and the
# one clean map the registry builds from them all. The shipped routines exercise int, pressure,
# duration and choice; bool, real and every value past a bound are only covered here.

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

# A choice may be a [value, title] pair: the value is what the setting stores, the title is what
# the row and the editor print, so an identifier and its label are one entry.
def a_pair_choice_stores_its_value_and_prints_its_title()
  var pairs = [["a", "Alpha"], ["b", "Beta"]]
  var mode = ChoiceSettingCfg("mode", "b", "Mode", nil, pairs, false)
  assert_eq(mode.read("settings_test"), "b", "a pair choice does not read its default value")
  assert_eq(mode.text("b"), "Beta", "a pair choice does not print its title")

  store.set("settings_test", "mode", "a")
  assert_eq(mode.read("settings_test"), "a", "a pair choice does not read its stored value")

  store.set("settings_test", "mode", "Alpha")
  assert_eq(mode.read("settings_test"), "b", "a pair choice accepted a title that is not a value")

  var scene = ChoiceScene("Mode", "a", pairs, false)
  assert_eq(scene.text(), "Alpha", "a pair choice's editor does not print its title")
  scene.nudge(1)
  assert_eq(scene.value(), "b", "a pair choice's editor does not step to the next value")
  assert_eq(scene.text(), "Beta", "a pair choice's editor does not print the title it stepped to")
end

test("a pair choice stores the value and prints the title",
     a_pair_choice_stores_its_value_and_prints_its_title)

# The store hands back the file's text, so a setting with no default of its type to lean on has to
# parse it: a nullable pressure, and a bool, are the two that would go wrong quietly.
def a_stored_value_is_parsed_by_its_kind()
  store.set("settings_test", "rest", "12.5")
  var rest = PressureSettingCfg("rest", nil, "Rest", nil, 1, true)
  assert_eq(rest.read("settings_test"), 12.5, "a stored nullable pressure is not its number")

  store.set("settings_test", "flags", "false")
  var flags = BoolSettingCfg("flags", true, "Flags", nil, false)
  assert_eq(flags.read("settings_test"), false, "a stored false does not read as false")

  store.set("settings_test", "gain", "0.25")
  var gain = RealSettingCfg("gain", 0.0, "Gain", nil, 0.5, 0.0, 1.0, false)
  assert_eq(gain.read("settings_test"), 0.25, "a stored real is not its number")
end

test("a stored value is parsed by its kind", a_stored_value_is_parsed_by_its_kind)

# A routine's values are read once, by the registry, into the map its init() is handed: held to their
# kinds, repaired by the routine's rule, and complete. A page never reads a document itself.
def load_makes_a_clean_map()
  var settings = routines.load("ramped_static")
  assert_eq(settings["initial_pressure"], 5.0, "load: the start pressure is not the file's")
  assert_eq(settings["total_duration"], 300, "load: the total duration is not the file's")

  # A value past its bounds arrives held to them, and a nullable key holding nothing arrives nil.
  store.set("interval", "set_count", "99")
  var held = routines.load("interval")
  assert_eq(held["set_count"], 20, "load: a stored int above max is not held to it")
  assert_nil(held["rest_pressure"], "load: a nullable key holding nothing is not nil")
  store.set("interval", "set_count", "4")

  # The routine's rule is applied to the map, so a page is never initialized with a set it refuses.
  store.set("ramped_static", "total_duration", "30")
  var repaired = routines.load("ramped_static")
  assert_eq(repaired["total_duration"], 300, "load: a set the rule refuses is not put back")
  store.set("ramped_static", "total_duration", "300")
end

test("loading a routine's settings makes a clean map", load_makes_a_clean_map)

def the_rules_say_no()
  assert_eq(RampedIntervalSettings.problem(
                {"initial_pressure": 10.0, "final_pressure": 5.0}),
            "FINAL < START", "the ladder rule does not refuse a final below the start")
  assert_nil(RampedIntervalSettings.problem(
                {"initial_pressure": 5.0, "final_pressure": 10.0}),
            "the ladder rule refuses a valid pair")

  assert_eq(RampedStaticSettings.problem(
                {"initial_duration": 60, "target_duration": 60, "total_duration": 100}),
            "TOTAL < HOLDS", "the profile rule does not refuse a total below the holds")
end

test("a routine's rule refuses a set that cannot run", the_rules_say_no)

def a_repair_puts_a_set_back()
  var items = routines.settings("ramped_static")
  var values = []
  for item : items
    values.push(item.read("ramped_static"))
  end
  routines.repair("ramped_static", items, values)
  assert_nil(routines.repair("ramped_static", items, values), "a set of valid values was refused")

  # A total below the two holds is what lowering Total under a longer hold pair produces.
  values[4] = 30
  assert_eq(routines.repair("ramped_static", items, values),
            "TOTAL < HOLDS", "the repair did not report the rule it applied")
  assert_eq(values[4], items[4].default_value, "the repair did not put the value back")
end

test("a repair puts a refused set back to its defaults", a_repair_puts_a_set_back)
