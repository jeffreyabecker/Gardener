# browser.test.be — the names list is the browser's, so it carries every registration, and the rows
# carry what each registration gave: the routine's name, and its icon when it has one.

suite = "browser"

def every_registration_is_listed()
  var names = routines.names()
  var rows = routines.rows()
  assert_eq(rows.size(), names.size(), "the browser has a row for every routine")
  var i = 0
  while i < names.size()
    assert_eq(rows[i][1], routines.title(names[i]), "the browser's row " + str(i) + " does not carry the routine's title")
    if routines.icon(names[i]) != nil
      assert_eq(rows[i][0], routines.icon(names[i]), "the row does not carry the routine's icon")
    end
    i += 1
  end
end

test("every registration is listed with its icon", every_registration_is_listed)

def shipped_routines_are_listed()
  for name : ["interval", "ramped_interval", "ramped_static"]
    var found = false
    for listed : routines.names()
      if listed == name
        found = true
      end
    end
    assert_true(found, "routines.names() is missing " + name)
  end
end

test("the shipped routines are listed", shipped_routines_are_listed)
