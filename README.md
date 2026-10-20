# Gardener

An ESP32-S3 vacuum pump controller. It pulls a chamber to a target depth below ambient, holds it
for a timer, then vents it — driven over a serial line or Bluetooth by a controller at the other
end.

* **MCU**: ESP32-S3 (developed against an ESP32-S3 Super Mini)
* **Interface**: USB CDC, one line oriented protocol ([spec](./docs/specs/serial-protocol.md)), and a Bluetooth LE GATT table
* **Pressure sensor**: CFSensor XGZP6847A100KPGN33 (-100–0 kPa gauge, 0.2–2.7 V analog)
* **Pump**: JQB2438 DC vacuum pump through a low side MOSFET, ~60 kPa of suction
* **Vent valve**: solenoid on a low side MOSFET — normally open by default, normally closed supported
* **Display**: Adafruit Monochrome 1.3" 128x64 OLED (SH1106, STEMMA QT / Qwiic)
* **Battery**: 1S Li-ion pack sensed through a divider on GPIO4
* **Buzzer**: passive piezo on GPIO12, one LEDC channel, timed tones and note sequences

The parts, what they cost and where to buy them, are in
[docs/guides/poc-parts.md](./docs/guides/poc-parts.md).

The controller decides everything: the display is a readout, and the one local control is holding
Back for 3 s, which stops and vents and keeps the valve open while Back is held — an OK pressed
during that hold gives the panel back to the readout. What the device owns is what has to happen
when nobody is watching — pulling to the target and venting the chamber through one path on stop,
abort, link loss, fault, or that hold.

## Units

kPa is the primary unit everywhere, firmware and replies. The pump reaches about 60 kPa of suction
below ambient, so kPa keeps the numbers short and the resolution meaningful: 1 kPa is 10 hPa,
10 mbar, 0.145 PSI or 7.5 mmHg.

| Term | Meaning |
| --- | --- |
| `kpa` | vacuum depth in kPa below ambient: 0 at rest, rising as the chamber is pumped down |
| `target` | depth below the same reference the control loop pulls to, so a target and a reading are on one ruler |
| `floor` | deepest target the pump can reach, `PUMP_MAX_SUCTION_KPA` below ambient |

The ambient reference belongs to the sensor: the part is a gauge sensor with an analog output, and
its driver takes a reference from its first reading when it comes up. Nothing on the wire sets or
restates a reference, so the rig needs to be at rest when the device boots.

## Project layout

| Path | Purpose |
| --- | --- |
| [firmware/gardener/](./firmware/gardener) | The Arduino sketch (four lines) |
| [libraries/VacuumCore/](./libraries/VacuumCore) | The firmware, built as a local library |
| [libraries/RLEBitmap/](./libraries/RLEBitmap) | The run-length bitmap decoder the panel's images use (vendored) |
| [scripts/arduino.py](./scripts/arduino.py) | Build, upload, data upload, monitor and library install wrapper |
| [scripts/asset.py](./scripts/asset.py) | Converts a BMP into an RLE image for the panel |
| [docs/specs/](./docs/specs) | The contracts: serial protocol, Bluetooth, vacuum control, gestures, assets and routines |
| [docs/guides/](./docs/guides) | The parts list and build guide |

```
core/   the constants the firmware owns — the board's pin map and the machine's policy — plus math
hal/    an abstract class per part, and its implementation in hal/impl/
app/    the loop, the vacuum system, the build function, storage, routines, serial/ and ble/
```

Dependencies only ever point downwards. Nothing checks it — keep it that way by reading:

| Layer | May include |
| --- | --- |
| `core/` | `core/` only |
| `hal/` | `core/` |
| `app/` | everything; nothing includes `app/` |

[core/Config.h](./libraries/VacuumCore/src/core/Config.h) is the one deliberate shared header: the
board's wiring is the same single truth for every layer that names it. A part's own numbers —
the panel's geometry, the sensor's range, the PWM channel — sit with its driver in `hal/impl/`,
which is the only place a vendor library is included. `buildAppContext()` in
[app/Build.cpp](./libraries/VacuumCore/src/app/Build.cpp) builds and owns the parts and hands out an
[AppContext](./libraries/VacuumCore/src/app/AppContext.h) of interfaces; everyone else is handed
what they use, or reads it from the owner, so there is no service locator and no second copy of the
machine. `hal/` never sees `app/`, which is what keeps a driver independent of the instrument.

## Wiring

### Pin map

| Signal | GPIO | Notes |
| --- | --- | --- |
| Sensor OUT | 7 | XGZP6847A analog output, ADC1 channel 6 |
| I2C panel SDA | 17 | OLED, on `Wire1` (bus 1) |
| I2C panel SCL | 18 | same bus |
| Pump PWM | 6 | to the MOSFET gate |
| Vent valve | 5 | MOSFET gate; **energise to close** for the default normally open part |
| Battery sense | 4 | 1S Li-ion through a ÷2 divider (ADC1 channel 3) |
| Button UP | 1 | active low, read on a change interrupt |
| Button DOWN | 2 | active low, read on a change interrupt |
| Button OK | 10 | active low, read on a change interrupt |
| Button BACK | 11 | active low, read on a change interrupt |
| Piezo | 12 | passive element, one LEDC channel; the 5-way module's fifth key is not used |
| USB D-/D+ | 19/20 | native USB: this is the control interface |
| UART0 | 43/44 | alternative control interface, 115200 |
| RGB LED | 48 | on board, not wired to anything |

Pins 8, 9 and 13–16 are free, and the firmware leaves the strapping pins (0, 3, 45, 46) and the USB
pins alone. The panel is the only I2C part, at `0x3C`; the sensor is an analog voltage on ADC1, so
`Wire` is unused. The battery sense sits on GPIO4 because it is the ADC1 pin no button took — a
pressed button would pull the sense line to ground.

### Pressure sensor

```
XGZP6847A   1  N/C
            2  VDD ── 3V3
            3  GND ── GND
            4  VDD ── 3V3
            5  OUT ──┬── GPIO7 (ADC1 channel 6)
                     └── 10k ── GND
            6  GND ── GND

            100 nF across VDD and GND, at the part
```

At 3.3 V the output is 0.2 V at -100 kPa and 2.7 V at 0 kPa, so the depth is `(2.7 - V) / 0.025`
kPa. The output is ratiometric to its supply, so keep the part on the MCU's `3V3` net rather than
the pump and valve load rail, and keep the pump's return current out of its ground. The 10 k
pulldown makes a disconnected sensor read 0 V, which the driver refuses as "no part".

The 3 mm port takes the chamber tube, and the small atmosphere hole in the side of the housing must
stay open. Light reaching the die shifts the output, so an opaque tube is the right default.

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

Use a **logic level** MOSFET: the S3 drives the gate with 3.3 V. Give the pump its own supply and
tie the grounds together. `PUMP_DUTY_MAX_PERCENT` (90 %) and `PUMP_RAMP_PERCENT_PER_SEC` in
[core/Config.h](./libraries/VacuumCore/src/core/Config.h) are the pump's only protections: the
device never ends a run of its own accord, so whoever is driving it decides when the pump stops.

### Vent valve

**The valve is normally open by default.** De-energised means open, so running a vacuum means
energising the coil to close it. That makes the fail-safe physical rather than logical: a crash, a
watchdog reset, a brownout or a cut wire all de-energise the coil and the chamber vents whether or
not the firmware noticed. `NormallyOpenValve` is the part named in
[app/Build.cpp](./libraries/VacuumCore/src/app/Build.cpp); `NormallyClosedValve` is the alternative
for holding vacuum through a power loss, and it inverts the relationship, so the fail-safe is gone.
Wire it like the pump, on GPIO5, with a flyback diode across the coil.

A normally open solenoid holds current for as long as it is closed, so **the valve, not the pump,
is the dominant battery cost on a long hold** — a 1S cell will not last an hour of solenoid duty. A
normally closed part pays that cost at rest instead. Prefer a latching or low-power valve if a long
hold has to run on battery.

### Battery

```
BAT+ ── 100k ──┬── GPIO4
               │
              100k ── GND              1S Li-ion, 3.2–4.2 V
```

The firmware averages `analogReadMilliVolts` and maps the pack linearly between
`BATTERY_EMPTY_VOLTS` (3.2 V) and `BATTERY_FULL_VOLTS` (4.2 V). The ESP32 ADC is not lab grade —
expect ±3–5 % — so `BATTERY_CALIBRATION` trims the divider against a meter. A reading below 1 V is
"no pack fitted", and nothing acts on a low battery: the icon and the `batt` key are the whole
story.

### Buzzer

A passive piezo on GPIO12, driven directly from one LEDC channel at 50 % duty; a series resistor
sets the volume, no transistor needed:

```
ESP32 GPIO12 ── 100R ── piezo ── GND
```

The firmware offers 100 Hz–10 kHz. The wire uses `buzzer tone 2000 500` and
`buzzer play a4:1000 c6:500`; a routine uses `buzzer.tone(2000, 500)` and
`buzzer.play("a4:1000 c6:500")`. A tone with a 0 ms duration runs until it is stopped. GPIO12 is
the 5-way module's fifth key on the POC rig; a board that wants that key back has to move the
buzzer.

The device also ships named jingles in `/modules/jingles.ini`. A routine plays the one selected on
the System page with `buzzer.play(jingles.current())`, or a named one by identifier with
`buzzer.play(jingles.notes("forelle"))`; `jingles.ids()`, `jingles.title()` and `jingles.label()`
are how it finds them.

## Building

Requires `arduino-cli` (or the Arduino IDE 2 with the ESP32 core installed), Python 3 for the
wrapper scripts, and a one time library install:

```powershell
arduino-cli core install esp32:esp32
python scripts/arduino.py --task libs
python scripts/arduino.py --task build
python scripts/arduino.py --task upload --port COM5
python scripts/arduino.py --task monitor --port COM5
```

The build uses `--libraries libraries`, so the project local `libraries/VacuumCore` is used instead
of the global sketchbook copy. In VS Code the same commands are tasks (`arduino: build`,
`arduino: upload`, `arduino: upload data`, `arduino: monitor`, `arduino: install libraries`).

**The board options matter**: the build pins
`esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PartitionScheme=default`, which maps
`Serial` to the chip's native USB CDC and fixes the partition table the data upload writes. Build
without `CDCOnBoot` and the whole interface goes to UART0 instead, so a USB cable sees nothing. It
is pinned in `firmware/gardener/sketch.yaml`, `scripts/arduino.py` and `.vscode/arduino.json`.

The radio is NimBLE-Arduino 2.5.1, which `--task libs` pins, with the compile options that cut it to
peripheral only, no central, no scanning, one connection. With the radio in, the build is 863 kB of
flash and 30.5 kB of static RAM, against 638 kB and 20.0 kB for the serial-only firmware.

## Using the device

### Over the serial line

Open the port at 115200 8N1 and send newline terminated commands. Connecting is not controlling: a
link can read as soon as it connects, but everything that changes the machine answers
`err not_controlling` until it sends `control acquire`. `control release` is itself a stop and vent.

```
-> control acquire
<- ok control=owned
-> vacuum target 40
<- ok target=40.0
-> vacuum start
<- ok state=PULL
-> vacuum get
<- ok kpa=40.07 target=40.0 floor=60.0 duty=72.0 state=HOLD sensor=1 fault=none up=123456
```

The full grammar, domain commands and error codes are in
[docs/specs/serial-protocol.md](./docs/specs/serial-protocol.md), and the state model is in
[docs/specs/vacuum-control.md](./docs/specs/vacuum-control.md). Anything that can send a line
works as a client — there is none in this repository at the moment, deliberately, while the
interface is being reworked.

### Over Bluetooth

The radio is a second contract rather than the line protocol over a pipe, because a phone is a GATT
client and not a terminal. The device advertises as `gardener`, reads are open without pairing, and
writing `1` to `control` acquires the machine. The characteristics, the command opcodes, the
pairing rules and the notify rates are in [docs/specs/bluetooth.md](./docs/specs/bluetooth.md).

### Display and keys

The panel is a peripheral: the device drives it, and the wire reports only its geometry. It shows
which link holds control — a DE-9 face-on for serial, the Bluetooth rune for a radio — with the run
state, the battery and the live connections down the right edge. The disconnected page is the boot
screen, and boot is safe: pump off, valve open, `IDLE`.

The four keys are read on change interrupts and debounced; a press is `short`, `long` or `double`.
Holding Back for 3 s stops and vents, keeping the valve open while Back is held, and an OK pressed
during that hold gives the panel back to the readout; Back+OK for 5 s releases the chamber and
restarts. That is the whole of the device's own key handling — the rest belongs to the page showing.
See [docs/specs/gestures.md](./docs/specs/gestures.md).

### Files on the device

The build reserves the core's `default` 4 MB partition table, which leaves a 1.5 MB `spiffs`
partition at `0x290000`. The firmware mounts it as LittleFS at boot and formats it if there is no
filesystem yet. Everything under `firmware/gardener/data/` is packed into an image and written over
the same USB cable:

```powershell
python scripts/arduino.py --task data --port COM5
```

That replaces the whole filesystem, so a file the device wrote itself is gone unless the data
folder carries a copy. Individual files can be changed without replacing the image through the
`fs` domain: `list`, `read`, `write`, `mkdir` and `delete`.

The display unit, the default target, the radio preference and the selected jingle are stored in
`/settings.ini` and restored at boot, so `device set unit mmhg` survives a power cycle:

```ini
[device]
unit=mmhg
target=12.5
bluetooth=on
jingle=poacher
```

Keys the firmware does not know are carried through a write; comments and spacing are not kept. A
routine keeps its own values the same way, in `/routines/<name>.ini`. Images are files too: the
mark both pages open with is `/assets/eggplant.rle`, a 1-bit bitmap converted from
`firmware/gardener/assets/` with `scripts/asset.py` ([format](./docs/specs/assets.md)).

### Routines

While no controller owns the device, the panel runs a Berry routine: every `/routines/*.be` loads at
boot and registers itself, and the one registered as `default` is the page. If none registers one,
the panel shows the device's mark.

Five ship: `default` (the readout), `browser` (the routine list), `system` (unit, default target,
radio, jingle), `interval` (pull, hold, rest, repeat) and `ramped` (a ladder of sets). A routine
reaches `vacuum`, `screen`, `device`, `buttons`, `buzzer`, `jingles`, `routines` and `store`; the
surface is in [docs/specs/ui/bindings.md](./docs/specs/ui/bindings.md) and the lifecycle in
[docs/specs/ui/routine.md](./docs/specs/ui/routine.md).

A `poll` that never returns blocks the panel loop, so returning is a convention the author keeps,
and `print` goes nowhere: `vacuum.log(text)` reaches the serial line as a `[I]` record.

## Vacuum control

`IDLE` → `PULL` → `HOLD` → `VENT` → `IDLE`, with a `FAULT` that only `vacuum reset` clears. The
pump can only pull down, so the normally open valve is the only way back up, and a target set
shallower than the chamber vents to it.

`HOLD` is the device's deadband saying the target is reached. The duration is the controller's:
the device has no timer and never ends a run on its own. Duty is capped and slew limited, an
implausible sensor reading latches `FAULT`, and a target deeper than `floor` pins the duty rather
than stopping the pump.

The states, the transition each command causes, the gains and the full list of protections are in
[docs/specs/vacuum-control.md](./docs/specs/vacuum-control.md).

## Safety

Vacuum work is not benign: imploding chambers, collapsing tubing and pumped liquids or vapours can
hurt people and destroy hardware. Keep a rated catch pot and filter in line, and remember that the
firmware's limits are software limits only.

The one hardware guarantee is the valve: normally open means losing power releases the chamber.
That belongs to `NormallyOpenValve` in `app/Build.cpp` — fitting a `NormallyClosedValve` removes it,
because a dead normally closed valve holds the vacuum.

## Roadmap

* **No host client in this build.** The serial client that used to live in `tools/` was deleted
  while the interface is in churn — it is in git at commit `85f3cbb` if it is wanted back.

## Status

* [x] ESP32-S3 project skeleton, build and upload flow
* [x] Layered core/hal/app layout, with the dependency direction kept by hand
* [x] XGZP6847A analog driver with averaging, EMA filtering and its own ambient reference
* [x] PWM pump driver with ramp and duty cap, and a PID vacuum controller with a reachable-floor check
* [x] Run model: target, vent valve and control release, with the duration owned by the controller
* [x] Explicit link control: one ownership record, a `control` domain, gated writes and the Back+OK reset
* [x] 1S Li-ion battery gauge
* [x] Panel: a `Display` interface, an Adafruit adapter, and an overlay for the battery, state and links
* [x] Line protocol over USB CDC, and Bluetooth LE as a GATT table sharing the ownership record
* [x] Front panel keys: four active-low buttons, classified short, long and double, with the reserved holds
* [x] HX710B 0–40 kPa gauge driver in `hal/impl/`, available to a build that fits that module
* [x] LittleFS storage: a mount at boot, persistent settings and a data-folder upload task
* [x] Berry routines: a vendored interpreter, a `/routines/*.be` registry and the native bindings
* [x] Passive piezo: a timed tone and a non-blocking note-sequence player on GPIO12
