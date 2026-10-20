# Project Guidelines

## Build

`python scripts/arduino.py --task build` — compiles the sketch with the project
local library. There is no host test suite, and nothing checks the layer rule, so a stray
`hal/` → `app/` include is a review catch rather than a build failure.

## Skills

Firmware conventions and design rules are in `.github/skills/firmware/`; docs and commit
practice are in `.github/skills/project/`.

## Where to look

- [README.md](../README.md) — wiring, build and upload flow, control model, safety notes.
- [docs/specs/serial-protocol.md](../docs/specs/serial-protocol.md) — the interface the firmware serves.
- [docs/guides/poc-parts.md](../docs/guides/poc-parts.md) — the parts a build is assembled from, with prices.
