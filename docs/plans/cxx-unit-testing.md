# Unit testing the core library

## What this is

The one host command for the tree's own C++, `python tools/unit/check.py`, is in place: it builds
`libraries/Gfx` and `libraries/Gardener` against the vendored Arduino API, runs the `TEST_CASE`s
under `tools/unit/cases` on doctest, and shares the frame writer under `tools/host/capture` with
the interpreter's tool. What is left of the plan is its own acceptance: the command does not pass.

## The suite exits 0

Today the command reports 203 cases, 199 passing and 4 failing. The same four were red before the
Berry and panel-tick work, so the plan is not met until the command exits 0 with the framework's
summary covering both libraries.

The four cases:

- `control_law.cpp` "the deadline ends the run as a release and reports itself once":
  `takeDeadlineExpired()` answers false after 2100 ms.
- `control_law.cpp` "a target set shallower than the reading vents to it and carries on": the run
  stays in `Venting` instead of returning to `Pulling`.
- `control_law.cpp` "readings past the target fault the run": `faultCode()` is empty rather than
  `overshoot`.
- `link.cpp` "a CRLF line leaves no carriage return in the body": the reply and the body keep the
  `\r`.

Each is either a case whose expectation must be corrected to what the device does, or a device
behaviour that must change; the check is the same either way.

Checked: `python tools/unit/check.py` exits 0 and reports no failure, and `--test-case=` on each of
the four names runs that case alone.

## Deferred

- The concrete parts — `PwmPump`, `XgzpSensor`, `SolenoidValve`, `PwmBuzzer`, `AdcBattery`,
  `Sh1106Device` and `Buttons` under `libraries/Runtime` — mean nothing off a board.
- The ESP32 core's own `WString`, the class the device actually runs, in place of the API's
  `String`, if the two are ever seen to disagree.
- `Machine::boot`'s device probes beyond the capability byte, `SafeReboot` and the two sketches.
- `BleLink`, the two BLE domains and NimBLE.
- `PanelTasks.cpp`, which names the RTOS, and the bindings, which name the interpreter.
- Reviving `tools/berry/check.py`, and the spec for what it loads and writes.

## How it is known to hold

`python tools/unit/check.py` exits 0, prints the framework's summary for `libraries/Gfx` and
`libraries/Gardener`, and `--test-case=` on each of the four names above reports that case alone and
passing.
