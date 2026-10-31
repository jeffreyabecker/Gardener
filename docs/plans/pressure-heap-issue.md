# The pressure readout draws nothing, sometimes

## What is wrong now, and where

On some runs the interval page's big pressure readout is not drawn: the cell is paper where the
reading and the target should be, for one frame at a time. It is not the drive — the machine state,
the beat count and the frame count are identical across runs — and it is inside
[libraries/Gardener/src/panel/page/PressureReadout.cpp](../../libraries/Gardener/src/panel/page/PressureReadout.cpp),
because replacing that node with a plain `screen.centred(...)` in the ramp chrome draws the same cell
every time.

It is real and it matters: the panel repaints that node several times a second for the life of the
boot, so whatever makes it draw nothing here is a latent read of memory that is no longer the
readout's to read.

## What the experiments so far have ruled out

- **Not the drive.** A trace of every beat showed the same `RunState`, pressure, duty and frame count
  in a good run and a bad one.
- **Not an uninitialised stack variable.** A build with `-ftrivial-auto-var-init=pattern` still
  varies, and the picture does not change.
- **Not the `std::string` buffer.** It is `String` now; the picture is byte-identical and the
  variance remains.
- **Not the check's activation path.** `check.py --art interval --seconds 60` is stable across the
  same perturbations, so this is not simply "a page activated from C++".
- **The trigger is heap layout.** With `PAD1` set to the empty string, every run of
  [tools/walkthrough/sample.walk](../../tools/walkthrough/sample.walk) writes a different GIF; with
  nothing set, ten runs in a row are byte-identical (`BFFDB8F7…`).
- **The formatted text matters**: hard-coding the target string to `"30.0"` made every run identical
  again, with the same picture as the good runs.

## The approach: make it reproducible, then read it

A fault that appears at random cannot be bisected, and a fix cannot be proved. So the first job is a
reproducer we can call on demand, and only then any diagnosis. Each step below says what it decides;
a step that decides nothing is a step to replace.

### 1. A reproducer

Sweep the length of an environment variable (`0, 1, … 64`, then a few larger) and hash the GIF for
each, against the reference picture. If some length always fails, that is the reproducer and nothing
in the tree changes. If no length is reliable, add a `--pad <bytes>` switch to the walkthrough tool
for the length of this branch: it holds one allocation of that size across the run, which shifts the
heap the same way and does it every time.

*Decides:* whether the rest of the work is cheap. *Fails if:* no perturbation is stable, in which
case the fault is a race or a timer and this plan is the wrong one.

### 2. Which layer: the draw or the capture

The capture packs each frame from `sent_`, a copy of only the rectangle that was written. A blank
cell is either a draw that made no ink or a frame that missed ink that was there.

Add a branch-only probe that prints `HarnessSession::ink()` over the readout's box after every beat,
and run the reproducer. Ink present but the frame blank means the capture's dirty-rect path; no ink
means the draw.

*Decides:* whether to read `CaptureDisplay::flush` or `layout`/`shapes` next. The probe must leave
the reproducer failing — check that first, or it has told us nothing.

### 3. See the values at the failing draw

`gdb` 17.1 is installed (`C:\tools\msys64\ucrt64\bin\gdb.exe`), so the readout can be inspected
without touching the code at all: break where the readout's closure runs and print `face`, `scale`,
`secondaryScale`, `sizeHint`, `state.running`, `state.unit`, `state.currentPressureKpa`,
`reading.c_str()`, `target.c_str()` and the computed `ink` — once under a good perturbation and once
under the failing one, driven by `-ex` scripts so the environment stays fixed.

*Decides:* a garbage `face` or a zero `scale` would explain a blank cell outright; sane values move
the suspicion to the borrowed text or the node itself.

*Fallback:* if gdb cannot see through the closure at `-O0` (it should), a Linux build of the same
sources under WSL Ubuntu 20.04 with `valgrind --track-origins=yes` names an uninitialised or invalid
read directly. That is a second toolchain and would be reported before it was taken on, not adopted
quietly.

### 4. Bisect the readout, one hypothesis per build

Each is a small patch on this branch, kept only if it makes the reproducer draw the reference picture
*and* leaves every run identical. In the order I would try them, cheapest first:

1. **The borrowed text outlives its `String`.** `centredReading` hands `value.c_str()` to
   `gfx::borrowedText`; the node is drawn in the same statement, so this should be safe — but it is
   the one place in the tree where a paint borrows a string built inside a closure. Test with a
   literal, then with an owned copy.
2. **The record's fields.** Use the parameters' defaults instead of reading `scale`,
   `secondaryScale` and `font` off the node; a garbage `face` measures zero and draws nothing.
3. **The `callable` itself.** Format the two strings in `bindings/DrawTree.cpp` while the tree is
   built and return a plain `aligned(sized(borrowedText(...)))`. The tree is rebuilt for every draw
   anyway, so the closure buys nothing but indirection — and it is the node that fails.
4. **The `SystemState` copy in the closure.** Capture a reference to the live state instead of a copy
   of a struct of volatiles.
5. **`String`'s heap buffer.** Draw from a fixed `char[16]` instead of a `String`.

### 5. Two suspects worth reading while we are in the file

- `be_getmember` **always pushes a value**, nil when the member is absent
  ([libraries/berry/src/be_api.c](../../libraries/berry/src/be_api.c), `ins_member` does `be_incrtop`
  and `var_setnil` before it looks anything up). Every helper in
  [bindings/DrawTree.cpp](../../libraries/Gardener/src/bindings/DrawTree.cpp) that returns early on
  `!be_getmember` — `intField`, `realField`, `flagField`, `textField`, `cellSide`, `numberField` —
  therefore leaks one VM stack slot per absent member, and the VM value stack then grows at
  addresses the heap chooses. Every field the draw path reads is declared on its record, so the
  normal path pops; one absent field is one leaked slot per draw. Audit the other `be_getmember`
  callers in `bindings/` for the same shape.
- Whether the VM value stack can be reallocated in the middle of a draw while a `bvalue*` is held.
- `CaptureDisplay::flush` and its dirty rectangle, if step 2 points there.

## The alternatives, and why not

- **Reassert the picture on every draw** — force the panel to repaint the whole page whenever the
  reading changes. It would hide the fault, not remove it, and the readout is already drawn every
  beat.
- **Make the node draw only from strings it owns** without finding the cause. It may well be the fix
  (hypothesis 1), but adopted as a guess it would leave the same class of bug in the six fields
  `DrawTree` reads, unexamined.
- **Accept the variance as a host artefact.** The node runs on the device at the panel's repaint
  cadence; a read of memory that is not its own is not a host artefact, and the walkthrough's own
  promise — the same file twice, the same GIF — is not met while it happens.

## The fix, and how it is known to have worked

The fix is whichever test wins, kept as small as it can be: an owned copy of the text in the node, the
parameter defaults, or the leaked stack slot popped. The likely shape is the first — a paint that
borrows a string built inside the closure is the one thing here that a heap layout could break.

It lands with a regression that fails without it: a case under
[tools/unit/cases](../../tools/unit/cases) that draws the readout, perturbs the heap between the build
and the draw (or draws it twice with a block allocated and freed in between) and asserts the same ink
both times.

Verified by:

- the reproducer drawing `BFFDB8F7…` on repeated runs, not just the unperturbed one;
- `python tools/unit/check.py` and `python tools/berry/check.py`;
- the sample walk still writing 100 frames over 25.0 s of device time.

## What stays out

The branch-only probes (`--pad`, the ink print) are deleted before the fix is committed. No device
build is needed unless the fix reaches something the host tools cannot compile, which it should not:
this is a host-visible fault on device code, and the suite compiles the code it is in.

## The outcome

The cause was not in the readout at all. A draw record's `x` was an integer whose value was a
pointer's low bits, so the node was drawn at a corner that depended on where the heap happened to put
the record: sometimes off the glass, sometimes far enough left that the cell clipped the box back to
itself and the picture looked right. That is why every perturbation moved it and why the "reference"
GIF turned out to be the broken drawing, with home's own readout blank.

The argument readers in [bindings/Screen.cpp](../../libraries/Gardener/src/bindings/Screen.cpp) were
bounded by `be_top(vm)`, but the node a call makes is pushed on top of its arguments before they run,
so the count included the record. An index one past the last argument — which is what
`screen.pressure_readout()`, taking no box at all, asks for — passed the check and read the record
itself: `argInt` is `be_toint`, which reinterprets a value's payload without looking at its type, so a
pointer came back as a whole number. `y`, `w` and `h` were past that slot and stayed nil, which is why
one field moved and not four.

The fix is one count, taken before the node is made, and `hasArg` in its place everywhere an argument
is read: the call's own argument count is what says where the arguments end. Auditing the rest of
`bindings/` found the same shape in three more places that happened to be harmless — a row's
`alignment` was being set to its children, a scrollbar's `hollow` to its record, and `screen.text`
raised on a font passed without a scale.

The six field readers in [bindings/DrawTree.cpp](../../libraries/Gardener/src/bindings/DrawTree.cpp)
are the other half: `be_getmember` always leaves a value on the stack, so each now pops the nil it
asked for instead of leaking the slot when a field is absent.

Verified by the reproducer: all sixty-five lengths of the environment variable, and every in-program
heap pad the earlier sweep found, now write one GIF — `D1C3583E…`, with the readout drawn on home and
on the interval page. The case that fails without the fix is `harness_boxes()` in
[tools/berry/driver.be](../../tools/berry/driver.be), which reads the box off a node the way a page
does: with the fault it answers `1733273392,nil,nil,nil`. The unit suite and the Berry check both pass,
and the sample walk still writes 100 frames over 25.0 s of device time.
