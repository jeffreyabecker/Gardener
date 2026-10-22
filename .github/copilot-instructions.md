# Project Guidelines

## Build

`python scripts/arduino.py --task build` — compiles the sketch with the project
local libraries. There is no host test suite, and nothing checks the layer rule, so a stray
`hal/` include that reaches upward is a review catch rather than a build failure.

**Build when the compiler is the question.** The build compiles the C++ under `libraries/` and
`firmware/` and the build files themselves; nothing else reaches it. A document, a `.bmp` under
`assets/`, or a file in `firmware/gardener/data/` — a `.be` routine, a page, a `.ini` — cannot change
what the build says, so editing one is not a reason to run it. The data image is checked on the host
instead: `python .github/skills/berry/check.py` compiles and drives every `.be` file, which is the
success a routine edit has to prove.

A commit runs nothing: when told to commit, stage the tree and write the message, and don't build,
test or review the diff on the way.

## Skills

Firmware design rules are in `.github/skills/firmware/`, with C++ naming and formatting in
`.github/skills/cpp/`; Git practice and which document work belongs in are in
`.github/skills/project/`, with a skill per document — `.github/skills/specs/`,
`.github/skills/plans/`, `.github/skills/ideation/` and `.github/skills/backlog/`; checking the
Berry data image without flashing is in `.github/skills/berry/`.

## Where to look

- [README.md](../README.md) — build and upload flow, the documentation index and safety notes.
- [docs/specs/vacuum-control.md](../docs/specs/vacuum-control.md) — the machine: states, transitions and protections.
- [docs/specs/serial-protocol.md](../docs/specs/serial-protocol.md) — the interface the firmware serves.
- [docs/guides/poc-parts.md](../docs/guides/poc-parts.md) — the parts a build is assembled from, with prices.
