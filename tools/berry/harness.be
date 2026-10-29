# harness.be — the driver's own bookkeeping, loaded after the image.
#
# There are no stubs here any more. The check compiles the firmware's bindings and runs them, so
# `vacuum`, `screen`, `device` and `routines` are the device's own modules, the record
# classes are the ones Bindings.cpp declares, and the drive reaches the parts through the native
# `harness` module (HarnessModule.cpp) instead of a model written in Berry.
#
# What is left is what a driver needs and the firmware has no use for: the log it builds, the count
# of what went wrong, and a guard around a call it does not trust to return.

harnessLog = ""
harnessErrors = 0

# The keys a run has driven: the routine's page was pushed at least once, which is what the last line
# counts against the keys the image registered. A key that answers no page is a registration the
# image got wrong, and it shows as a count that does not add up rather than as a gap in a log.
harnessDriven = []
harnessDrivenCount = 0

def harness_drove(routine_key)
  for listed : harnessDriven
    if listed == routine_key
      return
    end
  end
  harnessDriven.push(routine_key)
  harnessDrivenCount = harnessDriven.size()
end

# The registry is a Berry class now, so the routine count the summary prints is read here, after
# the image has loaded. The keys are kept as the image left them: a suite may register a routine of
# its own, and the memory report is about the image's pages, not whatever a case added.
harnessImageRoutines = routines.keys()
harnessRoutineCount = harnessImageRoutines.size()

def harness_fail(text)
  harnessErrors += 1
  harnessLog = harnessLog + "[E] " + text + "\n"
end

def harness_note(text)
  harnessLog = harnessLog + "[I] " + text + "\n"
end
