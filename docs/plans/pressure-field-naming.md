# Pressure field naming

## Requirement

A name that holds a pressure value must say so. `kpa` names a unit, not a value, so it is
gone as a field, variable, parameter, record field or wire field. The vacuum depth below
ambient — the one quantity `kpa` meant — is `pressureKpa` in C++ and `pressure_kpa` in
Berry and on the wire. The compound accessors that already named the thing keep the thing
and gain the word as well.

## The naming contract

C++ (camelCase):

| was | becomes |
| --- | --- |
| `PressureSensor::kpa()` and overrides | `pressureKpa()` |
| `Chamber::kpa()` / `kpa_` | `pressureKpa()` / `pressureKpa_` |
| `VacuumSystem::currentKpa()` | `currentPressureKpa()` |
| `VacuumSystem::targetKpa()` / `targetKpa_` | `targetPressureKpa()` / `targetPressureKpa_` |
| `MachineSettings::floorKpa()` / `floorKpa_` | `floorPressureKpa()` / `floorPressureKpa_` |
| `MachineSettings::deadbandKpa()` / `deadbandKpa_` | `deadbandPressureKpa()` / `deadbandPressureKpa_` |
| `setTarget(float kpa)`, `setFloorKpa(float kpa)`, `setDeadbandKpa(float kpa)` | `setTarget(float pressureKpa)`, `setFloorPressureKpa(float pressureKpa)`, `setDeadbandPressureKpa(float pressureKpa)` |
| `PanelSettings::setTarget(float kpa)`, `targetKpa()` / `targetKpa_` | `setTarget(float pressureKpa)`, `targetPressureKpa()` / `targetPressureKpa_` |
| `displayValue(float kpa, DisplayUnit)` | `displayValue(float pressureKpa, DisplayUnit)` |
| `float kpa` locals in the BLE, serial and routine-binding domains | `float pressureKpa` |
| BLE characteristic enum `Kpa` | `PressureKpa` |

Berry and the wire (snake_case):

| was | becomes |
| --- | --- |
| record field `s.kpa` (`kRecords`, `setReal(…, "kpa", …)`) | `s.pressure_kpa` |
| `var kpa` locals, `draw(kpa)` | `pressure_kpa` |
| serial field `kpa=` | `pressure_kpa=` |
| BLE characteristic `kpa` (docs and comments) | `pressure_kpa` |

## What does not change

- The unit's own name: `DisplayUnit::Kpa`, the `"kpa"` unit token, `unit=kpa` and prose
  `kPa`. These name the unit, not a value.
- Rates and ratios: `perKpa` / `per_kpa`, `kVoltsPerKpa`, `slewKpaPerSec`,
  `kPullKpaPerSecond` and the other `…KpaPerSecond` constants.
- The threshold constants `CONTROL_MIN_TARGET_KPA`, `CONTROL_FLOOR_KPA`,
  `CONTROL_DEADBAND_KPA`, `VENT_MARGIN_KPA`, `VENT_TARGET_LEAD_KPA`: they already name
  the thing the value is a threshold of, and this plan is about value holders.
- The sibling wire and record fields `target`, `floor`, `duty`, `deadband` already name
  the thing without a unit; they stay as they are.

## Interface

`kpa` is the documented wire name in two places, so this is a breaking change to the
interface the firmware serves: the serial reply field `kpa=` becomes `pressure_kpa=`, and
the BLE characteristic called `kpa` becomes `pressure_kpa`. The specs that describe them —
`serial-protocol.md`, `bluetooth.md` and `bindings.md` — change in the same commit, and the
in-tree Berry routines and the host harness compile against the new names.

## Acceptance

- No identifier, record field, wire field or characteristic named `kpa` remains; the only
  lowercase `kpa` left is the unit's own name (`"kpa"` token, `unit=kpa`).
- The build passes and the Berry host still compiles and drives the image.
