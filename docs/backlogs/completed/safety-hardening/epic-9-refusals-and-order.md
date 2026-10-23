# Epic 9 — Refusals and order

A refused write says so instead of looking like a lost one.

## What changed

- **`write_result`.** A new characteristic — the field's 16-bit suffix, least significant byte first,
  then a reason byte: `1` not controlling, `2` out of range, `3` not a number. It is notified when a
  `target` or `hold_s` write is refused, and it is the only channel that carries a refusal; the
  read-back stays the authority on what the machine holds.
- **A wrong-length payload is still ignored**, never padded or truncated.

## Verification

Builds pass. Observing the notification for a refused target or hold is a bench check.
