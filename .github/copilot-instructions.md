# Project Guidelines

## Build

`python scripts/arduino.py --task build` — compiles the sketch with the project
local libraries. There is no host test suite, and nothing checks the layer rule, so a stray
`hal/` include that reaches upward is a review catch rather than a build failure.

## Skills

Firmware conventions and design rules are in `.github/skills/firmware/`; Git practice and which
document work belongs in are in `.github/skills/project/`, with a skill per document —
`.github/skills/specs/`, `.github/skills/plans/`, `.github/skills/ideation/` and
`.github/skills/backlog/`; checking the Berry data image without flashing is in
`.github/skills/berry/`.

## Where to look

- [README.md](../README.md) — build and upload flow, the documentation index and safety notes.
- [docs/specs/vacuum-control.md](../docs/specs/vacuum-control.md) — the machine: states, transitions and protections.
- [docs/specs/serial-protocol.md](../docs/specs/serial-protocol.md) — the interface the firmware serves.
- [docs/guides/poc-parts.md](../docs/guides/poc-parts.md) — the parts a build is assembled from, with prices.
