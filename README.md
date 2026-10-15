# Eggplan Vacuum

An ESP32-C3 vacuum pump controller. It pulls a chamber to a target pressure, holds it
for a timer, then vents it — driven entirely over Bluetooth LE from a central.

* **MCU**: ESP32-C3 (developed against an ESP32-C3 Super Mini)
* **Bluetooth**: [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) 2.x
* **Pressure sensor**: Adafruit MPRLS Ported Pressure Sensor Breakout (0–172 kPa / 0–25 PSI absolute, I2C)
* **Pump**: JQB2438 DC vacuum pump through a low side MOSFET, ~60 kPa of suction
* **Vent valve**: normally open solenoid on a low side MOSFET
* **Display**: Adafruit Monochrome 1.3" 128x64 OLED (SH1106, STEMMA QT / Qwiic)
* **Battery**: 1S Li-ion pack sensed through a divider on GPIO4

There is no local control surface: the display is a readout and the central decides
everything. What the device owns is what has to happen when nobody is watching — pulling
to the target, expiring the timer, and venting the chamber through one path on stop,
abort, link loss or fault.

## Units

**kPa is the primary unit everywhere** — firmware, BLE replies and the OLED. Ambient air
is ~101.3 kPa, the MPRLS full scale is 172.4 kPa, and the pump reaches about 60 kPa of
suction below ambient, so kPa keeps the numbers short and the resolution meaningful.

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
| [tools/vacuum_cli.py](./tools/vacuum_cli.py) | Host side BLE client: status, start/stop, settings, telemetry |
| [scripts/arduino.ps1](./scripts/arduino.ps1) | Build, upload, monitor and library install wrapper |
| [scripts/check-layers.ps1](./scripts/check-layers.ps1) | Enforces the dependency rule below |
| [docs/ble-protocol.md](./docs/ble-protocol.md) | The BLE protocol: GATT layout, grammar, keys, commands, states |

### Architecture

```
core/      pure logic: no drivers, no NimBLE, no Adafruit, no Arduino peripherals
hal/       one file per physical device, plus the board pin map
services/  machine policy: what the vacuum system does, driven by the main loop
hosts/     how the outside world drives it: command grammar, BLE transport
ui/        local presentation: draws the snapshot, holds no state
app/       composition root: the only place that knows the whole graph
```

Dependencies only ever point downwards, and `scripts/check-layers.ps1` fails the build
if one points up:

| Layer | May include |
| --- | --- |
| `core/` | `core/` only |
| `hal/` | `core/` |
| `services/` | `core/`, `hal/` |
| `hosts/` | `core/`, `hal/`, `services/` (and `ble -> commands` inside it) |
| `ui/` | `core/`, `hal/` |
| `app/` | everything; nothing includes `app/` |

Three rules make that hold:

* **No service locator.** Every class takes exactly what it uses, by reference, in its
  constructor. `App` is the only file that knows the whole graph, and a missing
  dependency is a compile error rather than a null pointer at runtime.
* **No shared `util`.** A helper lives with its single owner and is private unless a
  second layer legitimately needs it. `core/` is visible everywhere, so anything parked
  there is public API whether or not that was the intent.
* **The machine view is plain data.** [core/Snapshot.h](./libraries/VacuumCore/src/core/Snapshot.h)
  carries pressure, pump, run, battery and device state as values, so `ui/` and `hosts/`
  can read the machine without depending on the modules that produce it.

## Wiring

### Pin map

| Signal | GPIO | Notes |
| --- | --- | --- |
| I2C SDA | 8 | STEMMA QT bus, MPRLS + OLED (ESP32-C3 `Wire` default) |
| I2C SCL | 9 | same bus |
| Pump PWM | 6 | to the MOSFET gate |
| Pump tach | 7 | optional open collector tach, disabled by default |
| Vent valve | 5 | MOSFET gate; **energise to close** |
| Battery sense | 4 | 1S Li-ion through a ÷2 divider |
| Button UP | 0 | reserved, nothing reads it |
| Button DOWN | 1 | reserved, nothing reads it |
| Button OK | 3 | reserved, nothing reads it |
| Button BACK | 10 | reserved, nothing reads it |
| USB D-/D+ | 18/19 | reserved |
| UART0 | 20/21 | serial log at 115200 |

Spare pin: 2 (an ESP32-C3 strapping pin, keep it high or floating at boot). The four
button pins are still wired and named for a future local override, but no code reads
them. GPIO0–3 are also ADC1 channels, which is why the battery sense is on GPIO4: a
pressed button would pull the sense line to ground. I2C addresses: MPRLS `0x18`, OLED
`0x3C`. Pins live in [hal/Pins.h](./libraries/VacuumCore/src/hal/Pins.h).

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

### Vent valve

**The valve is normally open.** De-energised means open, which means the chamber vents —
so *running a vacuum means energising the coil to close the valve*. That makes the
fail-safe physical rather than logical: a crash, a watchdog reset, a brownout or a cut
wire all de-energise the coil and the chamber vents whether or not the firmware noticed.
Wire it like the pump (GPIO5, logic level MOSFET, flyback diode across the coil):

```
ESP32 GPIO5 ──┬── 100R ──┬── GATE
              │          │
             (pull      [ MOSFET ]  logic level N-channel
              down)      │
                        SRC ── GND (common with the ESP32)
    +12V ─────── VALVE (+) ── DRAIN
                 VALVE (-) ── GND
    flyback diode across the coil (cathode to +12V)
```

A solenoid holds current for as long as it is closed, so **the valve, not the pump, is
the dominant battery cost on a long hold** — a 1S cell will not last an hour of
solenoid duty. Prefer a latching or low-power valve if a long hold has to run on
battery.

### Battery

```
BAT+ ── 100k ──┬── GPIO4
               │
              100k ── GND              1S Li-ion, 3.2–4.2 V
```

The firmware averages `analogReadMilliVolts` and maps the pack linearly between
`BATTERY_EMPTY_VOLTS` (3.2 V) and `BATTERY_FULL_VOLTS` (4.2 V). The ESP32 ADC is not lab
grade — expect ±3–5 % — so `BATTERY_CALIBRATION` in
[core/Config.h](./libraries/VacuumCore/src/core/Config.h) trims the divider against a
meter. Below `BATTERY_PRESENT_MIN_VOLTS` the gauge reports "no pack fitted" instead of a
permanent 0 %, and nothing acts on a low battery: the icon and the `batt` key are the
whole story.

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

The build uses `--libraries libraries`, so the project local `libraries/VacuumCore` is
used instead of the global sketchbook copy, and it runs `check-layers.ps1` first. In VS
Code the same commands are tasks (`arduino: build`, `arduino: upload`,
`arduino: monitor`, `arduino: install libraries`). For the IDE 2, run
`powershell -File scripts/arduino.ps1 -Task link` once.

Policy tunables live in [core/Config.h](./libraries/VacuumCore/src/core/Config.h) and
wiring in [hal/Pins.h](./libraries/VacuumCore/src/hal/Pins.h).

## Using the device

### Display

One page, no interaction — the display is the only local feedback, and everything an
operator can do is a BLE command:

```
  IDLE                                    ▐███▐ 78%
                41.3 kPa
  tgt 40.0 kPa                    duty 72%
                   09:32
  [######################----------------]
```

| Element | Notes |
| --- | --- |
| state | `IDLE`, `PULL`, `HOLD`, `VENT`, `FAULT`; inverted when faulted |
| battery | fill by percent, blinks below 20 % |
| pressure | current absolute pressure, size 2 |
| target / duty | `tgt --` when no target is set |
| countdown | hold time remaining, `--:--` when no timer is running, inverted in the last minute |
| duty bar | across the bottom |

A fault replaces the target row with an inverted banner (`FAULT max_run`), and a missing
sensor banners `NO SENSOR`. Boot is safe: pump off, valve open, `IDLE`, and nothing is
remembered across a reset.

### Over Bluetooth

The device advertises as `EggplanVac<ID>`. Protocol version 2, documented in
[docs/ble-protocol.md](./docs/ble-protocol.md):

| Characteristic | Properties | Purpose |
| --- | --- | --- |
| `…6f01` cmd | write | newline terminated command lines |
| `…6f02` rsp | notify/read | exactly one reply per command; long replies are chunked |
| `…6f03` state | notify/read | telemetry; **subscribing is what enables it** |
| `…6f04` info | read | device identity, including `proto=2` |

Every reply is one line of space separated `key=value` tokens, so a human with a BLE
terminal can read it and a client parses it in one line:

```
-> info
<- ok proto=2 fw=0.1.0 chip=ESP32-C3 heap=279384 tach=0 batt_v=3.94 batt_pct=78
-> set target 40
<- ok target=40.0
-> set timer 600
<- ok timer=600
-> start
<- ok started
-> status
<- ok kpa=41.23 vac=60.10 pct=59.3 target=40.0 floor=41.3 duty=72.0 state=HOLD remain=572 total=600 batt=78 sensor=1 fault=none up=123456
-> stop
<- ok stopped
```

Quick start with the bundled client:

```powershell
python -m pip install -r tools/requirements.txt
python tools/vacuum_cli.py scan
python tools/vacuum_cli.py info
python tools/vacuum_cli.py set target 40
python tools/vacuum_cli.py set timer 600
python tools/vacuum_cli.py start
python tools/vacuum_cli.py watch          # live telemetry, Ctrl+C to stop
python tools/vacuum_cli.py stop
```

## Vacuum control model

[VacuumControl](./libraries/VacuumCore/src/services/VacuumControl.cpp) runs a PID loop on
absolute pressure (all terms in kPa) with the pump as the only actuator, so it can pull
down but never vent — the vent valve is the only way back up. When the measurement
undershoots the target the output drops to zero and the chamber bleeds back up. The
controller math itself is a pure class in
[core/Pid.h](./libraries/VacuumCore/src/core/Pid.h).

```
        start
IDLE ──────────> PULL ──(target reached)──> HOLD ──(timer 0)──┐
  ^               │        countdown starts                    │
  │               │  stop / link loss / fault                  │
  │               v                    v                       v
  └──(vent complete)──────────────── VENT <─────────────────────┘
```

* The countdown is **hold time remaining** and starts when the target is first reached,
  so it sits at its full value while pulling. No timer means pull down and stay there.
* The coil is energised through `PULL` and `HOLD`, and released in `IDLE` and `VENT`.
* Venting ends when the pressure is within 2 kPa of ambient, or after 30 s, whichever
  comes first — a leaky rig takes longer, and that is logged rather than treated as an
  error.
* Faults latch. Only `reset` clears one; the device never retries on its own.
* Disconnecting, or silence from a connected central once `LINK_TIMEOUT_MS` is non-zero,
  stops the run and vents.

Gains and the deadband live in `core/Config.h` (`PID_KP`, `PID_KI`, `PID_KD`,
`CONTROL_DEADBAND_KPA`) and are starting points, not tuned values — tune them for your
chamber volume and pump.

Protections, all of which act without consulting the central:

* duty is hard capped at `PUMP_DUTY_MAX_PERCENT`
* a slew rate limit avoids step changes into the diaphragm pump
* `PUMP_MAX_RUN_MS` trips an emergency stop on a runaway control loop
* if the pressure sensor stops answering mid-run, the pump is cut and the state latches
  to `FAULT` (clear with `reset`)
* targets below `ambient - PUMP_MAX_SUCTION_KPA` only log a warning and pin the duty

## Safety

Vacuum work is not benign: imploding chambers, collapsing tubing and pumped
liquids/vapours can hurt people and destroy hardware. Keep a rated catch pot and filter
in line, and remember that the firmware's limits are software limits only.

The one hardware guarantee is the valve: it is normally open, so losing power or losing
control releases the chamber. Do not invert that relationship by wiring a normally
closed valve to the same output.

## Roadmap

* **No scripting in this build.** An earlier revision stored Berry programs in LittleFS
  and ran them over BLE (`hosts/script`, `services/ProgramStore`, the `prog`
  characteristic). It is all in git at commit `f35a3bb` if it is ever wanted again: the
  layering, the `set …` grammar and the snapshot are still there to hang it from.
* Optional BLE bonding / static passkey.

## Status

* [x] ESP32-C3 project skeleton, build and upload flow
* [x] Layered core/hal/services/hosts/ui/app layout with an enforced dependency rule
* [x] MPRLS driver with median + EMA filtering and ambient reference handling
* [x] PWM pump driver with ramp, duty cap, run time protection and optional tach
* [x] PID vacuum controller with a reachable-floor check
* [x] Run model: target, hold timer, vent valve and link-loss release
* [x] 1S Li-ion battery gauge
* [x] Single page SH1106 readout
* [x] NimBLE v2 protocol: command, reply, telemetry, info
* [x] Host side Python client
