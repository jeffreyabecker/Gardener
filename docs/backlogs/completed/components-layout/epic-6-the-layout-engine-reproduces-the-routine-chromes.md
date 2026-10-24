# Epic 6 — the layout engine reproduces the routine chromes

A suite in `tools/berry/tests/chromes.test.be` renders each routine chrome as it draws today and a
v2 reconstruction side by side, saving both frames into `tools/berry/artifacts/v2/`. Pixel-perfect
reproduction is a non-goal, and the chrome classes are not updated.

- 6.1 — the readout: `__rootScene.chrome` beside a v2 row of two growing `PressureDisplayV2`s.
- 6.2 — the ramped static chrome: `RampedStatic().chrome` beside a v2 pressure with a
  status-plus-growing-bar row.
- 6.3 — the ramped interval chrome: `RampedInterval().chrome` beside a v2 pressure, a
  `ProgressBubblesV2` field and the status-plus-growing-bar row.

Verified: `python tools/berry/check.py --tests` passes, and the six comparison frames
(`readout-v1/v2`, `static-v1/v2`, `interval-v1/v2`) are written.
