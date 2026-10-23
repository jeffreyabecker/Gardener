# Epic 7 — The fail-safe valve

A client can tell which way the fitted valve fails, and cannot change it from the radio.

## What changed

- **`caps` bit `0x04`.** `FailSafeValve` joins `Capability` and is set in `Machine::boot` when the
  fitted part is normally open, so `device get caps=` and the Bluetooth `caps` characteristic say
  whether a de-energised coil vents. A normally closed part clears it.
- **`valve` is read-only over the radio.** The write is gone from the GATT table and from the
  domain; `device set valve` on the serial line still stores the direction for the next boot, and the
  characteristic keeps reporting the part that is fitted now.

## Verification

Builds pass. The bit on a normally-open and a normally-closed build is a bench check.
