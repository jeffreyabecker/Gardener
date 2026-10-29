# Project Guidelines

## Build

`python scripts/arduino.py --task build` — compiles the sketch with the project
local libraries. Nothing checks the layer rule, so a stray `hal/` include that reaches upward is a
review catch rather than a build failure.

**The tree's C++ has a host unit test suite.** `python tools/unit/check.py` compiles
`libraries/Gfx` and `libraries/Gardener` with its fakes and its cases under doctest and runs them;
it needs no board, no interpreter and no data image. `--list-test-cases` lists what it holds and
`--test-case="a name"` runs one case, which is what a change to the machine, the wire, the documents
or the drawing is checked with.

**Build when the compiler is the question.** The build compiles the C++ under `libraries/` and
`firmware/` and the build files themselves; nothing else reaches it. A document, a `.bmp` under
`assets/`, or a file in `firmware/data/` — a `.be` routine, a page, an `.ini` — cannot change
what the build says, so editing one is not a reason to run it. The data image is checked on the
device instead: `--task upload`, then the panel and the `[E]` lines on the serial console.

**Consider the build before running it.** Ask what this run would catch that the last one did not: what
changed in the tree since then, and whether that change can reach the compiler. When the answer is
nothing — the same tree, a tree changed only in prose or data, a build before a commit after a build
that already passed — don't run it. One build after a change that can break the compile is the
verification; a second one is only waiting for the answer it already gave.

**A build is minutes, so the spinning is the expensive part.** Every rebuild waits minutes for an
answer, and a fix-one-error-and-rebuild loop is the likeliest way a task spends its whole budget
without moving. Make the change whole before the compiler sees any of it — every file of a refactor,
every caller of a renamed function — then build once. When a build fails, read the whole output and
fix everything it named in that same pass; when a change spans several files, don't verify them one
at a time.

**The build's answer is whether it compiles.** `arduino-cli` ends every run with the program-storage
and global-variables figures, and they move a little with every edit; that is a number to keep out of
the report unless someone asks how much room the firmware has. Don't build a baseline to compare
against, and don't build twice to show a delta. A size comparison nobody requested costs two slow
runs and changes no decision, so report pass or fail and leave the figures out.

**Consider the Berry check before running it.** The check compiles and drives the `.be` files under
`firmware/data/`, the `.ini` documents in `cfg/` beside them, the harness, and the vendored
VM. That is the whole of what reaches it, so a document, an `assets/` image, or C++ outside
`libraries/berry/` cannot change what it says. Ask what a run would catch that the last one did not;
when the answer is nothing — no Berry edit since the last check, or a check that already passed on
this tree — don't run it. One check after a change that can break a routine is the verification.

A commit runs nothing: when told to commit, read `git status --short`, stage the tree and write the
message, and don't build, test or review the diff on the way.

## Skills

Firmware design rules are in `.github/skills/firmware/`, with C++ naming and formatting in
`.github/skills/cpp/`; Git practice and which document work belongs in are in
`.github/skills/project/`, with a skill per document — `.github/skills/specs/`,
`.github/skills/plans/`, `.github/skills/ideation/` and `.github/skills/backlog/`; writing the
Berry data image is in `.github/skills/berry/`, with its host check under `tools/berry/`.

## Where to look

- [README.md](../README.md) — build and upload flow, the documentation index and safety notes.
- [docs/guides/poc-parts.md](../docs/guides/poc-parts.md) — the parts a build is assembled from, with prices.
- [docs/archive/specs-and-plans.zip](../docs/archive/specs-and-plans.zip) — every spec and plan
  written before the panel port. Nothing in it is read when implementing and no work is checked
  against it: what is true is the code and the backlog.
