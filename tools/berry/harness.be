# harness.be — the driver's own bookkeeping, loaded after the image.
#
# There are no stubs here any more. The check compiles the firmware's bindings and runs them, so
# `vacuum`, `screen`, `device`, `buttons`, `buzzer`, `jingles`, `routines` and `store` are the
# device's own modules, the record classes are the ones Bindings.cpp declares, and the drive reaches
# the parts through the native `harness` module (HarnessModule.cpp) instead of a model written in
# Berry.
#
# What is left is what a driver needs and the firmware has no use for: the log it builds, the count
# of what went wrong, a guard around a call it does not trust to return, and the event record it
# hands a page it is walking by hand.

harnessLog = ""
harnessErrors = 0

# The registry is a Berry class now, so the routine count the summary prints is read here, after
# the image has loaded.
harnessRoutineCount = routines.names().size()

def harness_fail(text)
  harnessErrors += 1
  harnessLog = harnessLog + "[E] " + text + "\n"
end

def harness_note(text)
  harnessLog = harnessLog + "[I] " + text + "\n"
end

# The label names what was being walked, so a raise reads like the page it happened on rather than
# like a frame in the driver.
def harness_guarded(label, action)
  try
    action()
  except .. as e, m
    harness_fail(label + " " + e + ": " + m)
    return false
  end
  return true
end

def harness_event(btn, press)
  var ev = event()
  ev.btn = btn
  ev.press = press
  return ev
end
