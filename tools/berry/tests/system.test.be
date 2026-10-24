# system.test.be — the System scene is the device's own settings list: it is not a routine, so it
# registers nothing and the shared settings walk does not cover it. Walk its rows with the real key
# path and make sure the device's settings move.

suite = "system"

def jingle_editor_walks()
  assert_not_nil(systemScene, "system: not built")
  ux.pop_to_root()
  ux.push(systemScene)
  var menu = ux.depth()
  # Rows: UNIT, BT, JINGLE. Start at the top and walk down to the last.
  systemScene.list.selected = 0
  var row = 0
  while row < 2
    harness.press("down:single")
    harness.beat(1)
    row += 1
  end
  # The menu is a settings list, so Forward opens the row's value scene.
  harness.press("forward:single")
  harness.beat(1)
  # The jingle scene is on top now, auditioning the value it opens on.
  assert_eq(ux.depth(), menu + 1, "system: Forward did not open the jingle editor")
  var before = device.jingle()
  harness.press("up:single")
  harness.beat(1)
  harness.press("ok:single")
  harness.beat(1)
  # The list writes the set it settled on when it leaves, the way a page writes its own list.
  harness.press("back:single")
  harness.beat(1)
  assert_true(device.jingle() != before, "system: the jingle did not change")
  device.set_jingle(before)
  ux.pop_to_root()
end

test("the jingle editor walks and changes the jingle", jingle_editor_walks)

def the_unit_row_changes_the_unit()
  assert_not_nil(systemScene, "system: not built")
  ux.pop_to_root()
  ux.push(systemScene)
  var menu = ux.depth()
  var before = screen.unit().name
  # The first row is the unit.
  systemScene.list.selected = 0
  harness.press("forward:single")
  harness.beat(1)
  assert_eq(ux.depth(), menu + 1, "system: Forward did not open the unit editor")
  harness.press("up:single")
  harness.beat(1)
  harness.press("ok:single")
  harness.beat(1)
  harness.press("back:single")
  harness.beat(1)
  assert_true(screen.unit().name != before, "system: the unit did not change")
  device.set_unit(before)
  ux.pop_to_root()
end

test("the unit row changes the device's unit", the_unit_row_changes_the_unit)

def the_radio_row_opens_nothing()
  assert_not_nil(systemScene, "system: not built")
  ux.pop_to_root()
  ux.push(systemScene)
  var menu = ux.depth()
  # The second row is the radio, which is a build fact: the row reports it and nothing edits it.
  systemScene.list.selected = 1
  harness.press("forward:single")
  harness.beat(1)
  assert_eq(ux.depth(), menu, "system: Forward opened an editor for a build fact")
  ux.pop_to_root()
end

test("the radio row opens nothing", the_radio_row_opens_nothing)
