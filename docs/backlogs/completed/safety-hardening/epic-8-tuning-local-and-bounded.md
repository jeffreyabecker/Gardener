# Epic 8 — Tuning, local and bounded

The loop's numbers have one definition of their bounds, one task that applies them, and no remote
write.

## What changed

- **One set of bounds.** `Machine.h` holds the four loop inputs' ranges — flow 0.05–10 L/min, volume
  0.005–5 L, deadband 0.05–2 kPa, `pump_max` above zero up to `PUMP_DUTY_MAX_PERCENT` — and the
  loader, the setters and both builds' `device set` refusals read them from there. A stored value
  outside falls back to its default at boot; a written one is refused with the bound it missed.
- **Read-only over the radio.** `flow`, `volume`, `deadband` and `pump_max` lost their writes, so a
  phone can see what the loop is tuned to but not retune it. They are still notified when a serial
  write moves them.
- **One task applies settings.** `device set` validates and answers at once, then posts `SetFlow`,
  `SetVolume`, `SetFloor`, `SetPumpMax`, `SetDeadband` or `SetValve`; the control task applies it on
  its next pass, so the interface task never writes settings the control task is reading. The two
  builds' device domains share one implementation in `serial/MachineFields.cpp`.

## Verification

Builds pass. The refusal, fallback and read-back checks are bench checks.
