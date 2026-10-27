# suite.be — the test framework the routine suites register against.
#
# Loaded after the image and driver.be, so a test can name every system file, module, routine and
# the `harness` drive. A suite file sets `suite` to its own name, registers cases with test(), and
# the runner here executes them one at a time, each from a reset page, reporting a line per case.

tests = []
suite = ""

# The heap the cases run against, set when the runner starts: a page's figure is read against this,
# so a memory line is a page's own cost rather than the whole VM's.
harness_suite_base = 0

def test(name, fn)
  tests.push([suite, name, fn])
end

# --- assertions ------------------------------------------------------------

def assert_true(cond, msg)
  if !cond
    raise "assert", msg
  end
end

def assert_eq(actual, expected, msg)
  if actual != expected
    raise "assert", msg + ": expected " + str(expected) + ", got " + str(actual)
  end
end

def assert_nil(x, msg)
  if x != nil
    raise "assert", msg + ": expected nil, got " + str(x)
  end
end

def assert_not_nil(x, msg)
  if x == nil
    raise "assert", msg + ": expected a value, got nil"
  end
end

# --- driving a machine -----------------------------------------------------

# A beat of the harness and an advance of a machine, which is the whole of what a page's tick() does
# for one: nothing in a machine moves on its own, so a case that drives it directly has to hand it
# `now` as well as let the clock run.
def drive(machine, beats)
  var i = 0
  while i < beats
    harness.beat(1)
    machine.advance(vacuum.now_ms())
    i += 1
  end
end

def drive_until(machine, label, beats, pred)
  var i = 0
  while i < beats
    harness.beat(1)
    machine.advance(vacuum.now_ms())
    if pred()
      return true
    end
    i += 1
  end
  raise "assert", label + " (not reached in " + str(beats) + " beats)"
end

# --- the runner ------------------------------------------------------------

# Run every registered case whose full name contains `filter`, each from a reset page. A failing
# case raises and the runner moves on, so one bad case does not hide the rest.
def suite_run(filter)
  var passed = 0
  var failed = 0
  # The band across the bottom of the glass is the ux component's, not a case's: every frame a case
  # writes hashes those rows, so a picture of a case is a picture of the page it drove.
  harness.mark_overlay(screen.height() - overlay_band_height, overlay_band_height)
  # The heap the cases run against: the image, this framework and the suites themselves. A figure a
  # case reports is read against it, so a per-page line is a page's cost and not the whole VM's.
  harness_suite_base = harness.mem()
  harnessLog = harnessLog + "[I] mem: suites " + str(harness_suite_base) + " B\n"
  for t : tests
    var full = t[0] + "." + t[1]
    if filter == nil || filter == "" || string.find(full, filter) >= 0
      harness.reset()
      # One GIF per case, named from the stem the command line hands over, so a failing case still
      # leaves its motion behind.
      harness.capture_start(harness.capture_stem() + "." + full + ".gif")
      try
        t[2]()
        passed += 1
        harnessLog = harnessLog + "[PASS] " + full + "\n"
      except .. as e, m
        failed += 1
        harnessLog = harnessLog + "[FAIL] " + full + ": " + e + ": " + m + "\n"
      end
      if !harness.capture_finish()
        harnessLog = harnessLog + "[E] suite: cannot write " + full + " capture\n"
      end
    end
  end
  harnessLog = harnessLog + "[I] " + str(passed) + " passed, " + str(failed) + " failed\n"
  # How far above the suites' own figure the run reached: what driving the pages cost.
  harnessLog = harnessLog + "[I] mem: peak +" + str(harness.mem_peak() - harness_suite_base) + " B\n"
  return failed == 0
end
