# memory.test.be — what the interpreter holds once a page has settled: each routine the image
# registers is pushed and driven to where a cycle has reached its own phases, then the VM's heap is
# read after a collection and reported. A figure is a reading, not a case: nothing here fails a run.
#
# The count is the host's, under the device's berry_conf.h. A host pointer is 8 bytes and a bvalue 16
# where the S3's is 8, so the same image bills here roughly twice what it does on the board: it is an
# upper bound on the board, not the board's figure.

suite = "memory"

# The drive's own budget for reaching a cycle's phases and its repeat: a page part-way through its
# first phase is not a steady state.
memory_beats = 1600

def report_pages()
  # The first push in a run pays for what every page then shares — the scene infrastructure, the
  # settings registry, the strings a page names — so one page is driven once before the lines are
  # read, and each line below is that page's own cost.
  if harnessImageRoutines.size() > 0
    harness_activate(harnessImageRoutines[0], nil)
    harness.beat(8)
    ux.pop_to_root()
  end
  for key : harnessImageRoutines
    # Back to the root scene first: the push at the start of a line would otherwise release the page
    # the line before left up, and the two would cancel.
    ux.pop_to_root()
    var before = harness.mem()
    harness_activate(key, nil)
    harness.beat(memory_beats)
    harnessLog = harnessLog + "[I] mem: " + key + " +" + str(harness.mem() - before) + " B\n"
  end
  ux.pop_to_root()
end

test("every image page reports its settled heap", report_pages)
