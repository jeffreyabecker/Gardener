# settings.test.be — the module a routine declares its values with: the record a declaration is read back
# through, the bounds that kind was declared with, and the rule that one declaration is one row on one
# record. Across the Berry boundary, so the host suite that compiles the firmware cannot see it.

suite = "settings"

# A record of the suite's own, holding the declarations a case hands over. The key is the case's, so a
# record a case registers replaces the one the case before it left.
def declare(key, rows)
  return routines.register(key, key, nil, rows, Scene)
end

def a_record_is_the_declaration_it_was_made_from()
  var rows = [
    settings.number("count", 4, "Sets", nil, 1, 1, 20, false, true),
    settings.pressure("target", 5.0, "Target", nil, 1, false)
  ]
  assert_true(declare("settings_probe", rows), "a record would not register")

  var cfgs = routines.settings_cfgs("settings_probe")
  assert_not_nil(cfgs, "a registered record declares nothing")
  assert_eq(cfgs.size(), 2, "the record does not hold both declarations")

  var number = cfgs[0]
  assert_true(isinstance(number, RoutineSetting), "a declaration is not a RoutineSetting")
  assert_eq(number.key, "count", "the record's key is not the declaration's")
  assert_eq(number.label, "Sets", "the record's label is not the declaration's")
  assert_eq(number.kind, "number", "the record's kind is not the declaration's")
  assert_eq(number.min, 1, "the record's lower bound is not the declaration's")
  assert_eq(number.max, 20, "the record's upper bound is not the declaration's")

  var pressure = cfgs[1]
  assert_eq(pressure.kind, "pressure", "a pressure declaration is not a pressure")
  assert_eq(pressure.label, "Target", "the pressure's label is not the declaration's")
  assert_eq(pressure.key, "target", "the pressure's key is not the declaration's")
end

test("a record is the declaration it was made from", a_record_is_the_declaration_it_was_made_from)

def a_declaration_is_one_row_on_one_record()
  var row = settings.number("only", 1, "Only", nil, 1, 1, 9, false, true)
  assert_true(declare("settings_probe_first", [row]), "the first record would not register")

  # The row went to the first record, so the second takes the same declaration and holds nothing: one
  # setting is one row on one record, and a declaration is not shared.
  assert_true(declare("settings_probe_second", [row]), "the second record would not register")
  assert_nil(routines.settings_cfgs("settings_probe_second"),
             "a consumed declaration was taken a second time")
  assert_eq(routines.settings_cfgs("settings_probe_first").size(), 1,
            "the first record lost the row to the second")
end

test("a declaration is one row on one record", a_declaration_is_one_row_on_one_record)

def a_list_that_holds_another_value_is_refused()
  assert_true(declare("settings_probe_kept", [settings.number("kept", 1, "Kept", nil, 1, 1, 9, false, true)]),
              "the kept record would not register")

  var refused = false
  try
    declare("settings_probe_refused", [settings.number("a", 1, "A", nil, 1, 1, 9, false, true), "not a setting"])
  except .. as kind, message
    refused = true
    assert_eq(kind, "type_error", "a bad list is refused as " + kind)
  end
  assert_true(refused, "a list that is not all declarations was taken")

  # Nothing was consumed on the way to the refusal, so the record that was there still declares its row.
  var kept = routines.settings_cfgs("settings_probe_kept")
  assert_not_nil(kept, "a refused registration took a record's rows with it")
  assert_eq(kept.size(), 1, "a refused registration changed another record's rows")
end

test("a list that holds another value is refused", a_list_that_holds_another_value_is_refused)

# --- the device's own list -------------------------------------------------

def the_system_list_writes_the_set_it_settles_on()
  # The home page's own entry opens the device's list over it. Down moves from the unit to the jingle
  # and then to the maximum, Forward opens the duration and Up takes one minute, and OK settles both
  # the editor and the list. The set the list settles on is written back through `device`.
  assert_true(open_system_settings(), "the system list would not open")
  harness.beat(1)
  assert_eq(ux.depth(), 2, "the system list did not go over the home page")

  harness.press("down:single")
  harness.beat(1)
  harness.press("down:single")
  harness.beat(1)
  harness.press("forward:single")
  harness.beat(1)
  harness.press("up:single")
  harness.beat(1)
  harness.press("ok:single")
  harness.beat(1)
  harness.press("ok:single")
  harness.beat(1)

  assert_eq(ux.depth(), 1, "OK did not close the system list")
  assert_eq(harness.setting("max_duration"), "960",
            "the settled maximum did not reach the document")
end

test("the system list writes the set it settles on", the_system_list_writes_the_set_it_settles_on)

def the_jingle_row_auditions_the_entry_it_shows()
  assert_true(open_system_settings(), "the system list would not open")
  harness.beat(1)

  # Down from the unit row to the jingle. Forward opens its entries and previews the one showing;
  # Up steps to the entry above it and previews that one; Forward previews the entry showing again.
  harness.press("down:single")
  harness.beat(1)

  device.silence()
  var sounds = harness.buzzer_sounds()
  harness.press("forward:single")
  harness.beat(1)
  assert_true(harness.buzzer_sounds() > sounds, "opening the jingle row did not preview it")

  device.silence()
  sounds = harness.buzzer_sounds()
  harness.press("up:single")
  harness.beat(1)
  assert_true(harness.buzzer_sounds() > sounds, "stepping to a jingle did not preview it")

  device.silence()
  sounds = harness.buzzer_sounds()
  harness.press("forward:single")
  harness.beat(1)
  assert_true(harness.buzzer_sounds() > sounds, "Forward did not preview the jingle")

  # Leave: the editor on Cancel and the list on Cancel, neither settling a change.
  harness.press("cancel:single")
  harness.beat(1)
  harness.press("cancel:single")
  harness.beat(1)
  assert_eq(ux.depth(), 1, "Cancel did not close the system list")
end

test("the jingle row auditions the entry it shows",
     the_jingle_row_auditions_the_entry_it_shows)
