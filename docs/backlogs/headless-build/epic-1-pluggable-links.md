# Epic 1 — Pluggable links

Both wire links now expose a surface the build registers instead of one they name themselves.

## Serial

`SerialLink`'s constructor no longer hard-wires the five domains. It exposes
`registerDomain()`, and `App::begin()` registers `control`, `device`, `vacuum`,
`buzzer` and `fs` in the same order, so the serial surface is unchanged. The
domain creation moved out of `SerialLink.cpp` into the composition root
(`App.cpp`), which is where a panel build will later add its own domains and a
machine-alone build will omit them.

## Bluetooth

The fixed `Field` enum and the one `describe`/`write`/`publish` switch are gone.
In their place is `BleDomain`, a Bluetooth counterpart of the serial `Domain`: a
registered piece that declares its characteristics (UUID suffix, properties,
max length), answers reads, queues writes and publishes on its own cadence,
while the link owns the transport — creating characteristics, routing the
NimBLE callbacks, tracking subscriptions and sending notifications.

The table split into two domains:

- `MachineBleDomain` — the twelve machine characteristics (`state`, `kpa`,
  `target`, `duty`, `fault`, `batt`, `command`, `fw`, `ble_proto`, `control`,
  `sensor`, `floor`) and the `command` opcode executor.
- `PanelBleDomain` — the `unit` characteristic.

`BleLink` builds its GATT service from whatever domains are registered, routes
reads, writes and subscriptions to the owning domain, and hands the domain a
`notify()` hook for its publish cadence. Every UUID suffix (`0001`–`000D`),
property set, max length, rate (5 Hz `kpa`/`duty`, 10 s `batt`, on-change
elsewhere) and opcode result is preserved.

The full device registers both domains, so its surface is unchanged. The panel
and machine domains are now separate seams a machine-alone build can leave the
panel's off.

## Verification

- Build passes at each step, and the full sketch's size returns to the baseline
  (within a few hundred bytes) once both domains are registered.
- The wire surfaces are byte-for-byte the same by construction; the on-hardware
  pass is Epic 7.
