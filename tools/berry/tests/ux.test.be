# ux.test.be — the stack a page moves through: what it answers, how deep it goes, which page is on top and
# what a popped one hands back. The pages it pushes are the image's own where a case needs behaviour, so
# the answers are the ones a page sees rather than a mock's.

suite = "ux"

def the_root_is_one_page_showing()
  assert_eq(ux.depth(), 1, "a reset stack is not one page")
  var root = ux.top()
  assert_true(root != nil, "the root answers no page")
  assert_eq(classname(root), "HomeScene", "the page showing is not home")
end

test("the root is one page showing", the_root_is_one_page_showing)

def a_push_answers_and_puts_its_page_on_top()
  var home_depth = ux.depth()
  assert_true(ux.push(BrowserScene()), "the browser would not push")
  harness.beat(1)
  assert_eq(ux.depth(), home_depth + 1, "a push did not deepen the stack")
  assert_eq(classname(ux.top()), "BrowserScene", "the page on top is not the one pushed")

  assert_true(ux.pop(), "a pop was refused over the root")
  harness.beat(1)
  assert_eq(ux.depth(), home_depth, "a pop did not leave the stack where it was")
  assert_eq(classname(ux.top()), "HomeScene", "the pop did not bring the root back")
end

test("a push answers and puts its page on top", a_push_answers_and_puts_its_page_on_top)

def nothing_pushed_is_refused()
  var depth = ux.depth()
  assert_true(!ux.push(nil), "a push of nothing was taken")
  assert_eq(ux.depth(), depth, "a push of nothing changed the stack")
  assert_true(!ux.pop(), "a pop over the root was taken")
  assert_eq(ux.depth(), depth, "a refused pop changed the stack")
end

test("nothing pushed is refused", nothing_pushed_is_refused)

def the_stack_stops_at_its_cap()
  var page = BrowserScene()
  var i = 0
  while i < 12
    ux.push(page)
    i += 1
  end
  assert_eq(ux.depth(), 8, "the stack went past its cap")
end

test("the stack stops at its cap", the_stack_stops_at_its_cap)

def a_handle_is_pushed_once()
  var handle = routines.activate("ramped_static", nil)
  assert_true(handle != nil, "the activation answered no handle")
  assert_true(ux.push(handle), "a launch would not push")
  assert_true(!ux.push(handle), "a handle was pushed twice")
end

test("a handle is pushed once", a_handle_is_pushed_once)

# What a popped page handed the stack, and how many callbacks ran: the values a case reads back out of a
# callback, which a lambda cannot keep because it would have to assign one.
ux_popped = nil
ux_callbacks = 0

def ux_keep(value)
  ux_popped = value
end

def ux_count(value)
  ux_callbacks += 1
end

def the_callback_is_handed_the_popped_pages_value()
  ux_popped = nil
  var cfgs = routines.settings_cfgs("ramped_static")
  assert_true(ux.push(SettingsEditScene(cfgs, routines.load_settings("ramped_static"), "Settings"),
                      ux_keep),
              "the settings list would not push")
  harness.beat(1)
  harness.press("ok:single")
  harness.beat(1)

  assert_true(ux_popped != nil, "the callback was handed nothing")
  assert_eq(ux_popped["initial_pressure"], routines.load_settings("ramped_static")["initial_pressure"],
            "the callback was handed another set")
end

test("the callback is handed the popped page's value", the_callback_is_handed_the_popped_pages_value)

def pop_to_root_runs_no_callback()
  ux_callbacks = 0
  assert_true(ux.push(BrowserScene(), ux_count), "the browser would not push")
  harness.beat(1)

  assert_true(ux.pop_to_root(), "pop_to_root was refused")
  harness.beat(1)
  assert_eq(ux_callbacks, 0, "pop_to_root ran the page's callback")
  assert_eq(ux.depth(), 1, "pop_to_root left a page showing")
end

test("pop_to_root runs no callback", pop_to_root_runs_no_callback)
