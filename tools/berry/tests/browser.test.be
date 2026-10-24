# browser.test.be — the names list is the browser's, so it carries every registration, and the rows
# carry what each registration gave: the routine's name, and its icon when it has one. The browser
# is also a caller that opens a routine's settings, so its Forward is driven here.

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

# Forward is the browser's way into a routine's settings. The browser reads the values and writes
# what the list settles on, so a value stepped in the list lands in the routine's file.
def forward_edits_the_selected_routines_settings()
  ux.pop_to_root()
  assert_true(ux.push(browserScene), "the browser would not push")

  var at = -1
  var i = 0
  while i < browserScene.names.size()
    if browserScene.names[i] == "ramped_static"
      at = i
    end
    i += 1
  end
  assert_true(at >= 0, "the browser does not list ramped_static")
  browserScene.list.selected = at

  var before = store.get("ramped_static", "initial_pressure", 0.0)

  harness.press("forward:single")
  harness.beat(1)
  assert_eq(ux.depth(), 3, "Forward did not open the selected routine's settings")

  # The first row is the start pressure; Up steps it, and OK keeps the step.
  harness.press("forward:single")
  harness.beat(1)
  harness.press("up:single")
  harness.beat(1)
  harness.press("ok:single")
  harness.beat(1)

  harness.press("back:single")
  harness.beat(1)
  assert_eq(ux.depth(), 2, "Back did not close the settings list")

  var after = store.get("ramped_static", "initial_pressure", 0.0)
  assert_true(after != before, "the browser did not write the value the list settled on")
  ux.pop_to_root()
end

test("Forward edits the selected routine's settings", forward_edits_the_selected_routines_settings)
