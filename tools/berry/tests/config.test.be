# config.test.be — the document instance a page opens through device.config(): a key read through
# its fallback, the document held until save() writes it, and no write at all when nothing changed.
# A path outside /cfg is refused.

suite = "config"

def a_missing_document_reads_its_fallbacks()
  var document = device.config("routines/config_test.ini")
  assert_eq(document.get("", "count", 7), 7, "an absent int is not its default")
  assert_eq(document.get("", "gain", 0.5), 0.5, "an absent real is not its default")
  assert_eq(document.get("", "label", "none"), "none", "an absent string is not its default")

  document.set("", "count", 3)
  assert_true(document.save(), "a document that is not there would not take a save")

  var reopened = device.config("routines/config_test.ini")
  assert_eq(reopened.get("", "count", 0), 3, "a saved value is not in the document")
end

test("a missing document reads its fallbacks and is created by save",
     a_missing_document_reads_its_fallbacks)

def one_save_writes_the_whole_document_once()
  var document = device.config("routines/config_test.ini")
  document.set("", "count", 4)
  document.set("", "gain", 0.25)

  var before = harness.writes()
  assert_true(document.save(), "a save of two changed keys is not accepted")
  assert_eq(harness.writes() - before, 1, "a save of two keys is not one write")

  var reopened = device.config("routines/config_test.ini")
  assert_eq(reopened.get("", "count", 0), 4, "a save did not write the first key")
  assert_eq(reopened.get("", "gain", 0.0), 0.25, "a save did not write the second key")
end

test("one save writes the whole document once", one_save_writes_the_whole_document_once)

def a_save_with_nothing_changed_writes_nothing()
  var document = device.config("routines/config_test.ini")
  var held = document.get("", "count", 0)
  document.set("", "count", held)

  var before = harness.writes()
  assert_true(document.save(), "a save with nothing changed is not accepted")
  assert_eq(harness.writes(), before, "a save with nothing changed wrote the file")
end

test("a save with nothing changed writes nothing", a_save_with_nothing_changed_writes_nothing)

def section_keys_name_the_sections_holding_keys()
  var document = device.config("routines/config_test.ini")
  document.set("extra", "note", "kept")
  assert_true(document.save(), "a sectioned write is not accepted")

  var reopened = device.config("routines/config_test.ini")
  var found = false
  for name : reopened.section_keys()
    if name == "extra"
      found = true
    end
  end
  assert_true(found, "a non-empty section is not named")
end

test("section keys name the sections holding keys", section_keys_name_the_sections_holding_keys)

def a_path_outside_cfg_is_refused()
  var refused = false
  try
    device.config("../settings.ini")
  except .. as e, m
    refused = e == "value_error"
  end
  assert_true(refused, "a path that climbs is not refused with value_error")

  refused = false
  try
    device.config("")
  except .. as e, m
    refused = e == "value_error"
  end
  assert_true(refused, "an empty path is not refused with value_error")
end

test("a path outside /cfg is refused", a_path_outside_cfg_is_refused)
