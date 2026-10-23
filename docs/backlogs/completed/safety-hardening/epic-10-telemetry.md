# Epic 10 — Telemetry

A client can place what it is told in time.

## What changed

- **`uptime_ms`.** A `uint32` characteristic, read and notified at the 5 Hz fast rate alongside
  `pressure_kpa` and `duty`. It advances across notifications, so a client can order state changes
  and a command result, and can tell a reconnect from a restart.

## Verification

Builds pass. Reading it and watching it advance is a bench check.
