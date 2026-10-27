# settings.test.be — what the settings model promises: a default when nothing has been stored, a
# value held to its kind on the way in, a set the caller repairs rather than hands on broken, and the
# one clean map the registry builds from them all. The shipped routines exercise int, pressure,
# duration and choice; bool, real and every value past a bound are only covered here.

suite = "settings"

# One setting read from a document the way the registry's load_settings() reads every key: the
# stored text parsed by the kind and held to it, and a key holding nothing reading as nothing.
def read_setting(cfg, name)
  var document = device.config("routines/" + name + ".ini")
  var raw = document.get("", cfg.key, "")
  if raw == ""
    return cfg.value(nil)
  end
  return cfg.value(cfg.parse(raw))
end

# One key written through the document, opened fresh, so a case seeds the same file a page reads.
def write_setting(name, key, value)
  var document = device.config("routines/" + name + ".ini")
  document.set("", key, value)
  document.save()
end

# A scene is given the page as it is pushed; a case that builds or draws one on its own hands it the
# same box, since a list window and a frame are both measured from it.
def page_box(scene)
  scene.to_box(0, 0, screen.page_width(), screen.page_height() - overlay_band_height)
  return scene
end

def missing_keys_read_their_default()
  var count = IntEditCfg("no_such_key", 4, "Count", nil, 1, 1, 20, false)
  assert_eq(read_setting(count, "settings_test"), 4, "a missing int is not its default")

  var mode = ChoiceEditCfg("no_choice", "b", "Mode", nil, ["a", "b"], false)
  assert_eq(read_setting(mode, "settings_test"), "b", "a missing choice is not its default")
end

test("a missing key reads as the setting's default", missing_keys_read_their_default)

def stored_values_are_held_to_their_kind()
  write_setting("settings_test", "count", "99")
  var count = IntEditCfg("count", 4, "Count", nil, 1, 1, 20, false)
  assert_eq(read_setting(count, "settings_test"), 20, "a stored int above max is not held to it")

  write_setting("settings_test", "mode", "sideways")
  var mode = ChoiceEditCfg("mode", "off", "Mode", nil, ["off", "on"], false)
  assert_eq(read_setting(mode, "settings_test"), "off", "a value outside the choices is not defaulted")

  write_setting("settings_test", "gain", "7.5")
  var gain = RealEditCfg("gain", 0.0, "Gain", nil, 0.5, 0.0, 1.0, false)
  assert_eq(read_setting(gain, "settings_test"), 1.0, "a stored real above max is not held to it")
end

test("a stored value is held to its kind", stored_values_are_held_to_their_kind)

def nothing_reads_as_nil_when_nullable()
  write_setting("settings_test", "empty", "")
  var empty = IntEditCfg("empty", 5, "Count", nil, 1, 0, 10, true)
  assert_nil(read_setting(empty, "settings_test"), "an empty nullable int is not nil")

  var absent = IntEditCfg("absent", 5, "Count", nil, 1, 0, 10, true)
  assert_nil(read_setting(absent, "settings_test"), "an absent nullable int is not nil")

  write_setting("settings_test", "zero", "0")
  var zero = IntEditCfg("zero", 5, "Count", nil, 1, 0, 10, true)
  assert_eq(read_setting(zero, "settings_test"), 0, "a stored zero is not a value")
end

test("a nullable key that holds nothing reads nil, not its default", nothing_reads_as_nil_when_nullable)

def every_kind_builds()
  var flags = BoolEditCfg("flags", true, "Flags", nil, false)
  assert_eq(flags.text(true), "ON", "a bool does not print ON")
  assert_eq(flags.text(false), "OFF", "a bool does not print OFF")

  var count = IntEditCfg("count", 4, "Count", nil, 1, 1, 20, false)
  assert_eq(count.text(7), "7", "an int does not print its number")

  var gain = RealEditCfg("gain", 0.5, "Gain", nil, 0.5, 0.0, 1.0, false)
  assert_eq(gain.text(0.5), "0.5", "a real does not print its number")

  var mode = ChoiceEditCfg("mode", "a", "Mode", nil, ["a", "b"], false)
  assert_eq(mode.text("b"), "b", "a choice does not print its value")

  var rest = PressureEditCfg("rest", nil, "Rest", nil, 1, true)
  assert_nil(rest.default_value, "a nullable pressure does not hold a nil default")

  var hold = DurationEditCfg("hold", 90, "Hold", nil, 1, 0, 7200, false)
  assert_eq(hold.text(90), "1m30s", "a duration does not print minutes and seconds")
end

test("every kind builds and prints its value", every_kind_builds)

# A choice may be a [value, title] pair: the value is what the setting stores, the title is what
# the row and the editor print. The setting normalizes the list it is given, so what it holds — and
# what it hands its scene — is a ChoiceItem either way.
def a_pair_choice_stores_its_value_and_prints_its_title()
  var pairs = [["a", "Alpha"], ["b", "Beta"]]
  var mode = ChoiceEditCfg("mode", "b", "Mode", nil, pairs, false)
  assert_eq(read_setting(mode, "settings_test"), "b", "a pair choice does not read its default value")
  assert_eq(mode.text("b"), "Beta", "a pair choice does not print its title")

  # The list is normalized on the way in, and a bare value is its own title.
  assert_eq(mode.values[0].key, "a", "a pair is not normalized to a record's key")
  assert_eq(mode.values[0].title, "Alpha", "a pair is not normalized to a record's title")
  var plain = choice_items(["a", "b"])
  assert_eq(plain[1].key, "b", "a bare value is not a record's key")
  assert_eq(plain[1].title, "b", "a bare value is not its own title")

  write_setting("settings_test", "mode", "a")
  assert_eq(read_setting(mode, "settings_test"), "a", "a pair choice does not read its stored value")

  write_setting("settings_test", "mode", "Alpha")
  assert_eq(read_setting(mode, "settings_test"), "b", "a pair choice accepted a title that is not a value")

  var scene = page_box(ChoiceEditScene("Mode", "a", mode.values, false))
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
  var scene = page_box(ChoiceEditScene("Mode", "a", [item], false))

  choiceExtraRan = nil
  assert_true(scene.handle("ok:double"), "a choice's own gesture was not handled")
  assert_eq(choiceExtraRan, item, "a choice's own gesture did not receive the choice it belongs to")

  # The editor keeps its own gestures: one of those is answered before any choice is asked.
  choiceExtraRan = nil
  assert_true(scene.handle("up:single"), "the editor did not answer its own gesture")
  assert_nil(choiceExtraRan, "a choice's own gesture ran for one the editor owns")

  # A gesture nobody answers is dropped.
  assert_true(!scene.handle("down:double"), "a gesture nobody answers was handled")
end

test("a choice answers the gestures it carries", a_choice_answers_its_own_gestures)

# What a choice editor steps through: the ring opens on the value it is handed, wraps at both ends,
# and — when the setting is nullable — is given "no choice" as an entry of its own by the editor, so
# the entry a nil value is drawn and stepped as is like any other.
def the_ring_opens_on_its_value_and_wraps()
  var items = choice_items(["a", "b", "c"])

  var ring = ChoiceRing(items, "b")
  assert_eq(ring.index, 1, "the ring did not open on the value it was handed")
  assert_eq(ring.current().key, "b", "the ring does not show the value it opened on")
  ring.nudge(1)
  assert_eq(ring.current().key, "c", "the ring does not step up")
  ring.nudge(1)
  assert_eq(ring.current().key, "a", "the ring does not wrap at the end")
  ring.nudge(-1)
  assert_eq(ring.current().key, "c", "the ring does not step down")

  # A stored value that is no longer one of the entries opens on the first, which is what a setting's
  # parse() has already refused it for.
  var odd = ChoiceRing(items, "gone")
  assert_eq(odd.current().key, "a", "a value outside the entries does not open on the first")

  # An empty ring shows nothing and steps nowhere.
  var empty = ChoiceRing([], nil)
  assert_nil(empty.current(), "an empty ring shows an entry")
  empty.nudge(1)
  assert_nil(empty.current(), "an empty ring stepped to an entry")
end

test("the ring opens on its value and wraps", the_ring_opens_on_its_value_and_wraps)

def a_nullable_choice_carries_no_choice_as_an_entry()
  # The entry is the normalization's: a nullable list is given one whose key is nil, at the front.
  var entries = choice_items(["a", "b"], true)
  assert_eq(entries.size(), 3, "a nullable list is not given a no-choice entry")
  assert_nil(entries[0].key, "the no-choice entry is not at the front")
  assert_eq(entries[0].title, "--", "the no-choice entry does not print the editor's dash")

  # A declaration that names that entry itself keeps its own title, and is not given a second one.
  var own = choice_items([["sedate", "Sedate"], [nil, "None"]], true)
  assert_eq(own.size(), 2, "a declared no-choice entry was given a second one")
  assert_eq(own[1].title, "None", "the declared no-choice entry lost its own title")

  # A setting that always holds a value has no such entry, nor does a list asked without one.
  assert_eq(choice_items(["a", "b"], false).size(), 2, "a list was given a no-choice entry")
  assert_eq(choice_items(["a", "b"]).size(), 2, "a list was given a no-choice entry by default")

  var scene = page_box(ChoiceEditScene("Mode", nil, entries, true))
  assert_nil(scene.value(), "a nullable choice does not open on no choice")
  assert_eq(scene.text(), "--", "the no-choice entry does not print the editor's dash")

  # Down from no choice wraps to the last entry, and up from the last lands on no choice again.
  scene.nudge(-1)
  assert_eq(scene.value(), "b", "down from no choice does not wrap to the last entry")
  scene.nudge(1)
  assert_nil(scene.value(), "up from the last entry does not land on no choice")

  var plain = ChoiceEditScene("Mode", "a", choice_items(["a", "b"]), false)
  assert_eq(plain.ring.items.size(), 2, "a choice that holds a value was given a no-choice entry")

  var named = page_box(ChoiceEditScene("Mode", nil, own, true))
  assert_eq(named.ring.items.size(), 2, "a declared no-choice entry was given a second one")
  assert_nil(named.value(), "the editor does not open on the declared no-choice entry")
  assert_eq(named.text(), "None", "the declared no-choice entry lost its own title")
end

test("a nullable choice carries no choice as an entry", a_nullable_choice_carries_no_choice_as_an_entry)

# A bool is the same ring with two entries, and three when the setting is nullable: it opens on the
# state it was handed and steps through them all, wrapping at the ends like any other choice.
def a_bool_walks_its_states()
  var flags = BoolEditScene("Flags", false, false)
  assert_eq(flags.text(), "OFF", "a bool set false does not print OFF")
  flags.nudge(1)
  assert_eq(flags.value(), true, "up from OFF does not reach ON")
  flags.nudge(1)
  assert_eq(flags.value(), false, "up from ON does not come back to OFF")

  var nothing = BoolEditScene("Flags", nil, true)
  assert_eq(nothing.ring.items.size(), 3, "a nullable bool does not carry a no-choice entry")
  assert_eq(nothing.text(), "--", "a nullable bool does not open on nothing")
  nothing.nudge(1)
  assert_eq(nothing.value(), false, "up from nothing does not reach OFF")
  nothing.nudge(1)
  assert_eq(nothing.value(), true, "up from OFF does not reach ON")
  nothing.nudge(1)
  assert_nil(nothing.value(), "up from ON does not wrap to nothing")
  nothing.nudge(-1)
  assert_eq(nothing.value(), true, "down from nothing does not wrap to ON")
end

test("a bool walks its states", a_bool_walks_its_states)

# The store hands back the file's text, so a setting with no default of its type to lean on has to
# parse it: a nullable pressure, and a bool, are the two that would go wrong quietly.
def a_stored_value_is_parsed_by_its_kind()
  write_setting("settings_test", "rest", "12.5")
  var rest = PressureEditCfg("rest", nil, "Rest", nil, 1, true)
  assert_eq(read_setting(rest, "settings_test"), 12.5, "a stored nullable pressure is not its number")

  write_setting("settings_test", "flags", "false")
  var flags = BoolEditCfg("flags", true, "Flags", nil, false)
  assert_eq(read_setting(flags, "settings_test"), false, "a stored false does not read as false")

  write_setting("settings_test", "gain", "0.25")
  var gain = RealEditCfg("gain", 0.0, "Gain", nil, 0.5, 0.0, 1.0, false)
  assert_eq(read_setting(gain, "settings_test"), 0.25, "a stored real is not its number")
end

test("a stored value is parsed by its kind", a_stored_value_is_parsed_by_its_kind)

# A routine's values are read once, by the registry, into the map its init() is handed: held to their
# kinds, repaired by the routine's rule, and complete. A page never reads a document itself.
def load_makes_a_clean_map()
  var settings = routines.load_settings("ramped_static")
  assert_eq(settings["initial_pressure"], 5.0,
            "load_settings: the start pressure is not the file's")
  assert_eq(settings["total_duration"], 300,
            "load_settings: the total duration is not the file's")

  # A value past its bounds arrives held to them, and a nullable key holding nothing arrives nil.
  # The shipped interval.ini carries a rest pressure, so the key is emptied first: an empty value is
  # how the store says a setting holds nothing.
  write_setting("interval", "set_count", "99")
  write_setting("interval", "rest_pressure", "")
  var held = routines.load_settings("interval")
  assert_eq(held["set_count"], 20,
            "load_settings: a stored int above max is not held to it")
  assert_nil(held["rest_pressure"],
             "load_settings: a nullable key holding nothing is not nil")
  write_setting("interval", "set_count", "4")
  write_setting("interval", "rest_pressure", "5.0")

  # The routine's rule is applied to the map, so a page is never initialized with a set it refuses.
  write_setting("ramped_static", "total_duration", "30")
  var repaired = routines.load_settings("ramped_static")
  assert_eq(repaired["total_duration"], 300,
            "load_settings: a set the rule refuses is not put back")
  write_setting("ramped_static", "total_duration", "300")
end

test("loading a routine's settings makes a clean map", load_makes_a_clean_map)

def the_rules_say_no()
  assert_eq(routines.entry("ramped_interval").problem(
                {"initial_pressure": 10.0, "final_pressure": 5.0}),
            "FINAL < START", "the ladder rule does not refuse a final below the start")
  assert_nil(routines.entry("ramped_interval").problem(
                {"initial_pressure": 5.0, "final_pressure": 10.0}),
            "the ladder rule refuses a valid pair")

  assert_eq(routines.entry("ramped_static").problem(
                {"initial_duration": 60, "target_duration": 60, "total_duration": 100}),
            "TOTAL < HOLDS", "the profile rule does not refuse a total below the holds")
end

test("a routine's rule refuses a set that cannot run", the_rules_say_no)

def a_repair_puts_a_set_back()
  var registration = routines.entry("ramped_static")
  var settings_values = routines.load_settings("ramped_static")
  assert_nil(registration.repair("", settings_values),
             "a set of valid values was refused")

  # A total below the two holds is what lowering Total under a longer hold pair produces.
  settings_values["total_duration"] = 30
  assert_eq(registration.repair("", settings_values),
            "TOTAL < HOLDS", "the repair did not report the rule it applied")
  assert_eq(settings_values["total_duration"], 300, "the repair did not put the value back")
end

test("a repair puts a refused set back to its defaults", a_repair_puts_a_set_back)

# The editor contract: draw_page() frames the content a kind measures and hands draw_editor the box
# inside the border, so a kind draws in a box no bigger than what it holds and nothing it draws
# leaves the frame. The frame is centred in the scene's own box, which is the box of the scene it
# was opened over.
class RecordingEditor : EditScene
  var window

  def init(label)
    super(self).init(label, nil)
    self.window = nil
  end

  def content_size()
    return bounds(60, 30)
  end

  def draw_editor(pos, dim)
    self.window = [pos.x, pos.y, dim.w, dim.h]
  end
end

def the_page_frames_the_editor_and_hands_it_the_content_box()
  var scene = page_box(RecordingEditor("Label"))
  scene.draw_page()

  var content = scene.content_size()
  assert_eq(scene.window[2], content.w, "the editor's box is not the content's width")
  assert_eq(scene.window[3], content.h, "the editor's box is not the content's height")

  # The frame is the content and the two pixels the border keeps either side of it: one of paper, one
  # of ink.
  var frame_x = scene.window[0] - 2
  var frame_y = scene.window[1] - 2
  var frame_w = scene.window[2] + 4
  var frame_h = scene.window[3] + 4
  assert_true(frame_w < scene.dim.w, "the frame is as wide as the scene's box")
  assert_true(frame_h < scene.dim.h, "the frame is as tall as the scene's box")

  var left = frame_x - scene.pos.x
  var right = scene.pos.x + scene.dim.w - (frame_x + frame_w)
  assert_true(left >= 1, "the frame keeps no margin from the box's left")
  assert_true(right >= 1, "the frame keeps no margin from the box's right")
  assert_true(left - right < 2 && right - left < 2, "the frame is not centered across the box")

  var top = frame_y - scene.pos.y
  var bottom = scene.pos.y + scene.dim.h - (frame_y + frame_h)
  assert_true(top >= 1, "the frame keeps no margin from the box's top")
  assert_true(bottom >= 1, "the frame keeps no margin from the box's bottom")
  assert_true(top - bottom < 2 && bottom - top < 2, "the frame is not centered down the box")
end

test("the page frames the editor and hands it the content box",
     the_page_frames_the_editor_and_hands_it_the_content_box)

# Every kind measures and draws the whole of its own content, chrome included: a label row and a
# hint row are in every one of them, whatever its body is. The base measures nothing of its own.
def every_editor_measures_its_own_chrome()
  var editors = [
    BoolEditScene("Flags", true, false),
    ChoiceEditScene("Mode", "a", choice_items(["a", "b"]), false),
    IntEditScene("Count", 7, 1, 1, 20, false),
    RealEditScene("Gain", 0.5, 0.5, nil, nil, false),
    PressureEditScene("Rest", 5.0, 1, false),
    DurationEditScene("Hold", 90, 1, 0, 7200, false)
  ]
  for scene : editors
    page_box(scene).activate()
    var content = scene.content_size()
    assert_true(content.h >= 20, "an editor does not measure its own label and hint rows")
    assert_true(content.w > 0, "an editor measures itself as no wider than nothing")
    assert_true(content.w >= screen.text_bounds(scene.label, 1).w,
                "an editor's content is narrower than its label")
  end
  assert_eq(EditScene("Label").content_size().w, 0, "the base measures a content of its own")
end

test("every editor measures its own chrome", every_editor_measures_its_own_chrome)

# A kind that shows a value draws its content in one column: the label row, two pixels of paper, the
# body, two pixels of paper and the hint row, each resolved to the pixels the kind measured it at.
def the_editor_column_places_its_rows()
  var editors = [
    BoolEditScene("Flags", true, false),
    IntEditScene("Count", 7, 1, 1, 20, false),
    RealEditScene("Gain", 0.5, 0.5, nil, nil, false),
    PressureEditScene("Rest", 5.0, 1, false),
    DurationEditScene("Hold", 90, 1, 0, 7200, false)
  ]
  for scene : editors
    page_box(scene).activate()

    var page = scene.page
    var content = scene.content_size()
    assert_eq(content.h, 20 + scene.body_h(), "the content is not the body and its two chrome rows")
    assert_true(content.w + 4 < scene.dim.w, "the frame is not narrower than the page")
    assert_true(content.h + 4 < scene.dim.h, "the frame is not shorter than the page")
    assert_eq(page.dim.w, content.w, "the column is not the content's width")
    assert_eq(page.dim.h, content.h, "the column is not the content's height")

    var rows = page.children
    assert_eq(rows.size(), 5, "the editor column does not have five rows")
    assert_eq(rows[0].pos.y, page.pos.y, "the label row is not the content's top")
    assert_eq(rows[0].dim.h, 8, "the label row is not eight pixels")
    assert_eq(rows[1].dim.h, 2, "the paper under the label is not two pixels")
    assert_eq(rows[2].pos.y, page.pos.y + 10, "the body does not start ten pixels down")
    assert_eq(rows[2].dim.h, scene.body_h(), "the body is not the height the kind measured")
    assert_eq(rows[3].dim.h, 2, "the paper under the body is not two pixels")
    assert_eq(rows[4].pos.y, page.pos.y + page.dim.h - 8, "the hint row is not the bottom eight")
    assert_eq(rows[4].dim.h, 8, "the hint row is not eight pixels")
    assert_eq(rows[0].dim.w, page.dim.w, "the label row is not the content's width")
    assert_eq(rows[2].dim.w, page.dim.w, "the body row is not the content's width")
    assert_eq(rows[4].dim.w, page.dim.w, "the hint row is not the content's width")
    assert_eq(classname(rows[0].children[0]), "Text",
              "the label row is not a text")
    assert_nil(rows[0].children[0].font, "the label row is not drawn in the system face")
  end
end

test("the editor column places its rows", the_editor_column_places_its_rows)

# A list that fits is a block the height of its heading and its rows, centred in the scene's box, the
# heading centred above rows the width of the band a selected row inverts.
def a_short_list_centres_its_block()
  var list = SettingsEditScene([IntEditCfg("count", 1, "Count", nil, 1, 0, 10, false)],
                               {"count": 3}, "TITLE")
  page_box(list).activate()

  var block_h = SettingsView.row + SettingsView.gap + SettingsView.row
  assert_eq(list.list.dim.h, SettingsView.row, "the list is not one row tall")
  assert_true(list.list.dim.w > 0, "the list has no width to fill")
  var left = list.list.pos.x - list.pos.x
  var right = list.pos.x + list.dim.w - (list.list.pos.x + list.list.dim.w)
  assert_true(left - right < 2 && right - left < 2, "the list is not centred across the scene")
  assert_eq(list.list.pos.y,
            list.pos.y + (list.dim.h - block_h) / 2 + SettingsView.row + SettingsView.gap,
            "the rows do not start under the centred heading")
end

test("a short list centres its block", a_short_list_centres_its_block)

# A choice with no entries — the device's jingle row on an image that ships no library — is a row
# with nothing to pick: it opens on no entry, steps nowhere, and closes on OK without a value to pop.
def a_choice_with_no_entries_hands_back_nothing()
  var scene = ChoiceEditScene("Mode", nil, [], false)
  page_box(scene).activate()
  assert_nil(scene.value(), "an empty choice names a value")
  assert_eq(scene.text(), "--", "an empty choice prints a value")

  var depth = ux.depth()
  ux.push(scene)
  harness.beat(1)
  harness.press("up:single")
  harness.beat(1)
  harness.press("ok:single")
  harness.beat(1)
  assert_eq(ux.depth(), depth, "an empty choice did not close on OK")
end

test("a choice with no entries hands back nothing", a_choice_with_no_entries_hands_back_nothing)

def a_bool_switch_divides_its_band_evenly()
  var flags = page_box(BoolEditScene("Flags", true, false))
  flags.activate()

  var page = flags.page
  var cells = page.children[2].children[0].children
  assert_eq(cells.size(), 2, "a two-entry switch does not have two cells")
  assert_eq(cells[0].pos.x, page.pos.x, "the first cell is not at the content's left")
  assert_eq(cells[0].dim.h, flags.body_h(), "a cell is not the band's height")
  assert_eq(cells[1].pos.x, cells[0].pos.x + cells[0].dim.w,
            "the second cell does not follow the first")
  assert_eq(cells[1].pos.x + cells[1].dim.w, page.pos.x + page.dim.w,
            "the cells do not fill the band")
  assert_true(cells[0].dim.w - cells[1].dim.w < 2 && cells[1].dim.w - cells[0].dim.w < 2,
              "the cells are not equal shares of the band")
  assert_eq(cells[0].children[0].scale, cells[1].children[0].scale,
            "the cells are not drawn at one size")
  assert_true(cells[1].children[0].inverted, "the entry showing is not the one inverted")
  assert_true(!cells[0].children[0].inverted, "an entry that is not showing is inverted")
end

test("a bool switch divides its band evenly", a_bool_switch_divides_its_band_evenly)

def a_nullable_bool_switch_carries_its_nothing()
  var flags = page_box(BoolEditScene("Flags", nil, true))
  flags.activate()

  var page = flags.page
  var cells = page.children[2].children[0].children
  assert_eq(cells.size(), 3, "a nullable switch does not have three cells")
  assert_eq(cells[2].pos.x + cells[2].dim.w, page.pos.x + page.dim.w,
            "the three cells do not fill the band")
  assert_true(cells[0].children[0].inverted, "the entry showing is not the one inverted")
end

test("a nullable bool switch carries its nothing", a_nullable_bool_switch_carries_its_nothing)

# Every scene draws in the page: a scene pushed over another one covers the whole page rather than a
# window inside it, and what the scene below drew comes back when it redraws.
class PageScene : Scene
  def draw_page()
    screen.draw_fillrect(self.pos.x, self.pos.y, self.dim.w, self.dim.h)
  end
end

def a_scene_opened_over_another_takes_the_page()
  assert_true(ux.push(PageScene()), "the page scene would not push")

  # A scene is given the whole page as it is pushed, over whatever was there: its box is the page,
  # not a window inside the scene below.
  var list = SettingsEditScene([IntEditCfg("count", 1, "Count", nil, 1, 0, 10, false)],
                           {"count": 1}, nil)
  assert_true(ux.push(list), "the settings list would not push")
  assert_eq(list.pos.x, 0, "the settings list does not start at the left of the page")
  assert_eq(list.pos.y, 0, "the settings list does not start at the top of the page")
  assert_eq(list.dim.w, 128, "the settings list is not the page's width")
  assert_eq(list.dim.h, 52, "the settings list is not the page's height")

  var editor = IntEditScene("Count", 2, 1, 0, 10, false)
  assert_true(ux.push(editor), "the value editor would not push")
  assert_eq(editor.pos.x, 0, "the value editor does not start at the left of the page")
  assert_eq(editor.pos.y, 0, "the value editor does not start at the top of the page")
  assert_eq(editor.dim.w, 128, "the value editor is not the page's width")

  ux.pop_to_root(nil)
end

test("a scene opened over another takes the page", a_scene_opened_over_another_takes_the_page)

# A cancel is the row as it stands: the editor hands back the value it was opened on, so a row that
# was stepped and then refused keeps what it held - including nothing, which a nullable setting can
# hold and commit - and the title the list was opened with stands.
def cancelling_a_value_leaves_its_row_alone()
  var list = SettingsEditScene([PressureEditCfg("rest", nil, "Rest", nil, 1, true)],
                           {"rest": nil}, "MY ROUTINE")
  var depth = ux.depth()
  assert_true(ux.push(list), "the settings list would not push")
  harness.beat(1)

  harness.press("forward:single")
  harness.beat(1)
  harness.press("up:single")
  harness.beat(1)
  harness.press("cancel:single")
  harness.beat(1)

  assert_nil(list.settings_values["rest"], "a cancelled edit changed the row's value")
  assert_eq(list.title, "MY ROUTINE", "a cancelled edit took the title with it")
  assert_eq(ux.depth(), depth + 1, "Cancel did not close the value scene")

  ux.pop(nil)
  harness.beat(1)
end

test("a cancel leaves its row alone", cancelling_a_value_leaves_its_row_alone)

# The title is the list's heading rather than part of the set, so a value that commits keeps it.
def committing_a_value_keeps_the_title()
  var list = SettingsEditScene([IntEditCfg("count", 1, "Count", nil, 1, 0, 10, false)],
                           {"count": 1}, "MY ROUTINE")
  var depth = ux.depth()
  assert_true(ux.push(list), "the settings list would not push")
  harness.beat(1)

  harness.press("forward:single")
  harness.beat(1)
  harness.press("up:single")
  harness.beat(1)
  harness.press("ok:single")
  harness.beat(1)

  assert_eq(list.settings_values["count"], 2, "a committed edit did not change the row's value")
  assert_eq(list.title, "MY ROUTINE", "a committed edit took the title with it")
  assert_eq(ux.depth(), depth + 1, "OK did not close the value scene")

  ux.pop(nil)
  harness.beat(1)
end

test("a committed edit keeps the title", committing_a_value_keeps_the_title)

# Each kind draws itself from that window. A case pushes its editor and beats once, which is the
# frame the runner captures — the picture to look at. The nil cases are the ones with no value to
# show and no bar or highlight to place.
def show_editor(scene)
  assert_true(ux.push(scene), "the editor would not push")
  harness.beat(1)
end

def the_bool_editor_draws_on()
  show_editor(BoolEditScene("Flags", true, false))
end

test("the bool editor draws ON", the_bool_editor_draws_on)

def the_bool_editor_draws_off()
  show_editor(BoolEditScene("Flags", false, false))
end

test("the bool editor draws OFF", the_bool_editor_draws_off)

def the_bool_editor_draws_nothing()
  show_editor(BoolEditScene("Flags", nil, true))
end

test("the bool editor draws a nullable nothing", the_bool_editor_draws_nothing)

def the_choice_editor_draws_its_list()
  show_editor(ChoiceEditScene("Mode", "b", choice_items(["a", "b", "c", "d", "e"]), false))
end

test("the choice editor draws its list", the_choice_editor_draws_its_list)

def the_choice_editor_draws_nothing()
  show_editor(ChoiceEditScene("Mode", nil, choice_items(["a", "b"]), true))
end

test("the choice editor draws a nullable nothing", the_choice_editor_draws_nothing)

def the_int_editor_draws_between_its_bounds()
  show_editor(IntEditScene("Count", 7, 1, 1, 20, false))
end

test("the int editor draws between its bounds", the_int_editor_draws_between_its_bounds)

def the_real_editor_draws_without_a_bar()
  show_editor(RealEditScene("Gain", 0.5, 0.5, nil, nil, false))
end

test("the real editor draws without a bar", the_real_editor_draws_without_a_bar)

def the_pressure_editor_draws_in_the_panel_unit()
  show_editor(PressureEditScene("Rest", 5.0, 1, false))
end

test("the pressure editor draws in the panel unit", the_pressure_editor_draws_in_the_panel_unit)

def the_duration_editor_draws_its_minutes()
  show_editor(DurationEditScene("Hold", 90, 1, 0, 7200, false))
end

test("the duration editor draws its minutes", the_duration_editor_draws_its_minutes)

def the_duration_editor_draws_its_seconds()
  var scene = DurationEditScene("Hold", 90, 1, 0, 7200, false)
  scene.stage = "sec"
  show_editor(scene)
end

test("the duration editor draws its seconds", the_duration_editor_draws_its_seconds)
