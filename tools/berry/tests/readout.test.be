# readout.test.be — the readout is the device's own page: it registers nothing, so the shared
# settings walk does not cover it, and its scene cannot be built in the host harness. What it
# declares, and the device helpers its rows are built from, are checked here; the rows themselves
# are declaration only, and the readout writes the device when its list commits.

suite = "readout"

def the_declaration_is_the_device_rows()
  var settings_cfgs = ReadoutSettings.settings()
  # The target first, then the unit, the jingle and the radio last.
  assert_eq(settings_cfgs[0].key, "target_pressure", "readout: the first row is not the target")
  assert_eq(settings_cfgs[1].key, "unit", "readout: the second row is not the unit")
  var last = settings_cfgs[settings_cfgs.size() - 1]
  assert_eq(last.key, "bluetooth", "readout: the last row is not the radio")

  # The jingle row joins them only when the build ships a note library, which is the check the
  # system menu made before it listed the row.
  var jingle_rows = 0
  for cfg : settings_cfgs
    if cfg.key == "jingle"
      jingle_rows += 1
    end
  end
  if jingles.ids().size() > 0
    assert_eq(jingle_rows, 1, "readout: the jingle row is missing")
  else
    assert_eq(jingle_rows, 0, "readout: the jingle row is there without a library")
  end
end

test("the declaration is the device's rows, target first", the_declaration_is_the_device_rows)

def the_rows_open_their_editors()
  var settings_cfgs = ReadoutSettings.settings()
  for cfg : settings_cfgs
    if cfg.key == "target_pressure"
      var scene = cfg.scene(5.0)
      assert_eq(classname(scene), "PressureScene", "readout: the target row does not open a pressure editor")
    elif cfg.key == "unit"
      var scene = cfg.scene(screen.unit().name)
      assert_eq(classname(scene), "ChoiceScene", "readout: the unit row does not open a choice editor")
    elif cfg.key == "bluetooth"
      assert_nil(cfg.scene(true), "readout: the radio row opens an editor for a build fact")
    elif cfg.key == "jingle"
      var scene = cfg.scene(stored_jingle())
      assert_eq(classname(scene), "JingleScene", "readout: the jingle row does not audition")
    end
  end
end

test("each row opens its own editor, and the radio opens nothing", the_rows_open_their_editors)

def the_helpers_report_the_device()
  # The unit row's choices are the tokens the device answers, each titled in capitals.
  var units = device.units()
  var choices = unit_choices()
  assert_eq(choices.size(), units.size(), "readout: the unit row does not carry every unit")
  var i = 0
  while i < units.size()
    assert_eq(choices[i][0], units[i], "readout: a unit choice is not the token the device answers")
    assert_eq(choices[i][1], string.toupper(units[i]), "readout: a unit choice is not titled in capitals")
    i += 1
  end

  # The jingle row's choices are the library's, each titled with its label.
  var jingles_listed = jingle_choices()
  assert_eq(jingles_listed.size(), jingles.ids().size(), "readout: the jingle row does not carry every jingle")

  # A jingle the image does not ship falls back to the one the library plays by default.
  var before = device.jingle()
  device.set_jingle("no_such_jingle")
  assert_eq(stored_jingle(), jingles.default_id(), "readout: an unknown jingle does not fall back")
  device.set_jingle(before)
end

test("the device helpers report what the device answers", the_helpers_report_the_device)

# The jingle editor steps through the library and plays what it shows, so what it hands the buzzer is
# the identifier the entry stores rather than the entry itself.
def the_jingle_editor_steps_and_plays()
  if jingles.ids().size() < 2
    return
  end
  var scene = JingleSettingCfg().scene(stored_jingle())
  var before = scene.value()
  scene.nudge(1)
  assert_true(scene.value() != before, "readout: the jingle editor did not step")
  assert_true(jingles.notes(scene.value()) != nil, "readout: the jingle editor stepped to no jingle")
end

test("the jingle editor steps and plays", the_jingle_editor_steps_and_plays)
