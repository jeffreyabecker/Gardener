# Gardener proof-of-concept parts

Everything the rig is built from, in one list. One 5 V rail with a 3.3 V load rail off it, one
switch module, one tube size. Bench use, dry air, attended.

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

Which of these parts move onto the alpha carrier board, and which stay modules, is in
[alpha-pcb.md](../plans/alpha-pcb.md).
