# Epic 2 — The capability record

One machine-side value says which optional assemblies the composition root
fitted and how the device stops itself, and both links serve it.

## What changed

- `Capabilities` joins the `AppContext` graph. It holds three booleans —
  panel, battery, ownership stop — and a one-byte `mask()` with the bits
  `1` panel, `2` battery, `4` ownership stop.
- The full build's composition root records panel and battery as fitted and
  leaves the ownership-stop bit clear: its stop is the local Back hold, not
  an ownership rule.
- The BLE machine domain gained a read-only `caps` characteristic (`0x000E`)
  that answers the mask.
- `device get` prints `caps=` with the same mask and drops `display-size`.
- Both specs updated: the serial `device` table and example now carry `caps`,
  and the BLE characteristic table gains the `caps` row.

## Verification

`Device.cpp` and `MachineBleDomain` both read `Capabilities::mask()`, so the
serial field and the BLE characteristic are the same value by construction.
The build passes throughout; the on-hardware read of both surfaces is the
parking lot's verification entry.

## What the headless build will do with it

A machine-alone composition root records no panel, battery by build, and the
ownership-stop bit set — that is Epic 5. Nothing here changes the full
device's behavior.
