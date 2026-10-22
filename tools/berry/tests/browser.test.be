# browser.test.be — the names list is the browser's, so hidden pages stay out of it and every
# other registration is in it.

suite = "browser"

def registrations_listed()
  var names = routines.names()
  for name : harness.entries()
    var found = false
    for listed : names
      if listed == name
        found = true
      end
    end
    if harness.hidden(name)
      assert_true(!found, "routines.names() lists hidden " + name)
    else
      assert_true(found, "routines.names() is missing " + name)
    end
  end
end

test("every registration is listed unless hidden", registrations_listed)
