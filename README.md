# Eggplan Vacuum

An ESP32-C3 vacuum pump controller with a local OLED/button UI and a Bluetooth LE
control channel that also accepts **downloadable, scriptable programs**.

* **MCU**: ESP32-C3 (developed against an ESP32-C3 Super Mini)
* **Bluetooth**: [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) 2.x
* **Scripting**: [Berry](https://github.com/berry-lang/berry) (integration in progress, see [Scripting](#scripting))
* **Pressure sensor**: Adafruit MPRLS Ported Pressure Sensor Breakout (0–172 kPa / 0–25 PSI absolute, I2C)
* **Pump**: JQB2438 DC vacuum pump through a low side MOSFET, ~60 kPa of suction
* **Display**: Adafruit Monochrome 1.3" 128x64 OLED (SH1106, STEMMA QT / Qwiic)
* **Input**: 4 buttons

## Units

**kPa is the primary unit everywhere** — firmware, BLE replies, scripts and the OLED.
Ambient air is ~101.3 kPa, the MPRLS full scale is 172.4 kPa, and the pump reaches
about 60 kPa of suction below ambient, so kPa keeps the numbers short and the
resolution meaningful.

| Term | Meaning |
| --- | --- |
| `1 kPa` | 10 hPa = 10 mbar = 0.145 PSI = 7.5 mmHg |
| pressure | absolute pressure in kPa |
| `vac` | deficit below the ambient reference, i.e. `ambient - pressure` |
| `pct` | `vac / ambient * 100` |
| `floor` | deepest target the pump can reach, `ambient - 60 kPa` |

## Project layout

| Path | Purpose |
| --- | --- |
| [firmware/eggplan_vacuum/](./firmware/eggplan_vacuum) | The Arduino sketch (four lines) |
| [libraries/VacuumCore/](./libraries/VacuumCore) | The firmware, built as a local library |
| [programs/](./programs) | Reference copies of the Berry programs that ship on the device |
| [tools/vacuum_cli.py](./tools/vacuum_cli.py) | Host side BLE client: upload/run/stop programs, watch telemetry |
| [scripts/arduino.ps1](./scripts/arduino.ps1) | Build, upload, monitor and library install wrapper |
| [scripts/check-layers.ps1](./scripts/check-layers.ps1) | Enforces the dependency rule below |
| [docs/ble-protocol.md](./docs/ble-protocol.md) | GATT layout, reply format and command reference |

### Architecture

```
core/       pure logic: no drivers, no NimBLE, no Adafruit, no Arduino peripherals
hal/        one file per physical device, plus the board pin map
services/   machine policy: what the vacuum system does, driven by the main loop
hosts/      how the outside world drives it: command grammar, BLE, script host
ui/         local presentation: renders the snapshot, emits intents
app/        composition root: the only place that knows the whole graph
```

Dependencies only ever point downwards, and `scripts/check-layers.ps1` fails the
build if one points up:

| Layer | May include |
| --- | --- |
| `core/` | `core/` only |
| `hal/` | `core/` |
| `services/` | `core/`, `hal/` |
| `hosts/` | `core/`, `hal/`, `services/` (and `ble -> commands -> script` inside it) |
| `ui/` | `core/`, `hal/` |
| `app/` | everything; nothing includes `app/` |

Three rules make that hold:

* **No service locator.** Every class takes exactly what it uses, by reference, in its
  constructor. `App` is the only file that knows the whole graph, and a missing
  dependency is a compile error rather than a null pointer at runtime.
* **No shared `util`.** A helper lives with its single owner and is private unless a
  second layer legitimately needs it. `core/` is visible everywhere, so anything
  parked there is public API whether or not that was the intent.
* **The machine view is plain data.** `core/Snapshot.h` carries pressure, pump,
  control, program and device state as values, so `ui/` and `hosts/` can read the
  machine without depending on the modules that produce it.

The script host sits in `hosts/` rather than `services/` because it holds no vacuum
state and no policy: it is an adapter at the machine boundary, the same kind of thing
as the BLE transport.

## Wiring

### Pin map

| Signal | GPIO | Notes |
| --- | --- | --- |
| I2C SDA | 8 | STEMMA QT bus, MPRLS + OLED (ESP32-C3 `Wire` default) |
| I2C SCL | 9 | same bus |
| Pump PWM | 6 | to the MOSFET gate |
| Pump tach | 7 | optional open collector tach, disabled by default |
| Button UP | 0 | active low, internal pull up |
| Button DOWN | 1 | active low, internal pull up |
| Button OK | 3 | active low, internal pull up |
| Button BACK | 10 | active low, internal pull up |
| USB D-/D+ | 18/19 | reserved |
| UART0 | 20/21 | serial console at 115200 |

Spare pins: 2, 4, 5 (GPIO2 is an ESP32-C3 strapping pin, keep it high or floating at
boot). I2C addresses: MPRLS `0x18`, OLED `0x3C`. Pins live in
[hal/Pins.h](./libraries/VacuumCore/src/hal/Pins.h).

If your C3 board has a NeoPixel on GPIO8, either accept that the LED will flicker with
I2C traffic or move the bus to two spare pins.

### Pump driver

The pump is a brushed DC motor and **must not** be driven straight from a GPIO.

```
ESP32 GPIO6 ──┬── 100R ──┬── GATE
              │          │
             (pull      [ MOSFET ]  logic level N-channel, e.g. AO3400 / IRLZ44N
              down)      │
                        SRC ── GND (common with the ESP32)
   +12V ─────────────── PUMP (+) ── DRAIN
                        PUMP (-) ── GND
   flyback diode across the pump terminals (cathode to +12V)
   470–1000 µF bulk capacitor across the pump supply
```

Use a **logic level** MOSFET: the C3 drives the gate with 3.3 V. Give the pump its own
supply and tie the grounds together. `PUMP_DUTY_MAX_PERCENT` (90 %),
`PUMP_RAMP_PERCENT_PER_SEC` and `PUMP_MAX_RUN_MS` (15 minutes) in
[core/Config.h](./libraries/VacuumCore/src/core/Config.h) are the firmware's hard
limits. Set `pins::TACH_ENABLED` to true if you wire a tach output.

## Building

Requires `arduino-cli` (or the Arduino IDE 2 with the ESP32 core installed) and a one
time library install:

```powershell
arduino-cli core install esp32:esp32
powershell -File scripts/arduino.ps1 -Task libs
powershell -File scripts/arduino.ps1 -Task build
powershell -File scripts/arduino.ps1 -Task upload -Port COM5
powershell -File scripts/arduino.ps1 -Task monitor -Port COM5
```

The build uses `--libraries libraries`, so the project local
`libraries/VacuumCore` is used instead of the global sketchbook copy, and it runs
`check-layers.ps1` first. In VS Code the same commands are tasks (`arduino: build`,
`arduino: upload`, `arduino: monitor`, `arduino: install libraries`). For the IDE 2,
run `powershell -File scripts/arduino.ps1 -Task link` once.

Features can be toggled in [core/Config.h](./libraries/VacuumCore/src/core/Config.h)
and [hal/Pins.h](./libraries/VacuumCore/src/hal/Pins.h), or on the command line with
`-DEGGPLAN_WITH_BERRY=1`.

## Using the device

### Buttons

| Button | Action |
| --- | --- |
| UP / DOWN | Navigate: page up/down on STATUS/GRAPH/INFO, list cursor on PROGRAMS, ±1 kPa on SETPOINT |
| OK (click) | PROGRAMS: run the selected program · SETPOINT/STATUS: toggle the hold target on/off |
| OK (hold) | Next page |
| BACK | Stop the running program and the pump |

The default pages are STATUS, PROGRAMS, SETPOINT, GRAPH and INFO. SETPOINT shows the
target's reachable floor and warns when a target is beyond the pump.

### Over Bluetooth

The device advertises as `EggplanVac<ID>` with one primary service, documented in
[docs/ble-protocol.md](./docs/ble-protocol.md):

| Characteristic | Properties | Purpose |
| --- | --- | --- |
| `…6f01` cmd | write | newline terminated command lines |
| `…6f02` rsp | notify/read | replies; long replies are chunked |
| `…6f03` prog | write | program transfer frames (`B` begin, `D` data, `E` commit, `X` abort) |
| `…6f04` info | read | device information |

Replies are space separated `key=value` tokens with token values, so a human with a BLE
terminal can read them and a client parses them in one line:

```
-> status
<- ok kpa=41.23 vac=60.10 pct=59.3 duty=72.0 state=PUMP target=41.0 floor=41.3 fault=- up=123456 ble=1
-> list
<- ok programs 2
<- p hold_vacuum 137 0x1A2B3C4D auto
<- p pulse 331 0xDEADBEEF -
<- eof
-> telemetry 250
<- ok telemetry on 250
<- tel kpa=41.23 vac=60.10 pct=59.3 duty=72.0 state=PUMP target=41.0 floor=41.3 fault=- up=123460 ble=1
```

Quick start with the bundled client:

```powershell
python -m pip install -r tools/requirements.txt
python tools/vacuum_cli.py scan
python tools/vacuum_cli.py status
python tools/vacuum_cli.py upload programs/hold_vacuum.be
python tools/vacuum_cli.py list
python tools/vacuum_cli.py run hold_vacuum
python tools/vacuum_cli.py watch          # live telemetry
```

## Scripting

Scripts are stored in LittleFS as `/programs/<name>.be` and are uploaded over the
`prog` characteristic. They are inert text today: the firmware ships with the
`NullRuntime`, which reports "firmware built without the Berry runtime" for
`run`/`eval` and lets everything else work normally.

The seams for the Berry VM are in place:

* [ScriptRuntime.h](./libraries/VacuumCore/src/hosts/script/ScriptRuntime.h) — the
  runtime interface. This is the project's single abstract class: it keeps vendored,
  macro heavy Berry headers out of the command router.
* [ScriptApi.h](./libraries/VacuumCore/src/hosts/script/ScriptApi.h) — the native
  surface handed to guest code. Every entry point is plain C++, shared by the BLE
  router, the UI and the future bindings.
* `EGGPLAN_WITH_BERRY` in [core/Config.h](./libraries/VacuumCore/src/core/Config.h)
  selects the runtime at build time.
* Cooperative cancellation: `ScriptApi::requestStop()` is polled by the native calls
  (`delay`, `pumpSet`, …) so `stop` can interrupt a running program.
* The script host draws into a `ScriptOverlay` value in `core/Snapshot.h` rather than
  reaching for the display.

The intended script surface (used by the reference programs in [programs/](./programs)):

```berry
vacuum.pump(percent)          # manual duty, 0..90
vacuum.pump_stop()
vacuum.duty()                 # current duty %
vacuum.pressure()             # absolute pressure, kPa
vacuum.vacuum()               # deficit vs ambient, kPa
vacuum.vacuum_percent()       # 0..100
vacuum.target(kpa)            # engage the closed loop
vacuum.target_off()
vacuum.display_line(row, s)   # 7 rows, script overlay page
vacuum.display_bar(fraction)
vacuum.button(index)          # 0..3 is the button held down?
vacuum.wait_button(index, ms) # 0 timeout, 1 click, 2 long, 3 repeat
vacuum.delay(ms)              # cooperative, keeps the UI/BLE serviced
vacuum.fmt(value, decimals)   # "50.0", for building display strings
vacuum.log(message)
vacuum.millis()
```

## Vacuum control model

[VacuumControl](./libraries/VacuumCore/src/services/VacuumControl.cpp) runs a PID loop
on absolute pressure (all terms in kPa) with the pump as the only actuator, so it can
pull down but never vent. When the measurement undershoots the target the output drops
to zero and the chamber bleeds back up. The controller math itself is a pure class in
[core/Pid.h](./libraries/VacuumCore/src/core/Pid.h).

Gains and the deadband live in `core/Config.h` (`PID_KP`, `PID_KI`, `PID_KD`,
`CONTROL_DEADBAND_KPA`) and are starting points, not tuned values — tune them for your
chamber volume and pump.

Protections:

* duty is hard capped at `PUMP_DUTY_MAX_PERCENT`
* a slew rate limit avoids step changes into the diaphragm pump
* `PUMP_MAX_RUN_MS` trips an emergency stop on a runaway control loop
* if the pressure sensor stops answering while the loop is engaged, the pump is cut
  and the state latches to `FAULT` (clear with `reset`)
* targets below `ambient - PUMP_MAX_SUCTION_KPA` only log a warning and pin the duty

## Safety

Vacuum work is not benign: imploding chambers, collapsing tubing and pumped
liquids/vapours can hurt people and destroy hardware. Keep a rated catch pot and filter
in line, do not leave an unattended script running, and remember the firmware's limits
are software limits only.

## Status

* [x] ESP32-C3 project skeleton, build and upload flow
* [x] Layered core/hal/services/hosts/ui/app layout with an enforced dependency rule
* [x] MPRLS driver with median + EMA filtering and ambient reference handling
* [x] PWM pump driver with ramp, duty cap, run time protection and optional tach
* [x] PID vacuum controller with a reachable-floor check
* [x] SH1106 UI, 4 button navigation, toast, graph page
* [x] LittleFS program store with chunked, checksum verified uploads
* [x] NimBLE command / response / program / info service
* [x] Host side Python client
* [ ] Berry VM integration (`EGGPLAN_WITH_BERRY=1`) and the `vacuum` module bindings
* [ ] Optional BLE bonding / static passkey
