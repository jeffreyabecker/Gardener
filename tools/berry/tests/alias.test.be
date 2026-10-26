# alias.test.be — alternates a routine's document names: every [section] is an alias, a second key
# that shares the routine's factory, settings declaration and icon but carries its own title and
# settings set. The routine's own set stays in the global section.
#
# The cases register a test-local routine over a seeded document, so discovery runs for real against
# the same call a routine file makes. `alias_test` is only used here: other suites list it but their
# assertions hold for any extra registration.

suite = "alias"

def has(list, value)
  for item : list
    if item == value
      return true
    end
  end
  return false
end

def index_of(list, value)
  var i = 0
  while i < list.size()
    if list[i] == value
      return i
    end
    i += 1
  end
  return -1
end

# The document the alias_test routine reads, seeded with a root set and two aliases: [cold] with a
# routine_title, and [warm] without, then the routine re-registered so discovery scans it.
def seed_alias_test()
  var document = device.config("routines/alias_test.ini")
  document.set("", "initial_pressure", 5.0)
  document.set("", "target_pressure", 20.0)
  document.set("", "initial_duration", 60)
  document.set("", "target_duration", 60)
  document.set("", "total_duration", 300)
  document.set("cold", "routine_title", "Cold")
  document.set("cold", "initial_pressure", 10.0)
  document.set("cold", "target_pressure", 15.0)
  document.set("warm", "target_pressure", 30.0)
  document.save()
  routines.register(RampedStaticSettings("alias_test", "Alias Test",
                                         RampedStaticIcon(),
                                         RampedStatic))
end

def every_named_section_is_an_alias()
  seed_alias_test()
  var keys = routines.keys()
  assert_true(has(keys, "cold"), "an alias section is not a key")
  assert_true(has(keys, "warm"), "an alias without routine_title is not a key")
  assert_true(index_of(keys, "alias_test") < index_of(keys, "cold"),
              "an alias is not listed after its routine")
end

test("every named section is an alias", every_named_section_is_an_alias)

def an_alias_is_titled_by_its_section()
  seed_alias_test()
  assert_eq(routines.title("cold"), "Cold", "an alias does not show its routine_title")
  assert_eq(routines.title("warm"), "warm", "an alias without routine_title does not fall back to its key")
  assert_eq(routines.title("alias_test"), "Alias Test", "a routine's own title was disturbed")
end

test("an alias is titled by its section", an_alias_is_titled_by_its_section)

def an_alias_resolves_to_its_parent()
  seed_alias_test()
  assert_eq(routines.entry("cold"), routines.entry("alias_test"),
            "an alias does not resolve to its parent's registration")
  assert_eq(routines.icon("cold"), routines.icon("alias_test"),
            "an alias does not share its parent's icon")

  var rows = routines.rows()
  var cold_row = nil
  for row : rows
    if row.value == "cold"
      cold_row = row
    end
  end
  assert_not_nil(cold_row, "an alias has no row")
  assert_eq(cold_row.title, "Cold", "an alias row does not show its title")
  assert_eq(cold_row.icon, routines.icon("alias_test"), "an alias row does not share the parent's icon")
end

test("an alias resolves to its parent", an_alias_resolves_to_its_parent)

def an_alias_reads_its_own_section()
  seed_alias_test()
  var values = routines.load_settings("cold")
  assert_eq(values["initial_pressure"], 10.0, "an alias does not read its section value")
  assert_eq(values["target_pressure"], 15.0, "an alias does not read its second section value")
  assert_eq(values["total_duration"], 300, "an alias missing a key does not read its default")
  assert_eq(values["routine_title"], "Cold", "an alias does not carry its routine_title")

  # The root set stays global: it neither takes an alias's value nor carries its title.
  var root = routines.load_settings("alias_test")
  assert_eq(root["initial_pressure"], 5.0, "the root set does not read the global section")
  assert_nil(root.find("routine_title"), "the root set carries an alias's routine_title")
end

test("an alias reads its own section", an_alias_reads_its_own_section)

def an_alias_set_is_repaired()
  seed_alias_test()
  var document = device.config("routines/alias_test.ini")
  document.set("cold", "total_duration", 30)
  document.save()

  var values = routines.load_settings("cold")
  assert_eq(values["total_duration"], 300, "an alias set the rule refuses is not put back")

  document.set("cold", "total_duration", 300)
  document.save()
end

test("an alias set is repaired", an_alias_set_is_repaired)

def an_alias_saves_to_its_own_section()
  seed_alias_test()
  var before_root = device.config("routines/alias_test.ini").get("", "initial_pressure", 0.0)

  var values = routines.load_settings("cold")
  values["initial_pressure"] = 7.0
  assert_true(routines.save_settings("cold", values), "an alias save was refused")

  var document = device.config("routines/alias_test.ini")
  assert_eq(document.get("cold", "initial_pressure", 0.0), 7.0,
            "an alias save did not write its section")
  assert_eq(document.get("", "initial_pressure", 0.0), before_root,
            "an alias save touched the root config")
  assert_eq(document.get("cold", "routine_title", ""), "Cold",
            "an alias save lost its routine_title")
end

test("an alias saves to its own section", an_alias_saves_to_its_own_section)
