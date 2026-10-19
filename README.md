# Gardener

An ESP32-S3 vacuum pump controller. It pulls a chamber to a target depth below ambient, holds it
for a timer, then vents it — driven over a serial line by a controller at the other end.

* **MCU**: ESP32-S3 (developed against an ESP32-S3 Super Mini)
* **Interface**: USB CDC, one line oriented protocol ([docs/serial-protocol.md](./docs/serial-protocol.md))
* **Pressure sensor**: Adafruit MPRLS Ported Pressure Sensor Breakout (0–172 kPa / 0–25 PSI absolute, I2C)
* **Pump**: JQB2438 DC vacuum pump through a low side MOSFET, ~60 kPa of suction
* **Vent valve**: solenoid on a low side MOSFET — normally open by default, normally closed supported
* **Display**: Adafruit Monochrome 1.3" 128x64 OLED (SH1106, STEMMA QT / Qwiic)
* **Battery**: 1S Li-ion pack sensed through a divider on GPIO4

The parts, what they cost and where to buy them, are in
[docs/poc-parts.md](./docs/poc-parts.md).

There is almost no local control surface: the display is a readout and the controller decides
everything, with one exception — holding Back for 3 s stops and vents. What the device owns is
what has to happen when nobody is watching — pulling to the target and venting the chamber through
one path on stop, abort, link loss, fault, or that hold.

## Units

**kPa is the primary unit everywhere** — firmware and replies. The pump reaches about 60 kPa of
suction below ambient, so kPa keeps the numbers short and the resolution meaningful.

| Term | Meaning |
| --- | --- |
| `1 kPa` | 10 hPa = 10 mbar = 0.145 PSI = 7.5 mmHg |
| `kpa` | vacuum depth in kPa below ambient: 0 at rest, rising as the chamber is pumped down |
| `target` | depth below the same reference the loop pulls to, so a target and a reading are on one ruler |
| `floor` | deepest target the pump can reach, `PUMP_MAX_SUCTION_KPA` below ambient |

The ambient reference belongs to the sensor: the MPRLS is absolute, so it takes one from its first
reading when it comes up and reports depth below it. Nothing on the wire sets or restates a
reference, so the rig needs to be at rest when the device boots.

## Project layout

| Path | Purpose |
| --- | --- |
| [firmware/gardener/](./firmware/gardener) | The Arduino sketch (four lines) |
| [libraries/VacuumCore/](./libraries/VacuumCore) | The firmware, built as a local library |
| [scripts/arduino.ps1](./scripts/arduino.ps1) | Build, upload, data upload, monitor and library install wrapper |
| [docs/serial-protocol.md](./docs/serial-protocol.md) | The protocol: port, grammar, resources and their fields, error items, the panel, states, modes, rates |
| [docs/poc-parts.md](./docs/poc-parts.md) | The parts list: every part, what to buy, and what it costs |

### Architecture

```
core/   shared foundations: the constants the firmware owns — the board's pin map and the
        machine's policy — plus math
hal/    what the instrument needs from its hardware: an abstract class per part — Display,
        PressureSensor, Pump, VentValve, Battery, ButtonPanel. The parts themselves are in
        hal/impl/, which is the only place a vendor library is included
app/    the instrument: the vacuum system that commands them, the build function that owns the
        parts, the application loop, the display overlay and the serial interface
```

Dependencies only ever point downwards. Nothing checks it — keep it that way by reading:

| Layer | May include |
| --- | --- |
| `core/` | `core/` only |
| `hal/` | `core/` |
| `app/` | everything; nothing includes `app/` |

Three rules make that hold:

* **No service locator.** Nothing reaches for a global to find what it needs: a class is handed
  what it uses at construction, or owns it outright. `buildAppContext()` in
  [app/Build.cpp](./libraries/VacuumCore/src/app/Build.cpp) is the only place that knows the whole
  graph, and every constructor still requires every dependency, so a missing dependency is a
  compile error.
* **No shared `util`.** A helper lives with its single owner. `core/` is visible everywhere,
  so anything parked there is public API whether or not that was the intent —
  [core/Config.h](./libraries/VacuumCore/src/core/Config.h) is the one deliberate exception,
  because the board's wiring is the same single truth for every layer that names it.
* **Reads go through the owner.**
  [Build.cpp](./libraries/VacuumCore/src/app/Build.cpp) builds and owns the parts and hands out an
  [AppContext](./libraries/VacuumCore/src/app/AppContext.h) of interfaces.
  [App](./libraries/VacuumCore/src/app/App.h) commands those interfaces, and `SerialLink` formats
  a reply by reading them, so there is no second copy of the machine to rebuild each loop, and a
  driver still does not know that a display or a controller exists.

`hal/` never sees `app/`, which is what keeps a driver independent of the instrument built on top
of it: a part reports what it is doing as values, and nothing in `hal/` knows that a serial line or
a control loop exists. The vacuum system is the other side of that line —
[app/VacuumSystem.h](./libraries/VacuumCore/src/app/VacuumSystem.h) is handed the sensor, the pump
and the valve and holds the loop that commands them, so the control law reads in one place and the
pins and the buses are named in another. Each of those is an interface, and the part that satisfies it
is in `hal/impl/`. A vendor header appears only where a part is named: `<Adafruit_MPRLS.h>` in the
sensor's own driver, and `<Adafruit_SH110X.h>` in
[app/Build.cpp](./libraries/VacuumCore/src/app/Build.cpp), which picks the panel driver and hands it
to the generic adapter. That file is also where the numbers in
[core/Config.h](./libraries/VacuumCore/src/core/Config.h) become a fitted rig: a driver is given its
pins rather than reading them.

## Wiring

### Pin map

| Signal | GPIO | Notes |
| --- | --- | --- |
| I2C sensor SDA | 8 | MPRLS, on `Wire` (bus 0; ESP32-S3 default pin) |
| I2C sensor SCL | 9 | same bus |
| I2C panel SDA | 17 | OLED, on `Wire1` (bus 1) |
| I2C panel SCL | 18 | same bus |
| Pump PWM | 6 | to the MOSFET gate |
| Vent valve | 5 | MOSFET gate; **energise to close** for the default normally open part |
| Battery sense | 4 | 1S Li-ion through a ÷2 divider (ADC1 channel 3) |
| Button UP | 1 | active low, read on a change interrupt |
| Button DOWN | 2 | active low, read on a change interrupt |
| Button OK | 10 | active low, read on a change interrupt |
| Button BACK | 11 | active low, read on a change interrupt |
| USB D-/D+ | 19/20 | native USB: this is the control interface |
| UART0 | 43/44 | alternative control interface, 115200 |
| RGB LED | 48 | on board, not wired to anything |

Pins 7 and 12–16 are free. The two I2C parts are on separate buses — the sensor on `Wire`, the
panel on `Wire1` — so neither driver can stall the other. The four button pins are wired, named in
`core/Config.h` and read
through [hal/impl/Buttons.h](./libraries/VacuumCore/src/hal/impl/Buttons.h), which arms them on
change interrupts, settles the debounce on each read, and classifies a press as short or long when
it is released; no key is assigned an action yet. The battery sense is on GPIO4 because it is the
one ADC1 pin that is not a button — a pressed button would pull the sense line to ground. Nothing in the firmware touches the pins that have to be left
alone at reset (strapping 0, 3, 45, 46) or the USB pins (19/20), and on a Super Mini GPIO0 is only
the BOOT button. I2C addresses: MPRLS `0x18`, OLED `0x3C`. The pin map and the harness constants
are in [core/Config.h](./libraries/VacuumCore/src/core/Config.h); a part's own numbers — the panel's
geometry, the sensor's range, the PWM channel's setup — sit with its driver in
[hal/impl/](./libraries/VacuumCore/src/hal/impl).

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

**The valve is normally open by default.** De-energised means open, which means the chamber
vents — so *running a vacuum means energising the coil to close the valve*. That makes the
fail-safe physical rather than logical: a crash, a watchdog reset, a brownout or a cut wire
all de-energise the coil and the chamber vents whether or not the firmware noticed. The part
is named in [app/Build.cpp](./libraries/VacuumCore/src/app/Build.cpp): `NormallyOpenValve` is the safe
default, and `NormallyClosedValve` is the alternative for holding vacuum through a power loss —
it inverts the relationship, so de-energised means *sealed* and the fail-safe above is gone.
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

A normally open solenoid holds current for as long as it is closed, so **the valve, not the pump,
is the dominant battery cost on a long hold** — a 1S cell will not last an hour of solenoid duty.
A normally closed part pays that cost at rest instead, because holding the valve open to keep the
chamber vented is what needs current. Prefer a latching or low-power valve if a long hold has to
run on battery.

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
`arduino: upload`, `arduino: upload data`, `arduino: monitor`, `arduino: install libraries`).

**The board options matter**: the build pins
`esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PartitionScheme=default`, which is
what maps `Serial` to the chip's native USB CDC and fixes the partition table the data upload
writes. Build without `CDCOnBoot` and the whole interface goes to UART0 on GPIO43/44 instead, so a
USB cable sees nothing at all. It is pinned in `firmware/gardener/sketch.yaml`,
`scripts/arduino.ps1` and `.vscode/arduino.json`.

The board's wiring and the machine's policy tunables both live in
[core/Config.h](./libraries/VacuumCore/src/core/Config.h); a part's own numbers live with its
driver in `hal/impl/`.

## Using the device

### Display

The panel is a peripheral: the device drives it, and the wire reports only its geometry, `w` and
`h`, so a host can lay out against the real size instead of assuming it.

```
  PULL  vv                    ▐███▐         <- the state and its icon, the status column
  12.3 kPa                                  <- how deep the chamber is, 18 pt
  TGT 30.0                                  <- what the loop is pulling to
```

The boot splash is the first frame; after a second and a half it gives way to that readout,
repainted four times a second so the battery blink keeps moving. Down the right edge is the
device's status: the battery, then a plug for the serial link and the Bluetooth rune beside it.
An icon is drawn only while its connection is up. Nothing on the serial line draws into the
buffer or changes the panel's configuration.

Boot is safe: pump off, valve open, `IDLE`, the splash on the panel. Nothing about a run is
remembered across a reset; the display unit is the one setting that is, from LittleFS. The screen
follows the link: the readout while a controller is attached, and the plain mark while none is.

### Front panel keys

The four keys are read on change interrupts and debounced. A press is classified when the key comes
up: under 600 ms is `short`, at or over it is `long`. There is no separate event for a longer hold —
a page that wants one watches `heldMs()` while the key is down and picks its own mark. A key held at
boot is not a press. The loop logs the gesture (`btn: ok short`), and holding Back for 3 s stops the
pump and vents whatever the link is doing; that rule is hard-coded in `App`.

### Files on the device

The build reserves the core's `default` 4 MB partition table, which leaves a 1.5 MB `spiffs`
partition at `0x290000`. The firmware mounts it as LittleFS at boot and formats it if there is no
filesystem yet. Everything under `firmware/gardener/data/` is packed into an image and written to
that partition over the same USB cable:

```powershell
powershell -File scripts/arduino.ps1 -Task data -Port COM5
```

The task creates the folder if it is missing, builds the image with the core's `mklittlefs`, writes
it with `esptool` at the offset the pinned partition table names, and resets the device so it
mounts the new filesystem. It replaces the whole filesystem, so a file the device wrote itself is
gone unless the data folder carries a copy. There is no per-file command on the serial line yet:
this is a whole-image upload.

The display unit is stored in `/settings.ini` and restored at boot:

```ini
[device]
unit=mmhg
```

So `device unit=mmhg` survives `device reboot` and a power cycle. Keys the firmware does not know
are carried through a write rather than dropped; comments and spacing are not kept.

### Over the serial line

Open the port at 115200 8N1 and send newline terminated commands. Every command produces one
`ok`/`err` line — a `help` page adds rows and an `end` line — and lines starting with `[` are
diagnostics rather than replies:

```
-> device get
<- ok proto=3 fw=0.6.0 chip=ESP32-S3 heap=279384 batt_v=3.94 batt_pct=78 log=on
-> vacuum target=40
<- ok target=40.0
-> vacuum start
<- ok result=start state=PULL
-> vacuum get
<- ok kpa=40.07 target=40.0 floor=60.0 duty=72.0 state=HOLD batt=78 sensor=1 fault=none up=123456
-> vacuum stop
<- ok result=stop state=VENT
```

That last pair is the whole run model in miniature: the device reaches the target, reports
`HOLD`, and waits. The controller times the hold and sends `vacuum stop` when it is done — the
device has no duration of its own and will hold until told, and no command that takes one.

The full grammar, resource tables, error codes and state model are in
[docs/serial-protocol.md](./docs/serial-protocol.md). Anything that can open a serial port and
send a line works as a client: there is none in this repository at the moment, deliberately,
while the interface is being reworked.

## Vacuum control model

One class, because the state the chamber is in and the state the run is in turned out to be the
same state.

[VacuumSystem](./libraries/VacuumCore/src/app/VacuumSystem.cpp) is the vacuum system and the machine
on top of it: the target, the PID on the vacuum depth the sensor reports (all terms in kPa,
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
       vacuum start                              vacuum stop / link loss / fault / back hold
IDLE ──────────────> PULL ──(target reached)──> HOLD ───────────────────────> VENT ──(complete)──> IDLE
  ^                                                 │                           │
  │                                                 │                           v
  └───────────────── vacuum reset ──────────────────┴─────────────────────── FAULT
```

* The controller owns how long a hold lasts. The device goes to `HOLD` when its own deadband
  says the target has been reached, and that is all `HOLD` now means: it is the cue for the
  controller to start timing, and the run continues until the controller sends `vacuum stop`.
* Nothing in the device ends a run on its own — no timer, timeout or maximum run time, so an
  unattended hold holds until someone stops it. The one local action is a deliberate Back hold for
  3 s: it stops and vents, even while a controller is attached.
* The countdown is the host's job: the host holds the timer, and the device's own screen shows
  only the two pressures, the state's icon and the battery.
* The valve is closed through `PULL` and `HOLD`, and open in `IDLE` and `VENT`. Whether "closed"
  means energised or de-energised is the fitted part's, and is named in `app/Build.cpp`.
* Venting ends when the pressure is within 2 kPa of ambient, or after 30 s, whichever comes
  first — a leaky rig takes longer, and that is logged rather than treated as an error.
* Faults latch. Only `vacuum reset` clears one; the device never retries on its own.
* Disconnecting the controller stops the run and vents. That is the one automatic ending left,
  and it does not help with a controller that hangs with the port open.

Gains and the deadband live in `core/Config.h` (`PID_KP`, `PID_KI`, `PID_KD`,
`CONTROL_DEADBAND_KPA`) and are starting points, not tuned values — tune them for your chamber
volume and pump.

Protections, all of which act without consulting the controller:

* duty is hard capped at `PUMP_DUTY_MAX_PERCENT`
* a slew rate limit avoids step changes into the diaphragm pump
* if the pressure sensor stops answering mid-run, the pump is cut and the state latches to
  `FAULT` (clear with `vacuum reset`)
* targets deeper than `floor` only log a warning and pin the duty
* holding Back for 3 s stops the pump and vents, whatever the controller is doing

Those five are the whole list. A target the pump cannot reach pins the duty at the cap and
nothing stops it, so a controller that wants a ceiling has to keep it: read `floor` from
`vacuum get` and keep targets no deeper than it.

## Safety

Vacuum work is not benign: imploding chambers, collapsing tubing and pumped liquids/vapours
can hurt people and destroy hardware. Keep a rated catch pot and filter in line, and remember
that the firmware's limits are software limits only.

The one hardware guarantee is the valve: it is normally open, so losing power or losing
control releases the chamber. That guarantee belongs to `NormallyOpenValve` in `app/Build.cpp` —
fitting a `NormallyClosedValve` instead removes it, because a dead normally closed valve holds
the vacuum. Do not expect the fail-safe to survive that swap.

## Roadmap

* **No host client in this build.** The serial client that used to live in `tools/` was deleted
  while the interface is in churn — it is in git at commit `85f3cbb` if it is wanted back.
* The line grammar is the whole interface, so anything that can carry newline terminated
  text — another microcontroller, a bridge, a terminal — can drive the device as it stands.

## Status

* [x] ESP32-S3 project skeleton, build and upload flow
* [x] Layered core/hal/app layout, with the dependency direction kept by hand
* [x] MPRLS driver with median + EMA filtering, capturing its own ambient reference
* [x] PWM pump driver with ramp and duty cap
* [x] PID vacuum controller with a reachable-floor check
* [x] Run model: target, vent valve and link-loss release, with the duration owned by the controller
* [x] 1S Li-ion battery gauge
* [x] Panel: a `Display` interface, an Adafruit adapter, and an overlay for the battery, the run state and the connections
* [x] Device screen: a splash and a live readout in GFX fonts, with an icon per run state
* [x] Line protocol over USB CDC: resource-first commands, a polled readout and diagnostics on the same stream
* [x] Front panel keys: four active-low buttons read on change interrupts, debounced on read,
  classifying short and long presses, exposing the running hold, and stopping on a 3 s Back hold
* [x] HX710B 0–40 kPa gauge driver in `hal/impl/`, available to a build that fits that module
* [x] LittleFS storage: a mount at boot, a persistent display unit, and a data-folder upload task
