# browser.test.be — the browser's way into a routine: Forward opens the selected routine's settings
# and writes what the list settles on, and OK launches it with the values in its file. The rows the
# browser lists belong to the registry and are tested with it.

suite = "browser"

# The document a routine's settings live in, opened fresh so a value a page wrote is seen by the
# next read, and one key written through it.
def settings_document(name)
  return device.config("routines/" + name + ".ini")
end

def write_setting(name, key, value)
  var document = settings_document(name)
  document.set("", key, value)
  document.save()
end

# Forward is the browser's way into a routine's settings. The browser reads the values and writes
# what the list settles on, so a value stepped in the list lands in the routine's file.
def forward_edits_the_selected_routines_settings()
  ux.pop_to_root()
  assert_true(ux.push(browserScene), "the browser would not push")

  var at = -1
  var i = 0
  while i < browserScene.list.rows.size()
    if browserScene.list.rows[i].value == "ramped_static"
      at = i
    end
    i += 1
  end
  assert_true(at >= 0, "the browser does not list ramped_static")
  browserScene.list.selected = at

  var before = settings_document("ramped_static").get("", "initial_pressure", 0.0)

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

  # The list keeps the set on OK; Back would have closed it on the set it was opened with. The set
  # is written and the routine is launched with it, so the page takes the top of the stack.
  harness.press("ok:single")
  harness.beat(1)
  harness.beat(1)
  assert_eq(ux.depth(), 3, "OK did not close the settings list and launch the routine")

  var after = settings_document("ramped_static").get("", "initial_pressure", 0.0)
  assert_true(after != before, "the browser did not write the value the list settled on")
  assert_eq(ux.top().start_pressure, after, "the launched page did not start on the settled set")
  ux.pop_to_root()
  # The suites share one store, so the value this test stepped goes back where it was.
  write_setting("ramped_static", "initial_pressure", before)
end

test("Forward edits the selected routine's settings", forward_edits_the_selected_routines_settings)

# OK is the browser's way into a routine: the scene is built from the routine's own document, so the
# page starts on the values its file holds.
def ok_launches_the_selected_routine()
  ux.pop_to_root()
  var before = settings_document("ramped_static").get("", "initial_pressure", 0.0)
  write_setting("ramped_static", "initial_pressure", 7.5)

  assert_true(ux.push(browserScene), "the browser would not push")
  var at = -1
  var i = 0
  while i < browserScene.list.rows.size()
    if browserScene.list.rows[i].value == "ramped_static"
      at = i
    end
    i += 1
  end
  assert_true(at >= 0, "the browser does not list ramped_static")
  browserScene.list.selected = at

  harness.press("ok:single")
  harness.beat(1)

  assert_eq(ux.depth(), 3, "OK did not launch the selected routine")
  assert_eq(ux.top().start_pressure, 7.5,
            "the launched page did not start on the values in its file")

  ux.pop_to_root()
  write_setting("ramped_static", "initial_pressure", before)
end

test("OK launches the selected routine on its values", ok_launches_the_selected_routine)
