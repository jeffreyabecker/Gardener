# Gardener

An ESP32-S3 vacuum pump controller. It pulls a chamber to a target depth below ambient, holds it
for a timer, then vents it — driven over a serial line or Bluetooth by a controller at the other
end.

The controller decides everything. What the device owns is what has to happen when nobody is
watching: pulling to the target, and venting the chamber through one path on stop, abort, link
loss, fault or the panel's reserved key hold. Pressures are kPa, on the wire and in the firmware.

The firmware ships as two builds from one tree. The **full device** is the machine, the OLED panel,
the keys, the buzzer and the battery gauge: the panel is itself a link, so the device drives itself
when nothing else does. The **headless** build is the machine alone — pump, sensor and vent valve
with the serial and Bluetooth links — for a chamber with no panel on it. Each is a sketch that
composes the libraries for its own build, so what a build has is what its sketch names.

## Build

Requires `arduino-cli` with the ESP32 core, and Python 3 for the wrapper.

```powershell
arduino-cli core install esp32:esp32
python scripts/arduino.py --task libs
python scripts/arduino.py --task build
python scripts/arduino.py --task upload --port COM5
python scripts/arduino.py --task monitor --port COM5
```

`--sketch` picks the build: `gardener` is the full device and the default, `headless` is the machine
alone. `--task data` writes [firmware/gardener/data/](./firmware/gardener/data) to the device's
filesystem, and the headless build has none to write. VS Code has the same commands as tasks.

```powershell
python scripts/arduino.py --task build --sketch headless
python scripts/arduino.py --task upload --sketch headless --port COM5
```

The board options are pinned in
[firmware/gardener/sketch.yaml](./firmware/gardener/sketch.yaml) — including `CDCOnBoot=cdc`,
without which `Serial` is UART0 and a USB cable sees nothing. The build uses
`--libraries libraries`, so [libraries/VacuumCore](./libraries/VacuumCore) (the machine) and
[libraries/PanelLink](./libraries/PanelLink) (the panel, which depends on it) are the
firmware rather than sketchbook copies.

## Documentation

- [docs/README.md](./docs/README.md) — the index: specs, guides, plans, ideas and backlogs
- [docs/guides/poc-parts.md](./docs/guides/poc-parts.md) — the parts, the wiring and a build
- [docs/specs/serial-protocol.md](./docs/specs/serial-protocol.md) — the line the device serves
- [docs/specs/vacuum-control.md](./docs/specs/vacuum-control.md) — the machine and its protections

## Safety

Vacuum work is not benign: imploding chambers, collapsing tubing and pumped liquids or vapours can
hurt people and destroy hardware. Keep a rated catch pot and filter in line, and remember that the
firmware's limits are software limits only.

The one hardware guarantee is the valve. It is normally open, so losing power releases the chamber
whether or not the firmware noticed; a normally closed part trades that fail-safe for holding the
vacuum through a power loss.
