# Epic 2 — The command surface

A target is a setpoint, a start runs it, a stop releases the chamber, and one function decides which
of the valve and the pump acts.

## What changed

- **`0` is the ambient target.** `setTarget` accepts `0`, stores it and, on a running machine,
  releases the chamber. `vacuum get` reports `target=0.0` rather than `-`, and `vacuum target -`
  survives as a synonym.
- **`clearTarget` is gone.** The op, the method and the bindings' use of it were deleted; clearing a
  target is `SetTarget 0`, so there is one way to state it.
- **A start is valid at any target.** `startRefusal` no longer answers `no_target`, Bluetooth result
  `0x01` is retired, and the routine binding's `false` now means only a latched fault or no reading.
- **One decision.** `setTarget`, `start` and `stop` all end in `VacuumSystem::converge()`: a target
  of `0` releases, a reading shallower than the target opens the valve, and anything else seals the
  chamber and hands it to the PID. A non-zero target written while stopped is stored, not acted on.

## Verification

Builds pass and the host harness's routines still drive the machine through the same calls — the
interval and ramped pages set, vent and hold through `converge()` exactly as before.
