# Epic 6 — The loop's own faults

Two failure modes that used to be the controller's problem are now the machine's.

## What changed

- **`no_progress`.** A pull driven at or above half the duty cap for ten seconds without a kilopascal
  of headway latches `no_progress` and releases the chamber: a blocked hose or a stuck sensor is the
  pump working against something it cannot move. The checkpoint is reset whenever the loop is not
  driven hard, so a coasting hold never trips it.
- **`overshoot`.** A depth more than 5 kPa past the target on five consecutive readings while pulling
  or holding latches `overshoot` and releases the chamber. Both thresholds live in `Machine.h` with
  the rest of the loop's numbers.

## Verification

Builds pass. The blocked-hose and stepped-target checks are bench checks.
