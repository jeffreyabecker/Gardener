# Gardener proof-of-concept parts

Everything the rig is built from, in one list. One 5 V rail with a 3.3 V load rail off it, one
switch module, one tube size. Bench use, dry air, attended.

**Total to buy: $132.93.**

## Parts

| # | Part | What it is | Get it |
| --- | --- | --- | --- |
| 1 | ESP32-S3 Super Mini | The MCU, 4-pack. Native USB CDC is the control interface | **$16.99** — [B0H8MV4RGZ](https://www.amazon.com/dp/B0H8MV4RGZ) |
| 2 | JQB2438 pump | Micro diaphragm vacuum pump, 3 V/3.7 V winding, 0.45 A | **$16.84** — [B0GR4K9K4P](https://www.amazon.com/dp/B0GR4K9K4P) |
| 3 | Solenoid valve | Mini 2-way **normally open**, 3.0–5.0 V, ~180 mA at 3.3 V. Ports 1 and 2 in use; port 3 capped | **$9.63** — [B0HDRX4C44](https://www.amazon.com/dp/B0HDRX4C44) |
| 4 | **MPRLS module** | Adafruit MPRLS ported pressure sensor breakout. 24-bit I2C, 0–25 PSI (0–172 kPa) absolute, `VIN / GND / SDA / SCL`, address `0x18`. Pre-calibrated, so no bridge ADC | **$29.95** — [3965](https://www.adafruit.com/product/3965) |
| 5 | **OLED** | Generic 1.3" 128x64 SH1106, I2C `0x3C`, `VCC / GND / SDA / SCL`. On its own bus, separate from the MPRLS | **$6.99** — [B0HFSCMGWD](https://www.amazon.com/Display-Breadboard-Multicolored-Compatible-Raspberry/dp/B0HFSCMGWD) |
| 6 | Button module | KOOBOOK 5-way navigation switch: up, down, left, right, centre — 5 switches, 5 GPIOs. 2.0–9.0 V | **$7.29** — [B07SC1H42W](https://www.amazon.com/dp/B07SC1H42W) |
| 7 | **DRV8833 module** | Dual H-bridge, 2 × 1.5 A, logic-level inputs. Drives the pump and the valve | **$6.99** — [B0G1RMXMSF](https://www.amazon.com/dp/B0G1RMXMSF) |
| 8 | **Breadboard + jumpers** | 2 × 830-point board with a wire kit | **$9.99** — [B0H17GR4S5](https://www.amazon.com/dp/B0H17GR4S5) |
| 9 | **Bulk capacitor** | 1000 µF 25 V electrolytic, across the 3.3 V load rail | **$6.99** — [B0F8C1VMT6](https://www.amazon.com/dp/B0F8C1VMT6) |
| 10 | **Silicone tube** | 2.5 mm ID x 4 mm OD, 6 ft. Covers every port: 2.4 mm tee, 2.5 mm MPRLS port, 3–3.5 mm valve, 4 mm pump | **$5.79** — [B0DMP15ZWS](https://www.amazon.com/dp/B0DMP15ZWS) |
| 11 | **Barbed tee** | 3/32" (2.4 mm), 4-pack | **$6.49** — [B0DG2YT19V](https://www.amazon.com/dp/B0DG2YT19V) |
| 12 | **3.3 V buck module** | 5 V → 3.3 V fixed, 1.8 A. Feeds the DRV8833, so it runs both loads | **$8.99** — [B0FP2TLXYW](https://www.amazon.com/dp/B0FP2TLXYW) |

Power comes from the PC's USB port through the MCU's USB-C cable, so that cable has to be
data-capable. A 1 A slow-blow fuse on the 5 V feed is optional, not required.

## Wiring

```
PC USB 5 V ─┬─ MCU "5V" pin ── its 3V3 pin ─┬─ MPRLS    SDA + SCL   (Wire, 8/9)
(via USB-C) │                               ├─ OLED     SDA + SCL   (Wire1, 17/18)
            │                               └─ buttons  5-way
            └─ 3.3 V buck ── DRV8833 VM ─┬─ channel A ── pump
                                         └─ channel B ── valve

   1000 µF across the 3.3 V load rail · one star ground
```

```
GPIO  8  9            I2C     MPRLS 0x18 (Wire)
GPIO 17 18            I2C     panel 0x3C (Wire1)
GPIO  6               pump
GPIO  5               valve
GPIO  1  2  10 11     buttons 5-way (four keys read)
GPIO  4               battery sense (reserved)
GPIO 12               buzzer (passive piezo)
GPIO  7 13 14 15 16   spare
```

A passive piezo on GPIO12 is an add-on to this list; the firmware drives it directly from one
LEDC channel. GPIO12 is the 5-way module's fifth key in the original wiring, but the firmware reads
only four keys, so that key is not wired on a build that fits the buzzer.

```
   pump inlet ──────┐
   MPRLS port ──────┼── 3/32" tee ── 2.5 mm ID tube
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
* The MPRLS port is smooth stainless, 2.5 mm, not barbed, so the 2.5 mm ID tube is only a slip fit
  there. Push it fully home; under vacuum the atmosphere clamps it on, but it is the joint most
  likely to slip. A short 3/32" (2.4 mm) ID sleeve is the fix if it does.
* The sensor shares the MCU's 3.3 V rail and ground with a brushed pump, and that rail dips when
  the pump starts. The MPRLS converts and compensates internally, so noise is more likely to show
  up as I2C errors than as jumpy readings, but keep the pump's return out of the sensor's ground
  and put 100 nF at the module. First upgrade if that is not enough: an AMS1117-3.3 off the 5 V
  rail, for the sensor alone.
* No chamber, so no realistic pump-down time and no leak measurement.
* The 5-way module costs five GPIOs but the firmware reads four keys. GPIO12, the fifth key's pin,
  drives the buzzer instead; the key is not wired on a build that fits the piezo.

## To investigate

* **[CFSensor XGZP6847A100KPGN33](https://www.digikey.com/en/products/detail/cfsensor/XGZP6847A100KPGN33/22530823)** —
  a 100 kPa gauge part in a 3.3 V variant. A 100 kPa range covers the pump's ~60 kPa pull with
  margin, and it could be a smaller or cheaper alternative to the MPRLS. Check the datasheet for
  the output — the XGZP6847A family looks analog, which would need an ADC channel rather than the
  I2C bus (GPIO7 is the one free ADC1 pin) — plus its transfer function, accuracy, temperature
  compensation and port size before adding it to the buy list.

Which of these parts move onto the alpha carrier board, and which stay modules, is in
[alpha-pcb.md](../plans/alpha-pcb.md).
