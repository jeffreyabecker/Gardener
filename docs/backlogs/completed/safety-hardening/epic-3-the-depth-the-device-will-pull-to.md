# Epic 3 — The depth the device will pull to

The floor is a bound the machine enforces, not a suggestion it publishes.

## What changed

- **A deeper target is refused on every path.** `VacuumSystem::setTarget` drops anything above
  `settings().floorPressureKpa()`, the serial verb answers `err out_of_range field=target …
  max=<floor>`, and the Bluetooth write notifies the refusal. The target is unchanged either way.
- **The stored floor is bounded.** `MachineSettings::load` falls back to `CONTROL_FLOOR_KPA` for a
  stored value outside 5–100 kPa, `setFloorPressureKpa` refuses one, and `device set floor` names
  the bound it missed. The six machine fields share one implementation (`serial/MachineFields.cpp`)
  between the panel build and the machine-alone build.
- **The floor is a commissioning value.** Its characteristic lost its write, so a phone can read the
  bound but not move it; `device set floor` on the serial line is the only way to change it.

## Verification

Builds pass. The stored-value, refusal and read-back checks are bench checks against a device.
