# system.test.be — the System scene is the only editor of the global jingle, and it is not a routine,
# so it registers no settings and the shared settings walk does not cover it. Walk its jingle row
# with the real key path and make sure the identifier moves.

suite = "system"

def jingle_editor_walks()
  assert_not_nil(systemScene, "system: not built")
  ux.push(systemScene)
  # Rows: UNIT, TGT, BT, JINGLE. Three Downs select the last.
  var row = 0
  while row < 3
    harness.press("down:short")
    harness.beat(1)
    row += 1
  end
  harness.press("ok:short")
  harness.beat(1)
  # The jingle scene is on top now, auditioning the value it opens on.
  assert_eq(ux.depth(), 3, "system: OK did not open the jingle editor")
  var before = device.jingle()
  harness.press("up:short")
  harness.beat(1)
  harness.press("ok:short")
  harness.beat(1)
  assert_true(device.jingle() != before, "system: the jingle did not change")
  device.set_jingle(before)
end

test("the jingle editor walks and changes the jingle", jingle_editor_walks)
