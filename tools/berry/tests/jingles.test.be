# jingles.test.be — the jingle library is data in /modules/jingles.txt: every identifier must
# resolve to a title, a label and notes, and current() follows the device setting and falls back.

suite = "jingles"

def library_resolves()
  var ids = jingles.ids()
  assert_true(ids.size() > 0, "jingles: no identifiers")
  for id : ids
    assert_true(type(id) == "string", "jingles: a non-string identifier")
    assert_true(id != "", "jingles: an empty identifier")
    assert_not_nil(jingles.title(id), "jingles: " + id + " has no title")
    assert_not_nil(jingles.label(id), "jingles: " + id + " has no label")
    assert_true(jingles.notes(id) != nil && jingles.notes(id) != "",
                "jingles: " + id + " has no notes")
  end
  assert_nil(jingles.notes("no_such_jingle"), "jingles: an unknown identifier answered notes")
end

test("library resolves every identifier", library_resolves)

def current_follows()
  var ids = jingles.ids()
  var before = device.jingle()
  device.set_jingle(ids[0])
  assert_eq(jingles.current(), jingles.notes(ids[0]),
            "jingles: current() ignored the device setting")
  var sounds = harness.buzzer_sounds()
  assert_true(jingles.play_current(), "jingles: play_current() returned false")
  assert_eq(harness.buzzer_sounds(), sounds + 1, "jingles: play_current() did not play")
  device.set_jingle("no_such_jingle")
  assert_eq(jingles.current(), jingles.notes(ids[0]), "jingles: current() did not fall back")
  device.set_jingle(before)
end

test("current follows the device setting and falls back", current_follows)
