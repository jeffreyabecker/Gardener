# Epic 5 — A release that either completes or latches

A release is judged against the depth this rig actually vents to, and one that misses it says so.

## What changed

- **The vent floor.** `VENT_FLOOR_KPA` in `Machine.h` is the bench-measured depth a chamber settles
  at with the valve open; a release completes within `VENT_MARGIN_KPA` of it, and `IDLE` is reported
  only once it is reached.
- **`vent_timeout` latches.** The timeout no longer falls through to `IDLE`: a release that has not
  reached the floor in 30 s latches `vent_timeout` with the pump cut and the valve open.
- **`sensor` covers releases.** A lost reading faults in any state but `IDLE`, so a release with a
  dead sensor latches rather than timing out.
- **Boot.** A machine that comes up above the vent floor enters `VENT` and lets the floor or the
  timeout finish it, instead of reporting `IDLE` over a chamber that holds pressure.
- **The run log.** The timeout warning is gone; the fault transition carries the code, so a release
  that fails reads `VENT -> FAULT (fault vent_timeout) after N ms`.

## Verification

Builds pass. The blocked-vent and unplugged-sensor checks are bench checks.
