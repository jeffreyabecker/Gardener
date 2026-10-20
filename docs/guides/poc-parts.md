# Gardener proof-of-concept parts

Everything the rig is built from, in one list. One 5 V rail with a 3.3 V load rail off it, one
switch module, one tube size. Bench use, dry air, attended. A 12 V variant with a bigger pump is at
the end.

**Total to buy: ≈ $143.** The sensor is the one price that is an estimate; the rest are listed.

## Parts

| # | Part | What it is | Get it |
| --- | --- | --- | --- |
| 1 | ESP32-S3 Super Mini | The MCU, 4-pack. Native USB CDC is the control interface | **$16.99** — [B0H8MV4RGZ](https://www.amazon.com/dp/B0H8MV4RGZ) |
| 2 | JQB2438 pump | Micro diaphragm vacuum pump, 3 V/3.7 V winding, 0.45 A | **$16.84** — [B0GR4K9K4P](https://www.amazon.com/dp/B0GR4K9K4P) |
| 3 | Solenoid valve | Mini 2-way **normally open**, 3.0–5.0 V, ~180 mA at 3.3 V. Ports 1 and 2 in use; port 3 capped | **$9.63** — [B0HDRX4C44](https://www.amazon.com/dp/B0HDRX4C44) |
| 4 | **XGZP6847A sensor** | CFSensor XGZP6847A100KPGN33: -100–0 kPa gauge, 3.3 V, amplified analog 0.2–2.7 V out, 6-DIP, 3 mm barbless top port. Pre-calibrated, so no bridge ADC; it takes one ADC pin rather than the I2C bus | **≈ $40** — [DigiKey](https://www.digikey.com/en/products/detail/cfsensor/XGZP6847A100KPGN33/22530823) |
| 5 | **OLED** | Generic 1.3" 128x64 SH1106, I2C `0x3C`, `VCC / GND / SDA / SCL`. On its own bus | **$6.99** — [B0HFSCMGWD](https://www.amazon.com/Display-Breadboard-Multicolored-Compatible-Raspberry/dp/B0HFSCMGWD) |
| 6 | Button module | KOOBOOK 5-way navigation switch: up, down, left, right, centre — 5 switches, 5 GPIOs. 2.0–9.0 V | **$7.29** — [B07SC1H42W](https://www.amazon.com/dp/B07SC1H42W) |
| 7 | **DRV8833 module** | Dual H-bridge, 2 × 1.5 A, logic-level inputs. Drives the pump and the valve | **$6.99** — [B0G1RMXMSF](https://www.amazon.com/dp/B0G1RMXMSF) |
| 8 | **Breadboard + jumpers** | 2 × 830-point board with a wire kit | **$9.99** — [B0H17GR4S5](https://www.amazon.com/dp/B0H17GR4S5) |
| 9 | **Bulk capacitor** | 1000 µF 25 V electrolytic, across the 3.3 V load rail | **$6.99** — [B0F8C1VMT6](https://www.amazon.com/dp/B0F8C1VMT6) |
| 10 | **Silicone tube** | 2.5 mm ID x 4 mm OD, 6 ft. Covers every port: 2.4 mm tee, 3 mm sensor port, 3–3.5 mm valve, 4 mm pump | **$5.79** — [B0DMP15ZWS](https://www.amazon.com/dp/B0DMP15ZWS) |
| 11 | **Barbed tee** | 3/32" (2.4 mm), 4-pack | **$6.49** — [B0DG2YT19V](https://www.amazon.com/dp/B0DG2YT19V) |
| 12 | **3.3 V buck module** | 5 V → 3.3 V fixed, 1.8 A. Feeds the DRV8833, so it runs both loads | **$8.99** — [B0FP2TLXYW](https://www.amazon.com/dp/B0FP2TLXYW) |

Power comes from the PC's USB port through the MCU's USB-C cable, so that cable has to be
data-capable. A 1 A slow-blow fuse on the 5 V feed is optional, not required.

One 10 k resistor and one 100 nF ceramic at the sensor are assumed to be in the drawer; they are a
few cents if they are not, and neither is in the total.

## Wiring

```
PC USB 5 V ─┬─ MCU "5V" pin ── its 3V3 pin ─┬─ XGZP6847A  VDD + GND   (OUT → GPIO7)
(via USB-C) │                               ├─ OLED       SDA + SCL   (Wire1, 17/18)
            │                               └─ buttons    5-way
            └─ 3.3 V buck ── DRV8833 VM ─┬─ channel A ── pump
                                         └─ channel B ── valve

   1000 µF across the 3.3 V load rail · one star ground
```

```
GPIO  7               ADC     XGZP6847A OUT (ADC1 channel 6)
GPIO 17 18            I2C     panel 0x3C (Wire1)
GPIO  6               pump
GPIO  5               valve
GPIO  1  2  10 11     buttons 5-way (four keys read)
GPIO  4               battery sense (reserved)
GPIO 12               buzzer (passive piezo)
GPIO  8  9 13 14 15 16   spare
```

The sensor is a bare 6-DIP: pin 1 is N/C, pins 2 and 4 are VDD, pins 3 and 6 are GND, and pin 5
is OUT. It plugs straight into the breadboard beside a 100 nF across VDD and GND and a 10 k from
OUT to GND. The pulldown is what makes an unplugged sensor read 0 V and be refused, rather than
read as full vacuum.

A passive piezo on GPIO12 is an add-on to this list; the firmware drives it directly from one
LEDC channel. GPIO12 is the 5-way module's fifth key in the original wiring, but the firmware reads
only four keys, so that key is not wired on a build that fits the buzzer.

```
   pump inlet ──────┐
   sensor port ─────┼── 3/32" tee ── 2.5 mm ID tube
   valve port 1 ────┘

   valve port 2 ──── open to air     de-energised, the chamber vents here
   valve port 3 ──── capped          energised, the chamber seals against it
```

The valve is a 3-port body used as 2-way: de-energised, ports 1 and 2 are open, so the chamber
vents to air. Energised, port 2 closes and port 1 connects to port 3 instead — which is why port 3
must be capped rather than left open.

Wire the DRV8833 as a switch, not as an H-bridge: tie each channel's `IN2` to GND and drive `IN1`.
That gives one active-high GPIO per load. Its body diodes are the flyback path, so no diodes, gate
resistors, pull-downs or screw terminals are needed.

The tube volume is the chamber. No vessel, no catch pot, no filter.

## Accepted compromises

Bench-only, to fix once the loop works.

* The 5 V rail is the PC's USB port, and the rig wants about 0.8 A with a 1.2 A surge when the pump
  starts. That sits inside a USB-C or USB 3 port but over a USB 2.0 port's 500 mA, so use a blue or
  USB-C socket rather than a plain black one. The 1000 µF is what absorbs the surge.
* The 3.3 V load rail is what puts both loads inside their ratings — the pump against a 3–3.7 V
  winding, the valve against a 3.0–5.0 V range. The cost is the buck itself, and that the DRV8833
  has one shared motor supply, so the pump and the valve cannot be given different rails.
* The valve is normally open, so its coil is energised — about 180 mA at 3.3 V — for the whole pull
  and the whole hold, and venting is free. That is the fail-safe direction: a brownout, a reset or
  a cut wire vents the chamber.
* The pump's ports are 4 mm against a 2.5 mm ID tube, so that joint is a 1.6x stretch. Silicone takes
  it and a smooth nozzle seals against the hoop force, but it is the one joint to check before
  trusting the plumbing — a torn or weeping tube there looks exactly like a leaky chamber.
* The sensor port is 3 mm, barbless plastic, and the datasheet recommends a 2.5 mm ID inlet tube.
  The tube is a 1.2x stretch there, which seals better than a 2.5 mm port, but it is still a slip
  fit: push it fully home. The atmosphere hole beside the port is the gauge reference and must stay
  open — no tape, no glue, no sleeve over it.
* The sensor shares the MCU's 3.3 V rail and ground with a brushed pump, and its output is
  ratiometric analog: the ADC measures volts, so a dip on that rail moves the reading. Keep the
  pump's return out of the sensor's ground, put 100 nF at the part and 10 k from OUT to GND, and
  keep the sensor off the 3.3 V load rail. First upgrade if that is not enough: an AMS1117-3.3 off
  the 5 V rail, for the sensor alone.
* The part's total accuracy is ±2.5 % of its 100 kPa span, about ±2.5 kPa. A 5 kPa target is
  therefore within twice the part's error and should be treated as approximate; the 20–60 kPa the
  pump actually reaches is comfortable. The part, not the ADC's resolution, is the limit.
* The datasheet warns that light reaching the die shifts the output. Use opaque tube, and do not
  leave the sensor under a lamp with its port lit.
* No chamber, so no realistic pump-down time and no leak measurement.
* The 5-way module costs five GPIOs but the firmware reads four keys. GPIO12, the fifth key's pin,
  drives the buzzer instead; the key is not wired on a build that fits the piezo.

Which of these parts move onto the alpha carrier board, and which stay modules, is a planned
board decision and not part of this build.

## Upgrading to 12 V and a bigger pump

The base build is a 5 V rig: a 3.7 V pump on a 3.3 V load rail, powered from the PC's USB port.
That is enough to prove the loop, and 60 kPa is the pump's measured reach. A 12 V pump buys depth
and flow, but it moves the pump off the DRV8833 and off the USB supply. This is a bench variant,
not a second rig: the MCU, sensor, panel, buttons, valve and firmware interface stay, and the pump,
its drive, the supply and the two bucks change.

The firmware does not care what the pump's rail is. `PwmPump` owns one LEDC channel at 20 kHz and
the low-side switch does the rest, so GPIO6 and the duty mean the same thing on either rail. What
changes is the numbers around it.

### Parts

| Part | Target | Job |
| --- | --- | --- |
| 12 V pump | Brushed diaphragm, 12 V, 1–2 A, ultimate vacuum at least -80 kPa, 4 mm barb, continuous duty | The point of the upgrade: more depth and flow than the JQB2438's ~60 kPa |
| 12 V supply | 12 V 5 A (60 W), 5.5 × 2.1 mm centre positive | Covers a 2 A pump's inrush; 3 A only if the pump runs under 1 A |
| 12 V → 5 V buck | 5 V 2 A, fixed or set and verified before connection | The MCU alone; its own 3V3 LDO still feeds sensor, panel and buttons |
| 12 V → 3.3 V buck | 3.3 V 1 A, fixed or set and verified | The valve load rail through the DRV8833; replaces the base 5 V → 3.3 V buck |
| Low-side switch | Logic-level N-channel MOSFET, IRLZ44N or IRLB8721, TO-220 | The S3 drives 3.3 V on the gate |
| Flyback diode | 1N5822 (3 A, 40 V Schottky) | The pump winding's current when the FET turns off |
| Gate parts | 100 Ω series, 10 k to ground | A defined edge, and off at reset |
| Fuse | 3 A slow-blow and an inline holder | Protects the 12 V wiring, not just the adapter |
| DC jack | 5.5 × 2.1 mm screw terminal | The supply's barrel plug to the rig |

Add about **$60–75** on top of the base build. The base pump and the 5 V → 3.3 V buck are left
over; the base 1000 µF moves to the 12 V rail, where the surge now is. The 3.3 V valve rail needs
no bulk of its own: it carries 0.18 A of DC, and the DRV8833 module has its own capacitors.

A 6 mm barb needs a short length of 4 mm ID tube and a 4 mm-to-2.4 mm reducer: the base 2.5 mm ID
tube will not stretch to 6 mm, and the tee is 3/32".

### Wiring

```
12 V PSU ── 3 A fuse ──┬── PUMP (+) ─────────── DRAIN
                       │                          │
                       │                      [ MOSFET ] ── SOURCE ── GND
                       │                          │
                       │        GPIO6 ── 100R ── GATE
                       │                  10k ── GND
                       │
                       ├── 1000 µF ── GND      at the pump
                       ├── 1N5822              across PUMP (+) and DRAIN,
                       │                       cathode to +12 V
                       │
                       ├── 12 V → 3.3 V buck ── DRV8833 VM ── valve (channel B)
                       │
                       └── 12 V → 5 V buck ── MCU "5V" pin

   one star ground: PSU -, both bucks' -, MOSFET source, DRV8833 GND, MCU GND
```

The pin map is unchanged: GPIO6 is the pump PWM and GPIO5 is the valve. The DRV8833 now drives
only the valve — `BIN1` from GPIO5, `BIN2` and both `AIN` inputs to GND — with its `VM` on the
3.3 V rail, so the coil sees the same voltage as the base build and its body diodes are still the
coil's flyback path. The pump's diode and capacitor are at the pump, not at the FET: the capacitor
is the surge reservoir and the diode is the winding's path when the FET turns off.

Two bucks, because the two loads no longer share a rail: 5 V for the MCU and 3.3 V for the valve.
Deriving the valve rail from 12 V rather than from the 5 V node is what keeps the fail-safe: when
the 12 V supply goes away, the valve rail goes with it and the normally-open valve vents the
chamber, whether or not USB is plugged in. Feed the 3.3 V buck from the 5 V node instead, and a USB
cable can hold the valve closed after the 12 V dies.

The 12 V pump current does not belong on a breadboard. Put the fuse, pump, diode, capacitor and
MOSFET on a screw-terminal barrier or soldered joints, and keep the breadboard for the logic side;
a breadboard contact is not a 2 A connector. The MOSFET's tab is at drain potential, so keep it
clear.

The supply's negative is the star ground. Join the two bucks' returns, the MOSFET source, the
DRV8833 ground and the MCU ground there, and keep the sensor's return off the pump's path. The
sensor stays on the MCU's `3V3` net, as in the base build, because its output is ratiometric.

### Firmware and tuning

[core/Config.h](../../libraries/VacuumCore/src/core/Config.h) is written for the base pump, so the
upgrade is also a constants change:

* `PUMP_MAX_SUCTION_KPA` is the fitted pump's measured reach, 60 kPa for the JQB2438. Measure the
  new pump and set it: it is the `floor` in `vacuum get` and the deepest target the loop will
  offer. Left at 60, the bigger pump looks like the old one.
* `PUMP_DUTY_MAX_PERCENT` is 90 %, which is 10.8 V on this rail. A 12 V pump is rated for 12 V, so
  the cap is a choice rather than a limit of the part. It is the pump's only software protection;
  keep it unless the datasheet allows continuous 12 V and the extra flow is needed.
* The PID gains and `CONTROL_DEADBAND_KPA` were tuned for the old pump and chamber. More flow
  reaches the target faster and overshoots the vent more easily, so retune them against the new
  rig. `PUMP_RAMP_PERCENT_PER_SEC` stays: a larger diaphragm still dislikes a step.
* Use a brushed pump. A BLDC pump with its own controller wants a steady rail, and this firmware
  PWMs the supply; low-side PWM into a BLDC controller is not a speed control.
* The sensor's range is -100 kPa, so a pump that pulls deeper saturates the reading. Check the
  pump's ultimate vacuum on its datasheet rather than the "beefier" claim; a single-stage
  diaphragm pump lands inside the sensor, a two-stage one may not.

### What the bigger pump breaks

* **The valve may not seal.** A deeper chamber puts more differential across the seat, and the
  mini 3–5 V normally-open valve is rated for some maximum. If the chamber leaks down faster than
  before, suspect the valve before the plumbing; a deeper-rated valve is the fix.
* **The tube is not a chamber at this depth.** 2.5 mm ID silicone collapses under a harder vacuum.
  Use a small rigid chamber — a jar with a bulkhead, or a short rigid tube — and keep flexible tube
  for the runs to the pump, sensor and valve. The base build used the tube's volume because the
  pump was small.
* **The tube-to-port stretch remains.** The base pump's 4 mm port already stretches 2.5 mm ID tube
  1.6x; a new pump with the same barb is no worse, and a 6 mm barb needs the reducer above.
* **The 5 V USB rail is gone.** The MCU runs from the 5 V buck. Check the Super Mini for a Schottky
  between USB VBUS and the 5V pin; if it has none, use a cable with VBUS cut or unplug USB while
  the 12 V is on. Two 5 V sources on one pin is not a fault the board protects you from.
* **The ground currents are larger.** A 2 A pump return through a shared ground moves the sensor's
  reading. The star ground and the separate sensor return in the base list matter more here, not
  less.
* **Startup surge.** A diaphragm pump's inrush is several times its running current, which is why
  the supply is 5 A for a 2 A pump. The 1000 µF covers the first few milliseconds; the adapter's
  current limit protects the adapter, not the rig.

It retires two of the base build's accepted compromises: the USB rail, with its 500 mA USB 2.0
ceiling and 1.2 A surge, and the single shared motor supply that forced the pump and the valve onto
the same 3.3 V rail. It leaves the valve, the sensor, the panel and the firmware interface alone.
The valve is still the hardware guarantee: on this build its rail comes from the 12 V supply, so
losing the supply vents the chamber. A bigger pump pulls a small chamber down faster, so use a
rated chamber, a catch pot and a filter, and keep the rig attended.
