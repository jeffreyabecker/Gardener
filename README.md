# Gardener

An ESP32-S3 vacuum pump controller. It pulls a chamber to a target depth below ambient, holds it
for a timer, then vents it — driven over a serial line by a controller at the other end.

* **MCU**: ESP32-S3 (developed against an ESP32-S3 Super Mini)
* **Interface**: USB CDC, one line oriented protocol ([docs/serial-protocol.md](./docs/serial-protocol.md))
* **Pressure sensor**: Adafruit MPRLS Ported Pressure Sensor Breakout (0–172 kPa / 0–25 PSI absolute, I2C)
* **Pump**: JQB2438 DC vacuum pump through a low side MOSFET, ~60 kPa of suction
* **Vent valve**: normally open solenoid on a low side MOSFET
* **Display**: Adafruit Monochrome 1.3" 128x64 OLED (SH1106, STEMMA QT / Qwiic)
* **Battery**: 1S Li-ion pack sensed through a divider on GPIO4

The parts list, and the firmware constant each one is tied to, is in
[docs/bom.md](./docs/bom.md).

There is no local control surface: the display is a readout and the controller decides
everything. What the device owns is what has to happen when nobody is watching — pulling to
the target, expiring the timer, and venting the chamber through one path on stop, abort, link
loss or fault.

## Units

**kPa is the primary unit everywhere** — firmware, replies and the OLED. Ambient air is
~101.3 kPa, the MPRLS full scale is 172.4 kPa, and the pump reaches about 60 kPa of suction
below ambient, so kPa keeps the numbers short and the resolution meaningful.

| Term | Meaning |
| --- | --- |
| `1 kPa` | 10 hPa = 10 mbar = 0.145 PSI = 7.5 mmHg |
| pressure | absolute pressure in kPa, reported as `kpa` |
| `vac` | deficit below the ambient reference, i.e. `ambient - pressure` |
| `target` | depth below the same reference the loop pulls to, so a target and a reading are on one ruler |
| `pct` | `vac / ambient * 100` |
| `floor` | deepest target the pump can reach, `PUMP_MAX_SUCTION_KPA` below the reference |

The ambient reference is **captured, never dialled in** — the device takes it from the sensor
(`ambient`), at boot and on request, and nothing else can set one. Targets are relative to it,
so capturing a new one restates every target, and both want the rig at rest when it happens.

## Project layout

| Path | Purpose |
| --- | --- |
| [firmware/gardener/](./firmware/gardener) | The Arduino sketch (four lines) |
| [libraries/VacuumCore/](./libraries/VacuumCore) | The firmware, built as a local library |
| [scripts/arduino.ps1](./scripts/arduino.ps1) | Build, upload, monitor and library install wrapper |
| [docs/serial-protocol.md](./docs/serial-protocol.md) | The protocol: port, grammar, keys, commands, the panel, states, rates |
| [docs/bom.md](./docs/bom.md) | The bill of materials: every part, and the constant each one is tied to |

### Architecture

```
core/   shared foundations: config, math, the machine view
hal/    what the instrument needs from its hardware: an abstract class per part —
        Display, PressureSensor, Pump, VentValve — the battery gauge, and the board pin
        map. The parts themselves are in hal/impl/, which is the only place a vendor
        library is included
app/    the instrument: the plant that commands them, the display overlay, the serial
        interface, and the composition root that wires it all up and runs the loop
```

Dependencies only ever point downwards. Nothing checks it — keep it that way by reading:

| Layer | May include |
| --- | --- |
| `core/` | `core/` only |
| `hal/` | `core/` |
| `app/` | everything; nothing includes `app/` |

Three rules make that hold:

* **No service locator.** Nothing reaches for a global to find what it needs: a class is handed
  what it uses at construction, or owns it outright. `App` is the only file that knows the whole
  graph, and a missing dependency is a compile error rather than a null pointer at runtime.
* **No shared `util`.** A helper lives with its single owner. `core/` is visible everywhere,
  so anything parked there is public API whether or not that was the intent.
* **The machine view is plain data.** [core/Snapshot.h](./libraries/VacuumCore/src/core/Snapshot.h)
  carries pressure, pump, run, battery and device state as values. One snapshot is rebuilt in
  place each loop iteration, so `app/` can format a reply and draw the display from the same
  instant without either reader holding a driver reference, or a driver knowing that a display
  or a controller exists.

`hal/` never sees `app/`, which is what keeps a driver independent of the instrument built on top
of it: a part reports what it is doing as values, and nothing in `hal/` knows that a serial line or
a control loop exists. The plant is the other side of that line —
[app/VacuumSystem.h](./libraries/VacuumCore/src/app/VacuumSystem.h) is handed the sensor, the pump
and the valve and holds the loop that commands them, so the control law reads in one place and the
pins and the bus read in another. Each of those is an interface, and the part that satisfies it is in
`hal/impl/`, which is where the pin map is read and where `<Adafruit_MPRLS.h>` and
`<Adafruit_GrayOLED.h>` appear and nowhere else.

## Wiring

### Pin map

| Signal | GPIO | Notes |
| --- | --- | --- |
| I2C SDA | 8 | STEMMA QT bus, MPRLS + OLED (ESP32-S3 `Wire` default) |
| I2C SCL | 9 | same bus |
| Pump PWM | 6 | to the MOSFET gate |
| Vent valve | 5 | MOSFET gate; **energise to close** |
| Battery sense | 4 | 1S Li-ion through a ÷2 divider (ADC1 channel 3) |
| Button UP | 1 | reserved, nothing reads it |
| Button DOWN | 2 | reserved, nothing reads it |
| Button OK | 10 | reserved, nothing reads it |
| Button BACK | 11 | reserved, nothing reads it |
| USB D-/D+ | 19/20 | native USB: this is the control interface |
| UART0 | 43/44 | alternative control interface, 115200 |
| RGB LED | 48 | on board, not wired to anything |

Pins 7 and 12–18 are free, and the four button pins are wired and named for a future local
override but no code reads them. The battery sense is on GPIO4 because it is the one ADC1 pin
that is not a button — a pressed button would pull the sense line to ground. Nothing in the
firmware touches the pins that have to be left alone at reset (strapping 0, 3, 45, 46) or the
USB pins (19/20), and on a Super Mini GPIO0 is only the BOOT button. I2C addresses: MPRLS
`0x18`, OLED `0x3C`. Pins live in [hal/Pins.h](./libraries/VacuumCore/src/hal/Pins.h).

The on-board addressable RGB LED is on GPIO48, which is clear of the I2C bus, so it needs no
special handling.

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

Use a **logic level** MOSFET: the S3 drives the gate with 3.3 V. Give the pump its own supply
and tie the grounds together. `PUMP_DUTY_MAX_PERCENT` (90 %) and `PUMP_RAMP_PERCENT_PER_SEC` in
[core/Config.h](./libraries/VacuumCore/src/core/Config.h) are the firmware's hard limits, and
they are the pump's only protections: the device never ends a run of its own accord, so whoever
is driving it decides when the pump stops.

### Vent valve

**The valve is normally open.** De-energised means open, which means the chamber vents — so
*running a vacuum means energising the coil to close the valve*. That makes the fail-safe
physical rather than logical: a crash, a watchdog reset, a brownout or a cut wire all
de-energise the coil and the chamber vents whether or not the firmware noticed. Wire it like
the pump (GPIO5, logic level MOSFET, flyback diode across the coil):

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

A solenoid holds current for as long as it is closed, so **the valve, not the pump, is the
dominant battery cost on a long hold** — a 1S cell will not last an hour of solenoid duty.
Prefer a latching or low-power valve if a long hold has to run on battery.

### Battery

```
BAT+ ── 100k ──┬── GPIO4
               │
              100k ── GND              1S Li-ion, 3.2–4.2 V
```

The firmware averages `analogReadMilliVolts` and maps the pack linearly between
`BATTERY_EMPTY_VOLTS` (3.2 V) and `BATTERY_FULL_VOLTS` (4.2 V). The ESP32 ADC is not lab
grade — expect ±3–5 % — so `BATTERY_CALIBRATION` in
[core/Config.h](./libraries/VacuumCore/src/core/Config.h) trims the divider against a meter. A
reading below 1 V is treated as "no pack fitted" rather than a permanent 0 %, and nothing acts
on a low battery: the icon and the `batt` key are the whole story.

## Building

Requires `arduino-cli` (or the Arduino IDE 2 with the ESP32 core installed) and a one time
library install:

```powershell
arduino-cli core install esp32:esp32
powershell -File scripts/arduino.ps1 -Task libs
powershell -File scripts/arduino.ps1 -Task build
powershell -File scripts/arduino.ps1 -Task upload -Port COM5
powershell -File scripts/arduino.ps1 -Task monitor -Port COM5
```

The build uses `--libraries libraries`, so the project local `libraries/VacuumCore` is used
instead of the global sketchbook copy. In VS Code the same commands are tasks (`arduino: build`,
`arduino: upload`, `arduino: monitor`, `arduino: install libraries`).

**The board options matter**: the build pins
`esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc`, which is what maps `Serial` to the chip's
native USB CDC. Build without `CDCOnBoot` and the whole interface goes to UART0 on GPIO43/44
instead, so a USB cable sees nothing at all. It is pinned in `firmware/gardener/sketch.yaml`,
`scripts/arduino.ps1` and `.vscode/arduino.json`.

Policy tunables live in [core/Config.h](./libraries/VacuumCore/src/core/Config.h) and wiring in
[hal/Pins.h](./libraries/VacuumCore/src/hal/Pins.h).

## Using the device

### Display

The panel is a peripheral: the host draws it over the serial line, and the device draws exactly
two things of its own — the boot splash, and the battery on top of every frame.

```
  Gardener
  fw 0.5.0                              ▐███▐          <- device: splash, then the battery
```

Everything else is whatever the host sent: `draw` commands fill the panel's buffer, and `flush`
puts it on the glass. A host that wants the readout the old page drew sends about twenty lines
per frame:

```
-> draw clear
<- ok drawn
-> draw text 0 0 1 0 IDLE
<- ok drawn
-> draw text 48 16 2 0 41.3
<- ok drawn
-> draw rect 0 40 100 8
<- ok drawn
-> draw fillrect 1 41 72 6
<- ok drawn
-> flush
<- ok flushed
```

| Who draws | What |
| --- | --- |
| the device | the splash at boot, and the battery on every frame, both through the overlay so a host cannot erase them — the battery clears its own footprint first, so a host cannot draw into it either |
| the host | everything else: pressure, target, state, countdown, bars, icons, in whatever layout it likes |

Boot is safe: pump off, valve open, `IDLE`, the splash on the panel, and nothing is remembered
across a reset. With no host talking, the panel holds the last frame it was given — the splash,
if a host never drew.

### Over the serial line

Open the port at 115200 8N1 and send newline terminated commands. Every command produces
exactly one `ok`/`err` reply line, and lines starting with `[` are diagnostics rather than
replies:

```
-> info
<- ok fw=0.5.0 chip=ESP32-S3 heap=279384 log=on batt_v=3.94 batt_pct=78
-> set target 40
<- ok target=40.0
-> start
<- ok started
-> status
<- ok kpa=61.26 vac=40.07 pct=39.5 target=40.0 floor=60.0 duty=72.0 state=HOLD batt=78 sensor=1 fault=none up=123456
-> stop
<- ok stopped
```

That last pair is the whole run model in miniature: the device reaches the target, reports
`HOLD`, and waits. The controller times the hold and sends `stop` when it is done — the device
has no duration of its own and will hold until told, and no command that takes one.

The full grammar, key tables, error codes and state model are in
[docs/serial-protocol.md](./docs/serial-protocol.md). Anything that can open a serial port and
send a line works as a client: there is none in this repository at the moment, deliberately,
while the interface is being reworked.

## Vacuum control model

One class, because the state the chamber is in and the state the run is in turned out to be the
same state.

[VacuumSystem](./libraries/VacuumCore/src/app/VacuumSystem.cpp) is the plant and the machine on
top of it: the target, the PID on pressure relative to the captured ambient (all terms in kPa,
and the same frame a target is set in), the valve, the pump, the run state in the diagram below
and the fault latch. It is handed the three devices it commands — the sensor, the pump and the
valve — so the loop is policy and the pins stay in `hal/`. The pump is the only actuator, so the
loop can pull down but never vent — the vent valve is the only way back up. When the measurement
undershoots the target the output drops to zero and the chamber bleeds back up.

It is told what to do — regulate, start, stop, rest — and it reports what is true: the pressure,
the duty, the state. Both of the transitions it makes for itself are facts about the chamber
rather than orders — the target has been reached, the release is over — so they are decided where
the measurements are, and each command sets the state it causes. Every "should" is still a
command's: when to start, what a stop means, whether a fault is permanent. The run's *duration*
lives at the other end of the cable, with the controller. Clearing the target releases the
chamber, because a run without a target is not a run.

```
        start                                              stop / link loss / fault
IDLE ──────────> PULL ──(target reached)──> HOLD ───────────────────> VENT ──(complete)──> IDLE
  ^                                              │                      │
  │                                              │                      v
  └──────────────────── reset ───────────────────┴─────────────────── FAULT
```

* The controller owns how long a hold lasts. The device goes to `HOLD` when its own deadband
  says the target has been reached, and that is all `HOLD` now means: it is the cue for the
  controller to start timing, and the run continues until the controller sends `stop`.
* Nothing in the device ends a run on its own. There is no timer, no timeout and no maximum
  run time, so an unattended hold holds until someone stops it.
* The countdown is the host's job: the host holds the timer and draws it, and the device renders
  nothing of its own except the splash and the battery.
* The coil is energised through `PULL` and `HOLD`, and released in `IDLE` and `VENT`.
* Venting ends when the pressure is within 2 kPa of ambient, or after 30 s, whichever comes
  first — a leaky rig takes longer, and that is logged rather than treated as an error.
* Faults latch. Only `reset` clears one; the device never retries on its own.
* Disconnecting the controller stops the run and vents. That is the one automatic ending left,
  and it does not help with a controller that hangs with the port open.

Gains and the deadband live in `core/Config.h` (`PID_KP`, `PID_KI`, `PID_KD`,
`CONTROL_DEADBAND_KPA`) and are starting points, not tuned values — tune them for your chamber
volume and pump.

Protections, all of which act without consulting the controller:

* duty is hard capped at `PUMP_DUTY_MAX_PERCENT`
* a slew rate limit avoids step changes into the diaphragm pump
* if the pressure sensor stops answering mid-run, the pump is cut and the state latches to
  `FAULT` (clear with `reset`)
* targets deeper than `floor` only log a warning and pin the duty

Those four are the whole list. A target the pump cannot reach pins the duty at the cap and
nothing stops it, so a controller that wants a ceiling has to keep it: read `floor` from
`status` and keep targets no deeper than it.

## Safety

Vacuum work is not benign: imploding chambers, collapsing tubing and pumped liquids/vapours
can hurt people and destroy hardware. Keep a rated catch pot and filter in line, and remember
that the firmware's limits are software limits only.

The one hardware guarantee is the valve: it is normally open, so losing power or losing
control releases the chamber. Do not invert that relationship by wiring a normally closed
valve to the same output.

## Roadmap

* **No host client in this build.** The serial client that used to live in `tools/` was deleted
  while the interface is in churn — it is in git at commit `85f3cbb` if it is wanted back.
* **No scripting in this build.** An earlier revision stored Berry programs in LittleFS and
  ran them from the device; it is in git at commit `f35a3bb` if it is ever wanted again.
* The line grammar is the whole interface, so anything that can carry newline terminated
  text — another microcontroller, a bridge, a terminal — can drive the device as it stands.

## Status

* [x] ESP32-S3 project skeleton, build and upload flow
* [x] Layered core/hal/app layout, with the dependency direction kept by hand
* [x] MPRLS driver with median + EMA filtering and ambient reference handling
* [x] PWM pump driver with ramp and duty cap
* [x] PID vacuum controller with a reachable-floor check
* [x] Run model: target, vent valve and link-loss release, with the duration owned by the controller
* [x] 1S Li-ion battery gauge
* [x] Host drawn panel: a `Display` interface, an Adafruit adapter, and a battery overlay
* [x] Line protocol over USB CDC, with polled status and diagnostics on the same stream
