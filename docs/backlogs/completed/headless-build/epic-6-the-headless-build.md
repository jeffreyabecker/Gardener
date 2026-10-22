# Epic 6 — The headless build

Each sketch is its own composition root. The links a build serves, the domains
it registers and the tasks it starts are named in the sketch, so the two builds
differ where a reader can see them rather than behind a library flag. Phase A
moved that setup out of the libraries and into the full device's sketch; Phase B
added the second sketch and taught the build script about it.

## What changed

- **The links are the sketch's.** `gardener.ino` constructs the `SerialLink`
  and the `BleLink`, registers every domain on them — the machine's `control`,
  `vacuum` and `device` and the panel's `device`, `buzzer`, `fs` and BLE fields —
  and starts both. `machineBoot` no longer builds or starts a link, and the
  `MachineBoot` struct that carried them back to the panel's boot is gone.
- **The tasks are the sketch's.** `gardener.ino` creates the two screens and
  starts the control, interface and panel tasks. `machineBoot` brings the
  machine's parts up, `panelBoot` brings the panel up, and neither starts
  anything.
- **`App` is gone.** Its whole job was sequencing the two boots, which the
  sketch now does. `App.h` and `App.cpp` became `PanelBoot.h` and
  `PanelBoot.cpp`, which hold `panelBoot` and nothing else. `AppContext` stays:
  `buildAppContext()` still returns two graphs.
- **The capability belongs to the build.** `buildMachineRoot()` no longer sets
  any capability bit; each composition states its own record. The full device
  sets panel, battery and ownership stop; the machine-alone build sets no panel,
  no battery and ownership stop.
- **The machine has a `device` domain.** `MachineDevice.cpp` answers `proto`,
  `fw`, `chip`, `heap` and `caps` and serves `reboot`, with no battery, log or
  unit field. The two builds register one `device` each — a link holds one
  domain per name — and the panel's carries the machine's fields beside its own.
- **The second build exists.** `firmware/headless/headless.ino` includes only
  `VacuumCore.h`, builds its machine from `buildMachineRoot()`, registers the
  machine's domains, and starts two tasks. With no settings file, it states the
  radio and diagnostics knobs itself.
- **`arduino.py` serves both.** `--sketch` picks the build, defaulting to the
  full device; the data and build folders follow it; upload, monitor and clean
  name the build they act on; and `--task data` refuses the machine-alone build,
  which ships no image.

## One correction to the plan

The items were written against `App::begin`, and item 6.5 deleted `App`. Both
phases are recorded here as they landed: 6.1–6.4 moved the setup into
`App::begin`, and 6.5 moved `App::begin` itself into the sketch.

## Verification

Both sketches build after every item. A clean build of the machine-alone sketch
compiles and links at 568897 bytes (43%) with no reference to `GardenerPanel`,
`berry`, the Adafruit drivers, `RLEBitmap` or `LittleFS`; the full device is
873089 bytes (66%) with 30612 bytes of static RAM. `--task data --sketch
`headless` exits with the refusal. The on-hardware pass is the parking lot's
verification entry.
