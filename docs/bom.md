# Gardener bill of materials

Every part the rig is built from as it stands, plus the parts the firmware already assumes
exist. This is an engineering BOM: no line has a vendor, an order code or a price, because
none is recorded in this repository, so it says what each part is and which decision needs it
rather than where to buy it.

Wiring detail — pin assignments, gate networks, the battery divider — lives in
[README.md § Wiring](../README.md#wiring) and
[core/Config.h](../libraries/VacuumCore/src/core/Config.h); a part's own numbers, which change with
the part rather than with the board, live with its driver in
[hal/impl/](../libraries/VacuumCore/src/hal/impl). Where a line is enforced or measured by
firmware, the constant is named so the two can be changed together.

Anything the code depends on but nobody has specified a value for is in
[§ 5 Open items](#5-open-items) instead of being guessed at here.

The vendor half — the modules a particular build was ordered from, the rails they sit on and the
traps in wiring them — is [prototype-build.md](./prototype-build.md). It describes one build rather
than the machine, which is why it carries prices and this does not.

## 1. Electronics

| # | Item | Qty | Part / spec | Firmware's view |
| --- | --- | --- | --- | --- |
| 1 | MCU board | 1 | ESP32-S3, developed against an ESP32-S3 Super Mini | `esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc`, pinned in [sketch.yaml](../firmware/gardener/sketch.yaml) and [scripts/arduino.ps1](../scripts/arduino.ps1). The on-board RGB LED on GPIO48 is unused. |
| 2 | Pressure sensor | 1 | Adafruit MPRLS ported pressure breakout, 0–172.369 kPa (0–25 PSI) absolute, I2C `0x18` | `kMinKpa` / `kMaxKpa`, polled every `kPollMs` at `kI2cAddr`, in [hal/impl/MprlsSensor.cpp](../libraries/VacuumCore/src/hal/impl/MprlsSensor.cpp). The sensor, not the pump, sets the measurable range. |
| 3 | Display | 1 | Monochrome 1.3" 128x64 OLED module, SH1106, 7-pin SPI breakout. A 128x32 module on the same 7-pin SPI interface is an accepted alternative | Not switched yet: [App::attachPanel()](../libraries/VacuumCore/src/app/App.cpp) still attaches an I2C panel and [core/Config.h](../libraries/VacuumCore/src/core/Config.h) has no SPI pins — see [§ 5](#5-open-items). A readout only: the host draws it over the serial line. |
| 4 | Buttons | 1 | 4-switch module with integrated pull-ups, active low on GPIO1/2/10/11 | None. They are wired and named (`cfg::BTN_*`) for a future local override and nothing reads them; the pull-ups are on the module, so the firmware configures none of its own. |

The pressure sensor is the only I2C peripheral: MPRLS on `cfg::I2C_FREQ_HZ` (400 kHz) at GPIO8/9,
which is why it is a STEMMA QT part — one cable and no soldering. The display (item 3) is a 7-pin
SPI module, so it no longer shares that bus; the firmware has not moved with it yet, see
[§ 5](#5-open-items).

## 2. Pump, valve and drive

| # | Item | Qty | Part / spec | Notes |
| --- | --- | --- | --- | --- |
| 5 | Vacuum pump | 1 | JQB2438 DC vacuum pump, 12 V, brushed | The source of the `PUMP_MAX_SUCTION_KPA` (60 kPa) figure. `PUMP_DUTY_MAX_PERCENT` and `PUMP_RAMP_PERCENT_PER_SEC` are its only protections — the device never ends a run on its own. |
| 6 | Vent valve | 1 | 12 V solenoid, **normally open** (default) or **normally closed** | The firmware picks: `NormallyOpenValve` or `NormallyClosedValve` in `app/App.h`. Energise-to-close is the safe default — a brownout, a watchdog reset or a cut wire vents the chamber. A normally closed part holds the vacuum through a power loss instead: it is the only hardware guarantee on the rig, and it does not survive that swap. |
| 7 | MOSFET | 2 | Logic level N-channel, AO3400 / IRLZ44N class | One per load. Must be logic level: the S3 gates at 3.3 V. Never drive either load from a GPIO. |
| 8 | Gate resistor | 2 | 100R, one per MOSFET | Series with the gate, to the GPIO. |
| 9 | Gate pull-down resistor | 2 | Value not fixed | Keeps the gate defined while the S3 is in reset. See [§ 5](#5-open-items). |
| 10 | Flyback diode | 2 | One per inductive load | Across the pump terminals and across the valve coil, cathode to +12 V. |
| 11 | Bulk capacitor | 1 | 470–1000 µF across the pump supply | Absorbs the pump's inrush and switching current so the 12 V rail does not sag when the loop steps the duty. |

Both loads sit on the low side of a 12 V supply with the S3 grounds tied to it. The pump and
valve are separate channels (GPIO6 PWM, GPIO5 on/off) because the control loop needs them to be:
the pump can only pull down, so the valve is the only way back up.

## 3. Power

| # | Item | Qty | Part / spec | Notes |
| --- | --- | --- | --- | --- |
| 12 | Battery | 1 | 1S Li-ion pack, 3.2–4.2 V | The gauge input: mapped linearly between `BATTERY_EMPTY_VOLTS` and `BATTERY_FULL_VOLTS`. Whether it is also what feeds the board's regulator is not stated anywhere — see [§ 5](#5-open-items). |
| 13 | Divider resistor | 2 | 100k each, pack to GPIO4 and GPIO4 to GND | `cfg::BATTERY_DIVIDER_RATIO` (2.0) assumes both are equal; `BATTERY_CALIBRATION` trims the ratio against a meter because the ESP32 ADC is not lab grade. |
| 14 | 12 V supply | 1 | For the pump and the valve | The pump's own supply, not the logic rail. See [§ 5](#5-open-items) for how it relates to item 12. |

GPIO4 carries the sense line because it is the one ADC1 pin that is neither a button nor a
strapping pin — a pressed button would pull the sense line to ground.

## 4. Wiring and plumbing

| # | Item | Qty | Part / spec | Notes |
| --- | --- | --- | --- | --- |
| 15 | USB-C cable | 1 | Data capable | Power, upload and the control interface, all on the one socket: `CDCOnBoot=cdc` is what puts the protocol there. |
| 16 | STEMMA QT / Qwiic cable | 1 | For the pressure sensor | The display is SPI (item 3) and does not use the I2C bus. |
| 17 | Chamber and tubing | 1 | Vacuum rated | The load the whole thing exists to pull down. |
| 18 | Catch pot and filter | 1 | Rated, inline | Between the chamber and the pump. Pumped liquids and vapours are the failure mode that destroys hardware, not the vacuum. |

## 5. Open items

The design implies these; nothing in the repository states them, so they are not in the tables
above:

* **The rails** (items 12 and 14): where the 12 V pump and valve supply comes from, whether the
  S3 runs from the 1S pack or its own supply, and how the two relate. The valve's hold current
  is the cost that makes a 1S cell unable to sustain an hour of solenoid duty, which is why
  [README.md](../README.md#vent-valve) suggests a latching or low-power valve for battery
  operation. Whether that is a boost converter, a second pack or a bench supply is undecided.
* **Gate pull-down value** (item 9) and the **flyback diode part** (item 10) beyond "one per
  load, cathode to +12 V".
* **MOSFET part number** (item 7) — the repository records a class, not a part.
* **Pump supply bulk capacitance** (item 11) within the 470–1000 µF range the wiring notes give.
* **Vent valve part number** (item 6), given the normally-open default and hold-current constraints.
* **Charging and protection** for the Li-ion pack (item 12). Nothing acts on a low battery: the
  reading is reported and drawn, and that is all.
* **Chamber, tubing and catch pot ratings** (items 17–18) — the firmware's limits are software
  limits, so these are what make the rig safe, and they are not spec'd here.
* **Display interface** (item 3): the panel is a 7-pin SPI module, but the firmware still attaches
  an I2C SH1106 and [core/Config.h](../libraries/VacuumCore/src/core/Config.h) has no SPI pins.
  Moving it needs a map for SCK, MOSI, CS, DC and RES; the S3's SPI defaults are not all free,
  because GPIO10/11 are `BTN_OK`/`BTN_BACK`. The free pins README lists (7, 12–18) are the
  candidates.
* **Display size** (item 3 alternative): the 128x32 module is a different geometry, so
  `panel::HEIGHT` in
  [hal/impl/AdafruitDisplay.h](../libraries/VacuumCore/src/hal/impl/AdafruitDisplay.h) and anything
  the host lays out would change with it — and 128x32 modules are usually not SH1106, which would
  mean an adapter built on a different Adafruit library than the `Adafruit_SH110X` one in use today.
