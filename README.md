# Gardener

An ESP32-S3 vacuum pump controller. It pulls a chamber to a target depth below ambient, holds it
for a timer, then vents it — driven over a serial line, Bluetooth, or the panel on the device.

There is no ownership handshake: the serial line, Bluetooth and the panel can all command the
machine, and the last write wins. What the device owns is what has to happen when nobody is
watching: pulling to the target, and venting the chamber through one path on stop, abort, fault or
the panel's reserved key hold. Pressures are kPa, on the wire and in the firmware.

The firmware ships as two builds from one tree. The **full device** is the machine, the OLED panel,
the keys, the buzzer and the battery gauge. The **headless** build is the machine alone — pump,
sensor and vent valve with the serial and Bluetooth links — for a chamber with no panel on it. Each
is a sketch that composes the libraries for its own build, so what a build has is what its sketch
names.

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
alone. `--task data` writes [firmware/data/](./firmware/data) to the device's
filesystem, and the headless build has none to write. VS Code has the same commands as tasks.

```powershell
python scripts/arduino.py --task build --sketch headless
python scripts/arduino.py --task upload --sketch headless --port COM5
```

Bluetooth is a build option rather than a setting. The full device compiles the radio in by default;
`--no-enable-bluetooth` leaves the NimBLE stack, the GATT table and the `bluetooth` transport out of
that image. The headless build always keeps the radio, because a machine with no panel has only the
two links to drive it.

```powershell
python scripts/arduino.py --task build --no-enable-bluetooth
```

The board options are pinned in
[firmware/gardener/sketch.yaml](./firmware/gardener/sketch.yaml) — including `CDCOnBoot=cdc`,
without which `Serial` is UART0 and a USB cable sees nothing. The build uses
`--libraries libraries`, so [libraries/Gardener](./libraries/Gardener) — the machine and the panel
— is the firmware rather than a sketchbook copy. Beside it a build finds
[libraries/Runtime](./libraries/Runtime), the board it runs on — the part drivers, the file store,
the Bluetooth transport, the two composition roots and the tasks that drive them;
[libraries/BerryHost](./libraries/BerryHost), the interpreter's host side — the install, the native
modules a page is written against and the page scene; [libraries/Gfx](./libraries/Gfx), the canvas
those pages draw on; and [libraries/berry](./libraries/berry), the vendored VM itself.

A build keeps its object files in `firmware/<sketch>/build/compile`, so the next one compiles only
what changed since, header dependents included. Changing a build option starts over on its own:
`arduino-cli` treats the panel and radio switches, the rig's numbers and the FQBN as part of the
build and throws the objects away when one of them differs. `--task clean` removes them, and the
data image beside them, by hand.

## Documentation

- [docs/README.md](./docs/README.md) — the index: specs, guides and backlogs
- [docs/guides/poc-parts.md](./docs/guides/poc-parts.md) — the parts, the wiring and a build
- [docs/archive/specs-and-plans.zip](./docs/archive/specs-and-plans.zip) — every spec and plan as it
  stood before the panel port, kept for reference and read by nothing

## Safety

Vacuum work is not benign: imploding chambers, collapsing tubing and pumped liquids or vapours can
hurt people and destroy hardware. Keep a rated catch pot and filter in line, and remember that the
firmware's limits are software limits only.

The one hardware guarantee is the valve. It is normally open, so losing power releases the chamber
whether or not the firmware noticed; a normally closed part trades that fail-safe for holding the
vacuum through a power loss.
