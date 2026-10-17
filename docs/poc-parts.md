# Gardener proof-of-concept parts

Everything the rig is built from, in one list. One 5 V rail, one switch module, one tube size.
Bench use, dry air, attended.

**Total to buy: $101.45.** Only items 1 and 7 are actually on hand — everything else still has to
be bought.

## Parts

| # | Part | What it is | Get it |
| --- | --- | --- | --- |
| 1 | ESP32-S3 Super Mini | The MCU. Native USB CDC is the control interface | owned |
| 2 | JQB2438 pump | Micro diaphragm vacuum pump, 3 V/3.7 V winding, 0.45 A | **$16.84** — [B0GR4K9K4P](https://www.amazon.com/dp/B0GR4K9K4P) |
| 3 | Solenoid valve | uxcell miniature 2-way, normally closed, 4.5 V 0.5 A, 3 mm ports | **$9.59** — [B07WR74SKV](https://www.amazon.com/dp/B07WR74SKV) |
| 4 | HX710B module | DWEII pressure module. 24-bit bridge ADC, `VCC / OUT / SCK / GND`, 0–40 kPa gauge | **$11.59** — [B09V5BDFF6](https://www.amazon.com/dp/B09V5BDFF6) |
| 5 | OLED | 1.3" 128x64 SH1106, 7-pin SPI | **$12.99** — [B0H7HVTLB5](https://www.amazon.com/dp/B0H7HVTLB5) |
| 6 | Button module | 5-way navigation switch, active low, pull-ups on the module | **$9.99** — [B0D8T4YQ3Y](https://www.amazon.com/dp/B0D8T4YQ3Y) |
| 7 | PD trigger | USB-C PD trigger board, set to 5 V. No battery in this build | owned |
| 8 | **DRV8833 module** | Dual H-bridge, 2 × 1.5 A, logic-level inputs. Drives the pump and the valve | **$6.99** — [B0G1RMXMSF](https://www.amazon.com/dp/B0G1RMXMSF) |
| 9 | **Breadboard + jumpers** | 2 × 830-point board with a wire kit | **$9.99** — [B0H17GR4S5](https://www.amazon.com/dp/B0H17GR4S5) |
| 10 | **Bulk capacitor** | 1000 µF 25 V electrolytic, across the 5 V rail | **$6.99** — [B0F8C1VMT6](https://www.amazon.com/dp/B0F8C1VMT6) |
| 11 | **Silicone tube** | 2 mm ID. Stretches onto the sensor's 2.5 mm port and the valve's 3 mm ports | **$9.99** — [B08BRB2LXX](https://www.amazon.com/dp/B08BRB2LXX) |
| 12 | **Barbed tee** | 3/32" (2.4 mm), 4-pack | **$6.49** — [B0DG2YT19V](https://www.amazon.com/dp/B0DG2YT19V) |

Items 2–6 are the pressure/plumbing set already in your Amazon cart; items 8–12 are the ones still
to add. A 1 A slow-blow fuse on the 5 V feed is optional, not required.

## Wiring

```
PD trigger @ 5 V ─┬─ MCU "5V" pin ── its 3V3 pin ─┬─ HX710B   SCK + OUT
                  │                               ├─ OLED     5-wire SPI
                  │                               └─ buttons  4 pins
                  ├─ DRV8833 channel A ──── pump
                  └─ DRV8833 channel B ──── valve

   1000 µF across the 5 V rail · one star ground
```

```
GPIO 12 13 14 15 16   panel   SCK, MOSI, CS, DC, RES
GPIO 17 18            HX710B  SCK, OUT
GPIO  5  6            valve, pump
GPIO  7  8  9         spare
```

```
   pump inlet ──┐
   HX710B port ─┼── 3/32" tee ── 2 mm ID tube
   valve port ──┘
```

Wire the DRV8833 as a switch, not as an H-bridge: tie each channel's `IN2` to GND and drive `IN1`.
That gives one active-high GPIO per load, which is what the firmware expects. Its body diodes are
the flyback path, so no diodes, gate resistors, pull-downs or screw terminals are needed.

The tube volume is the chamber. No vessel, no catch pot, no filter.

## Firmware, before it will run

1. **A new sensor driver.** Nothing in the tree speaks the HX710B. It needs a new class in
   [hal/impl/](../libraries/VacuumCore/src/hal/impl) implementing
   [PressureSensor](../libraries/VacuumCore/src/hal/PressureSensor.h), named in
   [App.h](../libraries/VacuumCore/src/app/App.h) where `MprlsSensor` is today. The part is
   **gauge** — zero at atmosphere, negative under vacuum — while the rest of the firmware assumes
   an absolute reading with a captured ambient, so `kpa()` makes that conversion. `OUT` swings to
   its own `VCC`, which is why it runs at 3.3 V.
2. **The panel moves to SPI.** [App::attachPanel()](../libraries/VacuumCore/src/app/App.cpp)
   builds an I²C SH1106 on `&Wire` and calls `abort()` if it does not answer, and `abort()` panics
   — so a SPI-only rig boot-loops. It needs SPI pins in
   [core/Config.h](../libraries/VacuumCore/src/core/Config.h) and the SPI constructor.
3. **The valve class inverts.** `NormallyOpenValve` → `NormallyClosedValve` in
   [App.h](../libraries/VacuumCore/src/app/App.h). De-energised then means sealed, so the fail-safe
   inverts and the coil draws 0.5 A whenever the chamber is open.

## Accepted compromises

Bench-only, to fix once the loop works.

* The pump runs at 5 V against a 3 V/3.7 V rating. Two 1N5819s in series drop it to about 4.1 V.
* The valve runs at 5 V against a 4.5 V rating — about 11 % over.
* The sensor shares the MCU's 3.3 V rail and ground with a brushed pump. Expect jumpy readings
  while pumping. Keep the pump's return out of the sensor's ground and put 100 nF at the module.
  First upgrade if that is not enough: an AMS1117-3.3 off the 5 V rail, for the sensor alone.
* No chamber, so no realistic pump-down time and no leak measurement.
* The buttons are wired but nothing in the firmware reads them.
