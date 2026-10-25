# Gardener proof-of-concept parts

Two builds share the firmware, the pin map and the interface. The 5 V build proves the loop from
the PC's USB port with a small pump; the 12 V build is the same machine on a bench supply with a
deeper pump. Pick one before buying: they share most of the parts, and each build lists what it
needs.

**5 V build: ≈ $108. 12 V build: ≈ $170.** The 12 V pump, the 12 V supply and the two
bucks are estimates; the rest are listed.

## 5 V build

One 5 V rail with a 3.3 V load rail off it, one switch module, one tube size. Bench use, dry air,
attended.

### Parts

| # | Part | What it is | Get it |
| --- | --- | --- | --- |
| 1 | ESP32-S3 Super Mini | The MCU, 4-pack. Native USB CDC is the control interface | **$16.99** — [B0H8MV4RGZ](https://www.amazon.com/dp/B0H8MV4RGZ) |
| 2 | JQB2438 pump | Micro diaphragm vacuum pump, 3 V/3.7 V winding, 0.45 A | **$16.84** — [B0GR4K9K4P](https://www.amazon.com/dp/B0GR4K9K4P) |
| 3 | Solenoid valve | Mini 2-way **normally open**, 3.0–5.0 V, ~180 mA at 3.3 V. Ports 1 and 2 in use; port 3 capped | **$9.63** — [B0HDRX4C44](https://www.amazon.com/dp/B0HDRX4C44) |
| 4 | **XGZP6847A sensor** | CFSensor XGZP6847A100KPGN33: -100–0 kPa gauge, 3.3 V, amplified analog 0.2–2.7 V out, 6-DIP, 3 mm barbless top port. Pre-calibrated, so no bridge ADC; it takes one ADC pin rather than the I2C bus | **$5.25** — [DigiKey](https://www.digikey.com/en/products/detail/cfsensor/XGZP6847A100KPGN33/22530823) |
| 5 | **OLED** | Generic 1.3" 128x64 SH1106, I2C `0x3C`, `VCC / GND / SDA / SCL`. On its own bus | **$6.99** — [B0HFSCMGWD](https://www.amazon.com/Display-Breadboard-Multicolored-Compatible-Raspberry/dp/B0HFSCMGWD) |
| 6 | **Six keys + 74HC165** | Six labelled momentary switches — up, down, forward, back, okay, cancel — into an 8-bit parallel-in/serial-out shift register. Three GPIOs instead of six; the two spare register inputs are tied high | **≈ $5** — any 74HC165 and six 6 mm tactile switches |
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

### Wiring

```
PC USB 5 V ─┬─ MCU "5V" pin ── its 3V3 pin ─┬─ XGZP6847A  VDD + GND   (OUT → GPIO7)
(via USB-C) │                               ├─ OLED       SDA + SCL   (Wire1, 17/18)
            │                               └─ keys       74HC165 (SH/LD, CLK, Q7)
            └─ 3.3 V buck ── DRV8833 VM ─┬─ channel A ── pump
                                         └─ channel B ── valve

   1000 µF across the 3.3 V load rail · one star ground
```

```
GPIO  7               ADC     XGZP6847A OUT (ADC1 channel 6)
GPIO 17 18            I2C     panel 0x3C (Wire1)
GPIO  6               pump
GPIO  5               valve
GPIO 10 11  1         keys: 74HC165 SH/LD, CLK, Q7 (six keys)
GPIO  4               battery sense (reserved)
GPIO 12               buzzer (passive piezo)
GPIO  2  8  9 13 14 15 16   spare
```

The sensor is a bare 6-DIP: pin 1 is N/C, pins 2 and 4 are VDD, pins 3 and 6 are GND, and pin 5
is OUT. It plugs straight into the breadboard beside a 100 nF across VDD and GND and a 10 k from
OUT to GND. The pulldown is what makes an unplugged sensor read 0 V and be refused, rather than
read as full vacuum.

A passive piezo on GPIO12 is an add-on to this list; the firmware drives it directly from one
LEDC channel. The key register's `CLK INH` is tied low, its six key inputs are active-low with
pull-ups to 3.3 V, and its two spare inputs are tied high: an unconnected 74HC165 input floats and
reads as noise.

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

### Accepted compromises

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
* The six-key panel costs one shift register and three GPIOs, so the key count is no longer what
  limits the rig; the 74HC165 has two spare inputs before a second register is needed.

Which of these parts move onto the alpha carrier board, and which stay modules, is a planned
board decision and not part of the 5 V build.

## 12 V build

The same machine on a bench supply. The MCU, sensor, panel, buttons, valve and firmware are the
5 V build's; the pump, its drive, the supply and the two bucks are different. The firmware does
not know which pump it is driving: `PwmPump` owns one LEDC channel at 20 kHz and the low-side
switch does the rest, so GPIO6 and the duty mean the same thing on either rail.

### What differs

| Subsystem | 5 V build | 12 V build |
| --- | --- | --- |
| Supply | PC USB 5 V | 12 V 5 A adapter |
| Pump | JQB2438, 3.7 V, 0.45 A, ~60 kPa | 12 V brushed diaphragm, 1–2 A, -80 kPa |
| Pump drive | DRV8833 channel A | logic-level MOSFET + 1N5822 |
| Valve | 3–5 V normally open | same |
| Valve drive | DRV8833 channel B | DRV8833 channel B |
| Valve rail | 3.3 V from the 5 V buck | 3.3 V from a 12 V buck |
| MCU rail | USB 5 V | 12 V → 5 V buck |
| Bulk capacitor | 1000 µF on the 3.3 V load rail | 1000 µF on the 12 V pump rail |
| Sensor, panel, buttons, buzzer | — | same |

### Parts

| Part | 12 V build | Get it |
| --- | --- | --- |
| MCU | ESP32-S3 Super Mini | the 5 V build's item 1 |
| Pump | 12 V brushed diaphragm, 1–2 A, 5–10 L/min, ultimate vacuum at least -80 kPa, 4 mm barb, continuous duty | **≈ $25** |
| Pump switch | Logic-level N-channel MOSFET, IRLZ44N or IRLB8721, TO-220 | **≈ $8** — a 5-pack |
| Flyback diode | 1N5822 (3 A, 40 V Schottky) | **≈ $7** — a 10-pack |
| Gate parts | 100 Ω series, 10 k to ground | in the drawer |
| Supply | 12 V 5 A (60 W), 5.5 × 2.1 mm centre positive | **≈ $15** |
| 12 V → 5 V buck | 5 V 2 A, fixed or set and verified | **≈ $7** |
| 12 V → 3.3 V buck | 3.3 V 1 A, fixed or set and verified | **≈ $7** |
| Valve driver | DRV8833 module | the 5 V build's item 7 |
| Valve | 3–5 V normally open | the 5 V build's item 3 |
| Sensor | XGZP6847A | the 5 V build's item 4 |
| Panel | SH1106 OLED | the 5 V build's item 5 |
| Keys | six switches + 74HC165 | the 5 V build's item 6 |
| Bulk capacitor | 1000 µF 25 V | the 5 V build's item 9 |
| Fuse | 3 A slow-blow and an inline holder | **≈ $6** |
| DC jack | 5.5 × 2.1 mm screw terminal | **≈ $5** |
| Terminal barrier | 4-position screw terminal | **≈ $8** |
| Tube | 2.5 mm ID × 4 mm OD | the 5 V build's item 10 |
| Tee | 3/32" | the 5 V build's item 11 |
| Breadboard | 2 × 830-point + jumpers | the 5 V build's item 8 |

The shared rows are the 5 V build's items 1, 3, 4, 5, 6, 7, 8, 9, 10 and 11, about $82 of its
total. The new rows add about $88, so the 12 V build is **≈ $170**. The 5 V pump and the
5 V → 3.3 V buck are not bought. The 12 V pump, supply and two bucks are the estimates.

A 6 mm barb needs a short length of 4 mm ID tube and a 4 mm-to-2.4 mm reducer: the 5 V build's
2.5 mm ID tube will not stretch to 6 mm, and the tee is 3/32".

### Wiring

```
12 V PSU ── 3 A fuse ──┬── 1000 µF ── GND
                       │
                       ├── PUMP (+) ─────────── DRAIN
                       │                          │
                       │                      [ MOSFET ] ── SOURCE ── GND
                       │                          │
                       │        GPIO6 ── 100R ── GATE
                       │                  10k ── GND
                       │
                       ├── 12 V → 3.3 V buck ── DRV8833 VM ── valve (channel B)
                       │
                       └── 12 V → 5 V buck ── MCU "5V" pin

   1N5822 across PUMP (+) and DRAIN, cathode to +12 V · one star ground:
   PSU -, both bucks' -, MOSFET source, DRV8833 GND, MCU GND
```

The pin map is the 5 V build's: GPIO6 is the pump PWM, GPIO5 is the valve, and the sensor, panel,
buttons and buzzer do not move. It is the sketch's `Board` block, so a variant of the rig is that
block and nothing in the libraries. The DRV8833 drives only the valve — `BIN1` from GPIO5, `BIN2` and
both `AIN` inputs to GND — with its `VM` on the 3.3 V rail, so the coil sees the same voltage as
the 5 V build and its body diodes are the coil's flyback path. The pump's diode and capacitor are
at the pump, not at the FET: the capacitor is the surge reservoir and the diode is the winding's
path when the FET turns off.

The 12 V pump path is on the terminal barrier: fuse, pump, diode, capacitor and FET. A breadboard
contact is not a 2 A connector. The MOSFET's tab is at drain potential, so keep it clear.

The supply's negative is the star ground. Join the two bucks' returns, the MOSFET source, the
DRV8833 ground and the MCU ground there, and keep the sensor's return off the pump's path. The
sensor stays on the MCU's `3V3` net, as in the 5 V build, because its output is ratiometric.

### Accepted compromises

* The 12 V supply is a brick on the bench, not the PC's port. That removes the 500 mA USB 2.0
  ceiling and the 1.2 A surge, and adds a mains-powered supply and a 3 A fuse to the rig.
* The valve is still the 3–5 V part, on 3.3 V. Its rated differential may be less than the pump's
  -80 kPa: if the chamber leaks down faster than the 5 V build, the valve seat is the first
  suspect, and a deeper-rated valve is the fix.
* The valve rail is a 12 V → 3.3 V buck, separate from the MCU's 5 V rail. That is a second buck,
  but it is what keeps the fail-safe: losing the 12 V drops the valve rail and vents the chamber
  even if USB is plugged in.
* The pump is brushed. The firmware PWMs the supply through a low-side FET; a BLDC pump with its
  own controller wants a steady rail, and low-side PWM into that controller is not a speed control.
* The pump draws 1–2 A at 12 V, which a breadboard is not rated for. The 12 V path is on a
  screw-terminal barrier; the breadboard carries the logic side only.
* The 1000 µF is on the 12 V rail, at the pump, as the surge reservoir. The 3.3 V valve rail is a
  0.18 A DC load and needs no bulk beyond the DRV8833 module's capacitors.
* The pump's port is 4 mm, the same as the 5 V pump, so the 2.5 mm ID tube is the same 1.6x
  stretch. A 6 mm barb needs a short 4 mm ID length and a 4 mm-to-2.4 mm reducer; the tee is 3/32".
* The sensor is still on the MCU's `3V3` and still ratiometric. The larger pump return makes the
  star ground and the separate sensor return more important, not less.
* The pump pulls deeper, so the tube-as-chamber is not enough: 2.5 mm ID silicone collapses at this
  depth. Use a small rigid chamber — a jar with a bulkhead, or a short rigid tube — and keep
  flexible tube for the runs to the pump, sensor and valve.
* The sensor's range is -100 kPa, so a pump that pulls deeper saturates the reading. Check the
  pump's ultimate vacuum on its datasheet; a single-stage diaphragm pump lands inside the sensor,
  a two-stage one may not.
* The pump's measured reach is `CONTROL_FLOOR_KPA`, so build with it once you have measured it —
  `--define CONTROL_FLOOR_KPA=<kPa>` — and `--define VALVE_NORMALLY_OPEN=0` if the fitted valve is
  normally closed. The PID gains are derived from the pump's free flow and the chamber volume, so a
  new pump or chamber is retuned by the two build symbols `PUMP_FLOW_LPM` and `CHAMBER_VOLUME_L`,
  not by re-deriving gains. The duty cap is `PUMP_DUTY_MAX_PERCENT`; the default 90 % is 10.8 V
  here, keep it unless the pump's datasheet allows continuous 12 V.
  `PUMP_RAMP_PERCENT_PER_SEC` stays: a larger diaphragm still dislikes a step.
* The MCU's 5V pin is the 12 V → 5 V buck's output. Check the Super Mini for a Schottky between
  USB VBUS and that pin; if it has none, use a cable with VBUS cut or unplug USB while the 12 V is
  on. Two 5 V sources on one pin is not a fault the board protects you from.
* Startup surge: a diaphragm pump's inrush is several times its running current, which is why the
  supply is 5 A for a 2 A pump. The 1000 µF covers the first few milliseconds; the adapter's
  current limit protects the adapter, not the rig.
* No chamber, so no realistic pump-down time or leak measurement, as in the 5 V build.

The sensor, panel, buttons, buzzer, valve and firmware interface do not change. The normally-open
valve is still the hardware guarantee: its rail comes from the 12 V supply, so losing the supply
vents the chamber. A bigger pump pulls a small chamber down faster, so use a rated chamber, a catch
pot and a filter, and keep the rig attended.
