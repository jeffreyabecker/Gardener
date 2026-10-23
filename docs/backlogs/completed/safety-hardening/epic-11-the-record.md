# Epic 11 — The record

The four documents describe the machine the code now is.

## What changed

- **`serial-protocol.md`.** `vacuum stop` is ungated; `vacuum start [<seconds>]` takes a hold and
  refuses one above the maximum; target `0` (and `-`) is the ambient target, with a refusal above
  `floor` that names it; `vacuum get` reports `hold` and `max_run`; `not_ready` no longer lists
  `no_target`; `caps` carries the fail-safe bit; the six `device set` fields state their bounds and
  that they apply on the control task.
- **`bluetooth.md`.** The table gains `hold_s`, `max_run_s`, `write_result` and `uptime_ms`, and the
  writes on `floor`, `flow`, `volume`, `deadband`, `pump_max` and `valve` are gone; `target` and
  `hold_s` are the writable fields, with their refusals and the reason bytes; command result `0x01`
  is retired; `caps` bit `4`; the fault codes and the notification rates are updated.
- **`vacuum-control.md`.** Target `0` is ambient, a start is valid at `0`, a stop keeps the target,
  the release criterion is the vent floor, runs have a deadline, and `vent_timeout`, `no_progress`
  and `overshoot` join the protections.
- **`ui/bindings.md`.** `vacuum.start()` no longer refuses for a missing target, `target_off()`
  writes the ambient target, the `reading` record's `target`, `min` and `fault` fields say what the
  machine now reports, and the fault codes are listed.

## Verification

Documentation only: checked against the code in the same change, not by a test.
