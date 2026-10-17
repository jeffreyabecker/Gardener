# Gardener proof-of-concept parts

Everything the rig is built from, in one list. One 5 V rail with a 3.3 V load rail off it, one
switch module, one tube size. Bench use, dry air, attended.

**Total to buy: $128.91.**

## Parts

| # | Part | What it is | Get it |
| --- | --- | --- | --- |
| 1 | **ESP32-C3 Super Mini** | The MCU, 3-pack. Native USB Serial/JTAG CDC is the control interface | **$12.97** — [B0GX966R9R](https://www.amazon.com/dp/B0GX966R9R) |
| 2 | JQB2438 pump | Micro diaphragm vacuum pump, 3 V/3.7 V winding, 0.45 A | **$16.84** — [B0GR4K9K4P](https://www.amazon.com/dp/B0GR4K9K4P) |
| 3 | Solenoid valve | Mini 2-way **normally open**, 3.0–5.0 V, ~180 mA at 3.3 V. Ports 1 and 2 in use; port 3 capped | **$9.63** — [B0HDRX4C44](https://www.amazon.com/dp/B0HDRX4C44) |
| 4 | **MPRLS module** | Adafruit MPRLS ported pressure sensor breakout. 24-bit I2C, 0–25 PSI (0–172 kPa) absolute, `VIN / GND / SDA / SCL`, address `0x18`. Pre-calibrated, so no bridge ADC | **$29.95** — [3965](https://www.adafruit.com/product/3965) |
| 5 | **OLED** | Generic 1.3" 128x64 SH1106, I2C `0x3C`, `VCC / GND / SDA / SCL`. Shares the bus with the MPRLS | **$6.99** — [B0HFSCMGWD](https://www.amazon.com/Display-Breadboard-Multicolored-Compatible-Raspberry/dp/B0HFSCMGWD) |
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
PC USB 5 V ─┬─ MCU "5V" pin ── its 3V3 pin ─┬─ MPRLS    SDA + SCL
(via USB-C) │                               ├─ OLED     SDA + SCL
            │                               └─ buttons  5-way
            └─ 3.3 V buck ── DRV8833 VM ─┬─ channel A ── pump
                                         └─ channel B ── valve

   1000 µF across the 3.3 V load rail · one star ground
```

```
GPIO  6  7            I2C     MPRLS 0x18, panel 0x3C
GPIO 10               pump
GPIO 20               valve
GPIO  0  1  3  5  21  buttons 5-way (unused)
GPIO  4               battery sense (reserved)
GPIO  2  8  9         do not use   strapping; 8 = LED, 9 = BOOT
```

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
* The five buttons are wired but unused. The module is a 5-way switch — up, down, left, right,
  centre — so it costs five GPIOs, not four.
* The C3 Super Mini exposes 13 GPIOs and no underside pads. GPIO2, 8 and 9 are strapping; 8 also
  carries the onboard LED and 9 is BOOT, which is why I2C is on 6/7. The five buttons take the
  remaining non-strapping pins, including 21 (U0TXD), which is harmless only because nothing
  reads them.
* This wiring is the C3 map. The firmware's pin map still names the S3 rig, so it has to move to
  match before the rig will run.
