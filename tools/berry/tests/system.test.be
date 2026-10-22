# system.test.be — the System page is the only editor of the global jingle, and it has no
# describe() for the shared editor walk to cover. Walk its jingle row with the real key path and
# make sure the identifier moves.

suite = "system"

def jingle_editor_walks()
  assert_not_nil(routines.get("system"), "system: not registered")
  harness_activate("system", nil)
  # Rows: UNIT, TGT, BT, JINGLE. Three Downs select the last.
  var row = 0
  while row < 3
    harness.press("down", "short")
    harness.beat(1)
    row += 1
  end
  harness.press("ok", "short")
  harness.beat(1)
  var page = routines.get("system")
  assert_true(page.editing, "system: OK did not open the jingle editor")
  var before = device.jingle()
  harness.press("up", "short")
  harness.beat(1)
  harness.press("ok", "short")
  harness.beat(1)
  assert_true(device.jingle() != before, "system: the jingle did not change")
  device.set_jingle(before)
end

test("the jingle editor walks and changes the jingle", jingle_editor_walks)
