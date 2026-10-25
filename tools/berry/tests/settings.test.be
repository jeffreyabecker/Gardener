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
# the row and the editor print. The setting normalizes the list it is given, so what it holds — and
# what it hands its scene — is a ChoiceItem either way.
def a_pair_choice_stores_its_value_and_prints_its_title()
  var pairs = [["a", "Alpha"], ["b", "Beta"]]
  var mode = ChoiceSettingCfg("mode", "b", "Mode", nil, pairs, false)
  assert_eq(mode.read("settings_test"), "b", "a pair choice does not read its default value")
  assert_eq(mode.text("b"), "Beta", "a pair choice does not print its title")

  # The list is normalized on the way in, and a bare value is its own title.
  assert_eq(mode.values[0].key, "a", "a pair is not normalized to a record's key")
  assert_eq(mode.values[0].title, "Alpha", "a pair is not normalized to a record's title")
  var plain = choice_items(["a", "b"])
  assert_eq(plain[1].key, "b", "a bare value is not a record's key")
  assert_eq(plain[1].title, "b", "a bare value is not its own title")

  store.set("settings_test", "mode", "a")
  assert_eq(mode.read("settings_test"), "a", "a pair choice does not read its stored value")

  store.set("settings_test", "mode", "Alpha")
  assert_eq(mode.read("settings_test"), "b", "a pair choice accepted a title that is not a value")

  var scene = ChoiceScene("Mode", "a", mode.values, false)
  assert_eq(scene.text(), "Alpha", "a pair choice's editor does not print its title")
  scene.nudge(1)
  assert_eq(scene.value(), "b", "a pair choice's editor does not step to the next value")
  assert_eq(scene.text(), "Beta", "a pair choice's editor does not print the title it stepped to")
end

test("a pair choice stores the value and prints the title",
     a_pair_choice_stores_its_value_and_prints_its_title)

# A choice may answer gestures of its own: the editor offers whatever it has no answer of its own for
# to the choice that is showing, and hands that choice to the action.
choiceExtraRan = nil

def choice_extra(item)
  choiceExtraRan = item
end

def a_choice_answers_its_own_gestures()
  var item = ChoiceItem("a", "Alpha", nil, {"ok:double": choice_extra})
  var scene = ChoiceScene("Mode", "a", [item], false)

  choiceExtraRan = nil
  assert_true(scene.handle_page("ok:double"), "a choice's own gesture was not handled")
  assert_eq(choiceExtraRan, item, "a choice's own gesture did not receive the choice it belongs to")

  # The editor keeps its own gestures: one of those is answered before any choice is asked.
  choiceExtraRan = nil
  assert_true(scene.handle_page("up:single"), "the editor did not answer its own gesture")
  assert_nil(choiceExtraRan, "a choice's own gesture ran for one the editor owns")

  # A gesture nobody answers is dropped.
  assert_true(!scene.handle_page("down:double"), "a gesture nobody answers was handled")
end

test("a choice answers the gestures it carries", a_choice_answers_its_own_gestures)

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

# --- the map forms ---------------------------------------------------------
#
# One call for the whole document: a page reads every value it keeps in one pass and commits a
# settled set in one write, rather than a `get` or a flash write per key.

def a_map_read_answers_every_stored_key()
  store.set("settings_test", "map_count", "12")
  store.set("settings_test", "map_gain", "0.25")

  var all = store.get("settings_test")
  assert_eq(all["map_count"], "12", "the whole read does not answer a stored key")
  assert_eq(all["map_gain"], "0.25", "the whole read does not answer every stored key")
  assert_nil(all.find("map_absent"), "the whole read answers a key nothing stored")
end

test("a map read answers every stored key as text", a_map_read_answers_every_stored_key)

# The fallback sets the type of one key; a map of defaults does the same for every key it names,
# and stands as the answer for a key the document holds nothing for.
def a_default_map_types_each_key()
  store.set("settings_test", "map_count", "12")
  store.set("settings_test", "map_gain", "0.25")

  var typed = store.get("settings_test",
                        {"map_count": 0, "map_gain": 0.0, "map_flags": true,
                         "map_label": "x", "map_absent": 7})
  assert_eq(typed["map_count"], 12, "a stored int is not read as an int")
  assert_eq(typed["map_gain"], 0.25, "a stored real is not read as a real")
  assert_eq(typed["map_flags"], true, "an absent bool is not its default")
  assert_eq(typed["map_label"], "x", "an absent string is not its default")
  assert_eq(typed["map_absent"], 7, "an absent int is not its default")
end

test("a map of defaults types each key it names", a_default_map_types_each_key)

def a_map_write_merges_in_one_write()
  store.set("settings_test", "map_keep", "kept")
  store.set("settings_test", "map_count", "1")

  var before = harness.writes()
  assert_true(store.set("settings_test", {"map_count": 9, "map_gain": 0.5}),
              "a map write is not accepted")
  assert_eq(harness.writes() - before, 1, "a map write is not one write")
  assert_eq(store.get("settings_test", "map_count", 0), 9, "a map write did not land")
  assert_eq(store.get("settings_test", "map_keep", ""), "kept",
            "a map write dropped a key it did not name")
end

test("a map write merges in one write", a_map_write_merges_in_one_write)

def a_map_write_of_the_same_values_writes_nothing()
  store.set("settings_test", "map_count", "4")

  var before = harness.writes()
  assert_true(store.set("settings_test", {"map_count": 4}), "a repeat map write is not accepted")
  assert_eq(harness.writes(), before, "a map write of the same values wrote the file")
end

test("a map write of the values already there writes nothing",
     a_map_write_of_the_same_values_writes_nothing)

# A routine's values are read once, by the registry, into the map its init() is handed: held to their
# kinds, repaired by the routine's rule, and complete. A page never reads a document itself.
def load_makes_a_clean_map()
  var settings = routines.load("ramped_static")
  assert_eq(settings["initial_pressure"], 5.0, "load: the start pressure is not the file's")
  assert_eq(settings["total_duration"], 300, "load: the total duration is not the file's")

  # A value past its bounds arrives held to them, and a nullable key holding nothing arrives nil.
  # The shipped interval.cfg carries a rest pressure, so the key is emptied first: an empty value is
  # how the store says a setting holds nothing.
  store.set("interval", "set_count", "99")
  store.set("interval", "rest_pressure", "")
  var held = routines.load("interval")
  assert_eq(held["set_count"], 20, "load: a stored int above max is not held to it")
  assert_nil(held["rest_pressure"], "load: a nullable key holding nothing is not nil")
  store.set("interval", "set_count", "4")
  store.set("interval", "rest_pressure", "5.0")

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
  var settings_cfgs = routines.settings("ramped_static")
  var settings_values = routines.load("ramped_static")
  assert_nil(routines.repair("ramped_static", settings_cfgs, settings_values),
             "a set of valid values was refused")

  # A total below the two holds is what lowering Total under a longer hold pair produces.
  settings_values["total_duration"] = 30
  assert_eq(routines.repair("ramped_static", settings_cfgs, settings_values),
            "TOTAL < HOLDS", "the repair did not report the rule it applied")
  assert_eq(settings_values["total_duration"], 300, "the repair did not put the value back")
end

test("a repair puts a refused set back to its defaults", a_repair_puts_a_set_back)

# The editor contract: draw_page() hands draw_editor the page window and draws nothing itself, so a
# kind's override controls the whole rendering.
class RecordingEditor : ValueScene
  var window

  def init(label)
    super(self).init(label)
    self.window = nil
  end

  def draw_editor(pos, dim)
    self.window = [pos.x, pos.y, dim.w, dim.h]
  end
end

def the_page_hands_the_editor_its_window()
  var scene = RecordingEditor("Label")
  scene.draw_page()
  assert_eq(scene.window[0], 0, "the editor window does not start at the page's left")
  assert_eq(scene.window[1], 0, "the editor window does not start at the page's top")
  assert_eq(scene.window[2], screen.page_width(), "the editor window is not the page's width")
  assert_eq(scene.window[3], screen.page_height(), "the editor window is not the page's height")
end

test("the page hands the editor its window", the_page_hands_the_editor_its_window)

# Each kind draws itself from that window. A case pushes its editor and beats once, which is the
# frame the runner captures — the picture to look at. The nil cases are the ones with no value to
# show and no bar or highlight to place.
def show_editor(scene)
  assert_true(ux.push(scene), "the editor would not push")
  harness.beat(1)
end

def the_default_editor_draws_its_three_lines()
  show_editor(ValueScene("Label"))
end

test("the default editor draws its three lines", the_default_editor_draws_its_three_lines)

def the_bool_editor_draws_on()
  show_editor(BoolScene("Flags", true, false))
end

test("the bool editor draws ON", the_bool_editor_draws_on)

def the_bool_editor_draws_off()
  show_editor(BoolScene("Flags", false, false))
end

test("the bool editor draws OFF", the_bool_editor_draws_off)

def the_bool_editor_draws_nothing()
  show_editor(BoolScene("Flags", nil, true))
end

test("the bool editor draws a nullable nothing", the_bool_editor_draws_nothing)

def the_choice_editor_draws_its_list()
  show_editor(ChoiceScene("Mode", "b", choice_items(["a", "b", "c", "d", "e"]), false))
end

test("the choice editor draws its list", the_choice_editor_draws_its_list)

def the_choice_editor_draws_nothing()
  show_editor(ChoiceScene("Mode", nil, choice_items(["a", "b"]), true))
end

test("the choice editor draws a nullable nothing", the_choice_editor_draws_nothing)

def the_int_editor_draws_between_its_bounds()
  show_editor(IntScene("Count", 7, 1, 1, 20, false))
end

test("the int editor draws between its bounds", the_int_editor_draws_between_its_bounds)

def the_real_editor_draws_without_a_bar()
  show_editor(RealScene("Gain", 0.5, 0.5, nil, nil, false))
end

test("the real editor draws without a bar", the_real_editor_draws_without_a_bar)

def the_pressure_editor_draws_in_the_panel_unit()
  show_editor(PressureScene("Rest", 5.0, 1, false))
end

test("the pressure editor draws in the panel unit", the_pressure_editor_draws_in_the_panel_unit)

def the_duration_editor_draws_its_minutes()
  show_editor(DurationScene("Hold", 90, 1, 0, 7200, false))
end

test("the duration editor draws its minutes", the_duration_editor_draws_its_minutes)

def the_duration_editor_draws_its_seconds()
  var scene = DurationScene("Hold", 90, 1, 0, 7200, false)
  scene.stage = "sec"
  show_editor(scene)
end

test("the duration editor draws its seconds", the_duration_editor_draws_its_seconds)
