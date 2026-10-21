# Project Guidelines

## Build

`python scripts/arduino.py --task build` — compiles the sketch with the project
local library. There is no host test suite, and nothing checks the layer rule, so a stray
`hal/` → `app/` include is a review catch rather than a build failure.

## Skills

Firmware conventions and design rules are in `.github/skills/firmware/`; docs and commit
practice are in `.github/skills/project/`; idea documents and how they differ from plans are in
`.github/skills/ideation/`; checking the Berry data image without flashing is in
`.github/skills/berry/`.

## Where to look

- [README.md](../README.md) — build and upload flow, the documentation index and safety notes.
- [docs/specs/vacuum-control.md](../docs/specs/vacuum-control.md) — the machine: states, transitions and protections.
- [docs/specs/serial-protocol.md](../docs/specs/serial-protocol.md) — the interface the firmware serves.
- [docs/guides/poc-parts.md](../docs/guides/poc-parts.md) — the parts a build is assembled from, with prices.
