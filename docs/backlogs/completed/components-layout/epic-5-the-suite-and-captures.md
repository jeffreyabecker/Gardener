# Epic 5 — the suite and the captures

The host test and capture work, spread across `tools/berry/check.py`, `driver.be`, `main.cpp`,
`tools/berry/tests/baseline.test.be` and `tools/berry/tests/v2.test.be`. Generated frames land in
`tools/berry/artifacts/` — `components/`, `baseline/` and `v2/` — and the folder is gitignored.

- 5.1 — `check.py --component NAME` renders one drawing primitive from either framework to a BMP,
  through a name-to-component table in `driver.be`'s `harness_art_component`, and the harness
  passes the name through to the driver.
- 5.2 — `baseline.test.be` drives the existing v1 components directly — text, fixed text,
  pressure, bar, bubbles at several counts, scrollbar, triangle and icon — asserting each one's
  behaviour and capturing a frame into `artifacts/baseline/`.
- 5.3 — `v2.test.be` now covers the secondary reading in all four corners and the bubble field at
  every count from 1 to 20, and captures v2 frames into `artifacts/v2/`.

Verified: `python tools/berry/check.py --tests` passes all cases, and `--component` writes a BMP
for each named component.
