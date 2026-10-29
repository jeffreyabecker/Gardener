# Berry marshaling cleanup

## What this is

Two heaps meet at the interpreter, and nothing measures whether what goes into one comes back out.
The VM's heap holds what the marshaling builds inside it — the nodes a page draws, the records it
reads, the lists that root what a C++ object holds — and three C++ objects cross into it as `comobj`:
the row a declaration owns, the row a record borrows, and the scene a factory answers. The process
heap holds those three, and the `String`, `std::vector` and `SettingValues` a call builds around
them. A large part of the marshaling's design is that these are released: each `comobj` carries a
destroy function, `__uxCallbacks` reuses the slots its functions leave, `~BerryScene` empties the
slot it took in `__pages`, and the message a raise carries is a stack buffer or a literal because
`be_raise` skips destructors. None of that is observed. The one figure anyone has is a single
reading per page, taken once, on the host, in a check that reports rather than fails.

This plan requires the contract to be observed rather than trusted: a page that is opened and closed
holds nothing, every native object handed to a page is released when the VM goes, a refused call
leaves both heaps as it found them, and the device can say what the interpreter holds. It leaves the
shape of the checks to whoever implements them, and it does not ask the device to give up its VM.

## A page that is opened and closed holds nothing

`tools/berry/tests/memory.test.be` drives each registered page once and reports `harness.mem()` less
the figure before the push. One reading cannot separate a settled page from one that keeps a little
of every launch: the second looks like the page's own cost, and the case's header says outright that
nothing in it fails a run.

One path already keeps a little of every launch. `BerryScene`'s constructor appends its page to
`__pages` and takes `size - 1` as its slot, and the destructor sets that slot to nil, but no slot is
ever reused or dropped, so the list is one entry longer for every page ever pushed and grows for the
life of the VM. The entry is a `bvalue` — 8 bytes on the S3, 16 on the host, the same figures the
memory case's header states — so a page opened a thousand times leaves about 8 KB in a global that
nothing clears. The page itself is not leaked; the table that rooted it is. The other two lists do
not have this shape: `__routineClasses` stops growing when the image finishes loading and
`__uxCallbacks` reuses a slot a callback left.

What is true instead: after a page has been driven once to a settle, ten cycles of push, four beats
and `pop_to_root` and a second ten must leave the interpreter exactly as they found it. The reading
is the VM's own count, `be_gc_memcount`, taken after `be_gc_collect` with the stack back at the root
page; the reading after the second batch equals the reading after the first, and the slope between
them is reported rather than only the totals. The first batch's figure less the figure before the
warm-up stays what the memory case reports today — the page's settled cost — so the reading it
already gives is kept and the growth it could not see is added.

Checked: for every page the image registers, the second batch's reading equals the first's and the
reported slope is zero; on the host by the image's check, on the device by the log line the next
requirement asks for, with ten open-and-closes of one page leaving the line at the figure it had
after the first.

## Every native object handed to a page is released by the VM

`be_vm_delete` is called nowhere. `BerryInterpreter` holds its VM in a raw pointer and no destructor
gives it back, and the panel's composition never tears its parts down, so the three destroy functions
are never shown to run at all: a `comobj` that the collector never reaches and one that is released
look identical from a heap reading, and the two owners and the scene they carry are process heap,
which the VM's count does not see.

What is true instead: with the scene stack empty, deleting the VM releases every object the
marshaling handed it, and afterwards the process holds no allocation made by the three owners or by
the pages and rows they carried. Emptying the stack first is part of the requirement, not a
convenience: a `BerryScene` destructor reads the VM, so a stack still standing when the VM goes is a
use-after-free rather than a leak. The device keeps its VM for the life of the boot and is not asked
to delete it; the teardown exists to be observed, on the host.

Checked: after the teardown the host reports zero live bytes from the marshaling — LeakSanitizer on a
build that ends by tearing the VM down, or a live-object counter in the allocation path, which is
also the instrument the next requirement needs. Every release ran exactly once, since a second
release is a double free rather than a reading.

## A refused call leaves both heaps as it found it

`be_raise` longjmps to the protected call, so a raise skips every C++ destructor between it and the
`be_pcall` that catches it. The modules answer this with a rule — messages in stack buffers, every
argument check finished before the first `String` is built — and the rule is enforced by review
alone. One site breaks it today: `routines.save_settings` holds `const String routineKey` and
`gfx::ui::SettingValues values` when it raises for a value a row refuses, so the string and every
entry the loop has already put in the map are dropped. It is the one site a read of the boundary
finds, not the only one there can be, and nothing has ever looked.

What is true instead: no native call raises while a `String`, `std::any`, `std::vector` or
`std::unique_ptr` of its own is alive. Where a call needs one, every check that can raise has
finished before it is built, or the raise happens after it is gone — which is the rule the modules
already state, holding on all six of them and on `routines.save_settings` in particular.

Checked: a loop calling each refusing path twenty-five times, with a full collection after, leaves
the VM's count where it started; the same loop under the live-byte counter leaves the process's count
where it started. The second is the one that sees this: a `String` and a `SettingValues` are the
process heap, so a leaked message or map is invisible to `be_gc_memcount`.

## The device reports what the interpreter holds

`BE_USE_GC_MODULE` is 0 in the device's configuration, so no image can read its own count, and no
line the firmware writes carries it. The only figure that exists is the host's, which the memory
case's header says bills a pointer at 8 bytes and a `bvalue` at 16 against the S3's 4 and 8: an upper
bound on the board, not the board's figure. So the first requirement's check cannot be taken on the
part it protects.

What is true instead: the interpreter's held bytes, read after a full collection, are written where
the interpreter already reports its own failures — a log line once the image has loaded and before
any page is pushed, and again once a page has opened and closed. It is a diagnostic line and not a
field: the serial protocol keeps the shape it has, and free heap stays parked.

Checked: on the device, the line is present before any page is pushed, and the line after one page
has been opened and closed ten times carries the same figure as the line after the first.

## Consequences

The memory case stops being a report: a slope becomes a failure, and its header's line about nothing
in it failing a run no longer describes it.

All three host checks land in that same check, which is in a known broken state and is not run. Until
it runs again none of them can be taken, and the plan's work is unverified beyond a compile; the
parking lot holds the same blockage against a spec for that tool.

The serial line gains a diagnostic record and the protocol gains nothing. The device's `[I]` records
are read by hand today, and this is one more of them.

## Deferred

- The out-of-memory path. `be_newcomobj` returns without storing or freeing the object it was handed
  when its own allocation fails, so an owner is dropped at the moment memory is already short. It is
  parked, since proving it needs an allocator that fails a chosen allocation.
- The board's own free heap — `ESP.getFreeHeap` and the largest free block — stays in the parking lot
  where it is, blocked on a protocol field nobody has decided on. The count this plan reports is the
  interpreter's, not the device's.

## How it is known to hold

1. On the device, `python scripts/arduino.py --task upload`: the line written once the image has
   loaded reports the count, and ten open-and-closes of one page leave the line at the figure it had
   after the first.
2. In the image's host check, once it runs again: per registered page, a settled cost, a flat repeat
   with a reported slope, a refusal loop that moves neither count, and zero live bytes after the VM
   is torn down.
3. `python scripts/arduino.py --task build` compiles the teardown, the log line and any change to the
   modules the first requirement needs.
4. `python tools/unit/check.py` holds nothing here: it builds `libraries/Gfx` and
   `libraries/Gardener`, and the marshaling is neither.
