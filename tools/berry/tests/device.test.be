# device.test.be — the jingle library is data in /cfg/jingles.ini, answered as records:
# device.all_jingles() carries every entry this build can play, in file order, and
# device.play_current_jingle() sounds the one `[device] jingle` selects, falling back to the first.

suite = "device"

def library_resolves()
  var all = device.all_jingles()
  assert_true(all.size() > 0, "device: the library answered no jingles")
  for j : all
    assert_true(type(j.key) == "string", "device: a non-string key")
    assert_true(j.key != "", "device: an empty key")
    assert_true(j.full_title != nil && j.full_title != "", "device: " + j.key + " has no full title")
    assert_true(j.short_title != nil && j.short_title != "", "device: " + j.key + " has no short title")
    assert_true(j.notes != nil && j.notes != "", "device: " + j.key + " has no notes")
  end

  # The readout's row is the same list in the same order, and the page helper finds one entry.
  var choices = jingle_choices()
  assert_eq(choices.size(), all.size(), "device: the row does not carry every jingle")
  var i = 0
  while i < all.size()
    assert_eq(choices[i][0], all[i].key, "device: a row key is not the record's")
    assert_eq(choices[i][1], all[i].short_title, "device: a row label is not the record's")
    i += 1
  end
  assert_eq(jingle_notes(all[0].key), all[0].notes, "device: the helper does not find a record")
  assert_nil(jingle_notes("no_such_jingle"), "device: the helper answered an unknown key")
end

test("every jingle answers the record's fields", library_resolves)

def current_follows()
  var all = device.all_jingles()
  var before = device.jingle()
  device.set_jingle(all[0].key)
  var sounds = harness.buzzer_sounds()
  assert_true(device.play_current_jingle(), "device: play_current_jingle() returned false")
  assert_eq(harness.buzzer_sounds(), sounds + 1, "device: play_current_jingle() did not play")

  # An identifier the library does not hold plays the first entry, as the row shows it.
  device.set_jingle("no_such_jingle")
  assert_true(device.play_current_jingle(), "device: an unknown selection played nothing")
  assert_eq(harness.buzzer_sounds(), sounds + 2, "device: an unknown selection did not fall back")
  device.set_jingle(before)
end

test("play_current_jingle follows the setting and falls back", current_follows)
