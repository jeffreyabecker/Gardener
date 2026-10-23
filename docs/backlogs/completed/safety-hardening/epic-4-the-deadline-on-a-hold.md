# Epic 4 — The deadline on a hold

A run ends itself, and the controller is told what it was granted and what the ceiling is.

## What changed

- **`CONTROL_MAX_RUN_MS`.** Fifteen minutes in `Machine.h`, the most any start may ask for. A run
  that reaches it releases the chamber — `VENT` then `IDLE`, not a fault — and the run log names the
  deadline with `run: hold deadline reached after <s> s`.
- **A start carries a hold.** `vacuum start [<seconds>]` runs for that many whole seconds; a value
  above the maximum is `err out_of_range field=hold value=… max=900`, and zero, negative or
  fractional is refused against `min=1`. The command carries the hold, and the control task clamps
  it again.
- **The hold travels over Bluetooth.** `hold_s` is written before `start`, told to the machine by
  the `0x01` opcode; `0` asks for the maximum.
- **Both links publish it.** `vacuum get` gains `hold=` (the grant, `0` when nothing is running) and
  `max_run=`; Bluetooth gains `max_run_s` and reads the granted hold back through `hold_s`.

## Verification

Builds pass. The shortened-constant release is a bench check; the harness's runs start with no hold
and are unaffected within its 250 ms beats.
