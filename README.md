# Eggplan Vacuum

An ESP32-C3 vacuum pump controller with a local OLED/button UI and a Bluetooth
LE control channel that also accepts **downloadable, scriptable programs**.

* **MCU**: ESP32-C3 (developed against an ESP32-C3 Super Mini)
* **Bluetooth**: [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) 2.x
* **Scripting**: [Berry](https://github.com/berry-lang/berry) (integration in progress, see [Scripting](#scripting))
* **Pressure sensor**: Adafruit MPRLS Ported Pressure Sensor Breakout (0–172 kPa / 0–25 PSI absolute, I2C)
* **Pump**: JQB2438 DC vacuum pump through a low side MOSFET
* **Display**: Adafruit Monochrome 1.3" 128x64 OLED (SH1106, STEMMA QT / Qwiic)
* **Input**: 4 buttons

## Units

**kPa is the primary unit everywhere** — firmware, BLE JSON, scripts and the OLED
all speak kPa. Ambient air is ~101.3 kPa, the MPRLS full scale is 172.4 kPa, and
the parts are comfortable below 60 kPa (`cfg::WORKING_RANGE_MAX_KPA`), so kPa
keeps the numbers short and the resolution meaningful.

| Term | Meaning |
| --- | --- |
| `1 kPa` | 10 hPa = 10 mbar = 0.145 PSI = 7.5 mmHg |
| pressure | absolute pressure in kPa |
| `vacuum` | deficit below the ambient reference, i.e. `ambient - pressure` |
| `vacuum_pct` | `vacuum / ambient * 100` |

## Project layout

| Path | Purpose |
| --- | --- |
| [firmware/eggplan_vacuum/](./firmware/eggplan_vacuum) | The Arduino sketch (deliberately tiny) |
| [libraries/VacuumCore/](./libraries/VacuumCore) | All firmware modules, built as a local library |
| [programs/](./programs) | Reference copies of the Berry programs that ship on the device |
| [tools/vacuum_cli.py](./tools/vacuum_cli.py) | Host side BLE client: upload/run/stop programs, watch telemetry |
| [scripts/arduino.ps1](./scripts/arduino.ps1) | Build, upload, monitor and library install wrapper |
| [docs/ble-protocol.md](./docs/ble-protocol.md) | GATT layout, framing and command reference |

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

Spare pins: 2, 4, 5 (GPIO2 is an ESP32-C3 strapping pin, keep it high or floating at boot).

I2C addresses: MPRLS `0x18`, OLED `0x3C`.

### I2C

The MPRLS breakout and the OLED both sit on the same bus, so chain them with
STEMMA QT cables. If your C3 board has a NeoPixel on GPIO8, either accept that the
LED will flicker with I2C traffic or move the bus to two spare pins by editing
`cfg::PIN_I2C_SDA` / `cfg::PIN_I2C_SCL` in
[VacuumConfig.h](./libraries/VacuumCore/src/VacuumConfig.h).

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

Notes:

* Use a **logic level** MOSFET: the C3 drives the gate with 3.3 V.
* Give the pump its own supply and tie the grounds together.
* Confirm the JQB2438's rated voltage/current before connecting it, and keep the
  20 kHz PWM drive within the pump's ratings. `cfg::PUMP_DUTY_MAX_PERCENT` (90 %)
  and `cfg::PUMP_MAX_RUN_MS` (15 minutes) are the firmware's hard limits.
* Set `cfg::PUMP_TACH_ENABLED` to `true` if you wire a tach/hall output; the
  firmware then reports RPM over BLE and on the `INFO` page.

## Building

Requires `arduino-cli` (or the Arduino IDE 2 with the ESP32 core installed) and a
one time library install:

```powershell
arduino-cli core install esp32:esp32
powershell -File scripts/arduino.ps1 -Task libs
powershell -File scripts/arduino.ps1 -Task build
powershell -File scripts/arduino.ps1 -Task upload -Port COM5
powershell -File scripts/arduino.ps1 -Task monitor -Port COM5
```

The build uses `--libraries libraries`, so the project local
`libraries/VacuumCore` is used instead of the global sketchbook copy. In VS Code
the same commands are available as tasks (`arduino: build`, `arduino: upload`,
`arduino: monitor`, `arduino: install libraries`). If you prefer the IDE 2, run
`powershell -File scripts/arduino.ps1 -Task link` once to junction the project
library into your sketchbook.

Features can be toggled from `VacuumConfig.h` (pins, addresses, PWM frequency,
safety caps) or on the command line with `-DEGGPLAN_WITH_BERRY=1`.

## Using the device

### Buttons

| Button | Action |
| --- | --- |
| UP / DOWN | Navigate: page up/down on STATUS/GRAPH/INFO, list cursor on PROGRAMS, ±1 kPa on SETPOINT |
| OK (click) | PROGRAMS: run the selected program · SETPOINT/STATUS: toggle the hold target on/off |
| OK (hold) | Next page |
| BACK | Stop the running program and the pump |

The default pages are STATUS, PROGRAMS, SETPOINT, GRAPH and INFO.

### Over Bluetooth

The device advertises as `EggplanVac<ID>` with one primary service, documented in
[docs/ble-protocol.md](./docs/ble-protocol.md):

| Characteristic | Properties | Purpose |
| --- | --- | --- |
| `…6f01` cmd | write | newline terminated command lines, same grammar as the serial console |
| `…6f02` rsp | notify/read | replies; long replies are chunked |
| `…6f03` prog | write | program transfer frames (`B` begin, `D` data, `E` commit, `X` abort) |
| `…6f04` info | read | device information JSON |

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

Any generic BLE UART app works too: write `status\n` to `cmd` and read `rsp`.

## Scripting

Scripts are stored in LittleFS as `/programs/<name>.be` and are uploaded over the
`prog` characteristic. They are inert text today: the firmware ships with the
`NullRuntime`, which reports "firmware built without the Berry runtime" for
`run`/`eval` and lets everything else work normally.

The seams for the Berry VM are already in place:

* [ScriptRuntime.h](./libraries/VacuumCore/src/ScriptRuntime.h) — `ScriptRuntime`
  interface plus the full `ScriptApi` native surface (`pumpSet`, `pressureKpa`,
  `vacuumKpa`, `targetKpa`, `displayLine`, `buttonWait`, `delay`, `log`).
  Every entry point is plain C++ so the BLE command router, the local UI and the
  future Berry bindings all share one implementation.
* `EGGPLAN_WITH_BERRY` in [VacuumConfig.h](./libraries/VacuumCore/src/VacuumConfig.h)
  selects the runtime at build time.
* Cooperative cancellation: `ScriptApi::requestStop()` is polled by the native
  calls (`delay`, `pumpSet`, …) so `stop` can interrupt a running program. The
  planned Berry runtime runs the VM in its own task and unwinds through the same
  flag.

The intended script surface (used by the reference programs in
[programs/](./programs)) is:

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
vacuum.fmt(value, decimals)   # "40.2", for building display strings
vacuum.log(message)
vacuum.millis()
```

## Vacuum control model

`VacuumController` runs a PID loop on absolute pressure (all terms in kPa) with
the pump as the only actuator, so it can pull down but never vent. When the
measurement undershoots the target the output drops to zero and the chamber bleeds
back up. Gains and the deadband live in `VacuumConfig.h` (`PID_KP`, `PID_KI`,
`PID_KD`, `CONTROL_DEADBAND_KPA`) and are starting points, not tuned values — tune
them for your chamber volume and pump. `CONTROL_MIN_TARGET_KPA` sets the floor for
targets, and a target above `WORKING_RANGE_MAX_KPA` only logs a warning.

Protections:

* duty is hard capped at `PUMP_DUTY_MAX_PERCENT`
* a slew rate limit avoids step changes into the diaphragm pump
* `PUMP_MAX_RUN_MS` trips an emergency stop on a runaway control loop
* if the pressure sensor stops answering while the loop is engaged, the pump is
  cut and the state latches to `FAULT` (clear with `reset`)

## Safety

Vacuum work is not benign: imploding chambers, collapsing tubing and pumped
liquids/vapours can hurt people and destroy hardware. Keep a rated catch pot and
filter in line, do not leave an unattended script running, and remember the
firmware's limits are software limits only.

## Status / next steps

* [x] ESP32-C3 project skeleton, build and upload flow
* [x] MPRLS driver with median + EMA filtering and ambient reference handling
* [x] PWM pump driver with ramp, duty cap and run time protection
* [x] SH1106 UI, 4 button navigation, toast, graph page
* [x] LittleFS program store with chunked verified uploads
* [x] NimBLE command / response / program / info service
* [x] Host side Python client
* [ ] Berry VM integration (`EGGPLAN_WITH_BERRY=1`) and the `vacuum` module bindings
* [ ] Optional BLE bonding / static passkey
