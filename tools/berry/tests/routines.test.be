# routines.test.be — the registry a page is found through: the keys and rows it answers, the set it reads
# and writes, and the page a launch builds. A case registers a record of its own where it needs one, which
# adds a key to the registry for the rest of the run; the count a run's last line prints was read before
# any suite loaded, so a case's own record changes nothing about it.

suite = "routines"

def the_keys_are_every_registration_in_order()
  var keys = routines.keys()
  assert_eq(keys.size(), 3, "the image does not register three routines")
  assert_eq(keys[0], "ramped_interval", "the first key is not the ramp's")
  assert_eq(keys[1], "interval", "the second key is not the interval's")
  assert_eq(keys[2], "ramped_static", "the third key is not the static profile's")

  var before = keys.size()
  assert_true(routines.register("routines_probe", "Probe", nil, nil, Scene),
              "a record of the suite's own would not register")
  assert_eq(routines.keys().size(), before + 1, "a registration did not add a key")
  assert_eq(routines.keys()[before], "routines_probe", "the new key was not the last one")
end

test("the keys are every registration in order", the_keys_are_every_registration_in_order)

def the_rows_are_one_per_key()
  var keys = routines.keys()
  var rows = routines.rows()
  assert_eq(rows.size(), keys.size(), "the rows are not one per key")

  var i = 0
  while i < rows.size()
    assert_eq(rows[i].value, keys[i], "a row stands for another key")
    assert_eq(rows[i].title, routines.title(keys[i]), "a row carries another title")
    i += 1
  end
end

test("the rows are one per key", the_rows_are_one_per_key)

def a_key_nobody_registered_answers_nothing()
  assert_nil(routines.title("nobody_registered_this"), "an unknown key answers a title")
  assert_nil(routines.settings_cfgs("nobody_registered_this"), "an unknown key declares settings")
  assert_nil(routines.activate("nobody_registered_this", nil), "an unknown key launches a page")
end

test("a key nobody registered answers nothing", a_key_nobody_registered_answers_nothing)

def a_record_that_declares_nothing_answers_nil()
  assert_true(routines.register("routines_probe_none", "None", nil, nil, Scene),
              "a record that declares nothing would not register")
  assert_nil(routines.settings_cfgs("routines_probe_none"),
             "a record that declares nothing answers declarations")
  assert_true(routines.load_settings("routines_probe_none") != nil,
              "a record that declares nothing answers no set at all")
end

test("a record that declares nothing answers nil", a_record_that_declares_nothing_answers_nil)

def an_activation_fills_a_missing_setting_with_its_default()
  var stored = routines.load_settings("ramped_static")
  # One setting from the caller and the rest from the record: a set the caller half-states is held to
  # the declaration, so nothing a page reads is missing.
  ux.push(routines.activate("ramped_static", {"initial_pressure": 7.0}))
  var page = ux.top()
  assert_true(page != nil, "the activation built no page")

  var held = page.serialize_to_settings()
  assert_eq(held["initial_pressure"], 7.0, "the setting the caller gave was not taken")
  assert_eq(held["target_pressure"], stored["target_pressure"],
            "a setting the caller left out did not begin at its default")
end

test("an activation fills a missing setting with its default",
     an_activation_fills_a_missing_setting_with_its_default)

def a_save_writes_the_keys_it_changes()
  var settings = routines.load_settings("ramped_static")
  var writes = harness.writes()

  # A value the file does not hold is written, and the document is written once.
  settings["initial_pressure"] = 9.0
  assert_true(routines.save_settings("ramped_static", settings), "the set would not save")
  assert_eq(harness.writes(), writes + 1, "a save wrote the document more or less than once")
  assert_true(harness.wrote("ramped_static", "initial_pressure"), "the changed key was not written")
  assert_eq(routines.load_settings("ramped_static")["initial_pressure"], 9.0,
            "the set the save wrote is not the one it was handed")
end

test("a save writes the keys it changes", a_save_writes_the_keys_it_changes)

def a_key_the_map_leaves_out_is_written_as_nothing()
  # A set that names one setting: the save fills the rest from the declaration, so no key the record
  # declares is left out of the document.
  assert_true(routines.save_settings("ramped_static", {"initial_pressure": 8.0}),
              "a set naming one setting would not save")

  var stored = routines.load_settings("ramped_static")
  for cfg : routines.settings_cfgs("ramped_static")
    assert_true(stored.contains(cfg.key), "the save left " + cfg.key + " out of the set")
  end
  assert_eq(stored["initial_pressure"], 8.0, "the setting the set named was not saved")
end

test("a key the map leaves out is written as nothing", a_key_the_map_leaves_out_is_written_as_nothing)
