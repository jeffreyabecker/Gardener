# Gardener prototype build

[bom.md](./bom.md) is the engineering bill of materials: what every part **is**, and the firmware
constant it is tied to. It carries no vendor, no order code and no price, because none of those is
a fact about the machine. This is the other half of that pair — the document you shop from. It
records the modules on hand, the modules still to buy, the rail each one sits on, and the traps
that would otherwise cost an evening.

It describes one build: a breadboard prototype fed by a USB-C PD trigger board, no battery, from
the modules already ordered. Prices and links are Amazon US as of 2026-10-16 and will move; the
ASIN is the part that stays put. Where a choice here contradicts [bom.md](./bom.md), the BOM is
describing the intended rig and this is describing the parts that turned up.

## 1. Parts on hand

| Asin | What it actually is | BOM line |
| --- | --- | --- |
| B0D8T4YQ3Y | 5-way navigation button module, 5-pack. 7-pin: `VCC`, `GND/COM` and five active-low direction outputs, pull-ups on the module. | 4 |
| B09V5BDFF6 | **HX710B** pressure module, 6-pack. 4-pin `VCC / OUT / SCK / GND`: a 24-bit ADC and a 5 kΩ bridge, 0–40 kPa gauge. | 2 |
| B07WR74SKV | Miniature 2-way solenoid valve, **normally closed**, 4.5 V 0.5 A, 3 mm ports. | 6 |
| B0GR4K9K4P | JQB2438 diaphragm vacuum pump, **3 V/3.7 V** winding, 0.45 A. Ordered twice: one ASIN, so one part. | 5 |
| B0H7HVTLB5 | 1.3" 128x64 SH1106 OLED, 2-pack, **7-pin SPI** — `GND VCC SCL SDA RES DC CS`. | 3 |

Four of the five are not the part the BOM names, and three of those differences reach the
firmware. [§ 6](#6-what-this-build-asks-of-the-firmware) is the list.

Two things read wrong on arrival and are worth checking with a meter before they are trusted: the
OLED's `SCL`/`SDA` silkscreen is SPI, not I²C, and the HX710B's `OUT` swings to whatever `VCC`
is, so the supply choice sets the logic level.

## 2. Power

One PD trigger board set to **12 V**, no battery. The whole rig draws about 7 W, so any PD source
of 20 W or more will carry it. 12 V rather than 20 V because a buck converter's failure mode is
input pass-through, and a rail that can destroy 3.3 V logic is not worth the headroom.

```
PD trigger @ 12 V ─┬─ buck → 5.0 V ─── board "5V" pin → on-board LDO → 3.3 V logic
                   ├─ buck → 3.7 V ─── pump MOSFET drain
                   └─ buck → 4.5 V ─── valve MOSFET drain

   5 V ── AMS1117 ── 3.3 V ─── HX710B, OLED VCC, button module VCC

   1000 µF across the pump rail · 1 A slow-blow on the 12 V feed · one star ground
```

| Rail | Source | Feeds | Draw |
| --- | --- | --- | --- |
| 12 V | PD trigger | the three bucks | ~0.6 A |
| 5.0 V | buck 1 (LM2596) | the MCU board's `5V` pin | ~0.2 A |
| 3.7 V | buck 2 (LM2596) | the pump | ~0.45 A |
| 4.5 V | buck 3 (LM2596) | the valve | ~0.5 A |
| 3.3 V | the board's own regulator | nothing but the button module | ~10 mA |
| 3.3 V, quiet | AMS1117 from 5 V | HX710B, OLED | ~25 mA |

The pump and the valve get their own buck rather than sharing 5 V, because the pump is a 3 V part
and the valve a 4.5 V one. The logic rail stays off the load rails entirely. The battery and its
divider (BOM items 12 and 13) are omitted: tie GPIO4 to GND through 100 k and the firmware reports
no pack fitted rather than a permanent 0 %, which is what [README.md § Battery](../README.md#battery)
describes.

## 3. Modules to buy

| # | Need | Pick | Price | Link |
| --- | --- | --- | --- | --- |
| 1 | Logic-level MOSFETs, one per load | Genuine Infineon **IRLZ44N**, 5-pack | $8.99 | [B0FMQCYG6Q](https://www.amazon.com/dp/B0FMQCYG6Q) |
| 2 | 100 Ω gate resistors, 10 kΩ gate pull-downs | Chanzon **60-value 1/4 W** metal film kit | $9.99 | [B08PF3HNMD](https://www.amazon.com/dp/B08PF3HNMD) |
| 3 | Flyback diodes, one per load | Chanzon **1N5819** Schottky, 100-pack | $7.99 | [B079KG1TN2](https://www.amazon.com/dp/B079KG1TN2) |
| 4 | Bulk capacitor, pump rail | **1000 µF 25 V** Rubycon | $6.99 | [B0F8C1VMT6](https://www.amazon.com/dp/B0F8C1VMT6) |
| 5 | Three bucks: 12 V → 5.0 / 3.7 / 4.5 V | **LM2596** adjustable, 5-pack | $7.99 | [B0DBVYP91F](https://www.amazon.com/dp/B0DBVYP91F) |
| 6 | Isolated 3.3 V rail for the analogue side | **AMS1117-3.3** modules, 10-pack | $6.49 | [B01HXU1NQY](https://www.amazon.com/dp/B01HXU1NQY) |
| 7 | Breadboard and jumpers | 2x 830-point board with a wire kit | $9.99 | [B0H17GR4S5](https://www.amazon.com/dp/B0H17GR4S5) |
| 8 | Screw terminals for the load wiring | 2.54 mm PCB-mount assorted, 46-piece | $14.99 | [B0FCXV6GT3](https://www.amazon.com/dp/B0FCXV6GT3) |
| 9 | Fuse and holder, 12 V feed | Inline holder with 150-piece fuse kit | $9.99 | [B07F8RLMPB](https://www.amazon.com/dp/B07F8RLMPB) |
| 10 | ESP32-S3 board, **only if the one on hand is not an S3** | ESP32-S3 Super Mini, 4-pack | $16.99 | [B0H8MV4RGZ](https://www.amazon.com/dp/B0H8MV4RGZ) |

About $100 for the electronics.

Item 1 is bare transistors rather than a module on purpose. The common cheap MOSFET modules are
either the IRF520 boards, which are not logic level and sit in their linear region at a 3.3 V gate,
or opto-isolated boards whose optocoupler is far too slow for [PwmPump.cpp](../libraries/VacuumCore/src/hal/impl/PwmPump.cpp)'s
`kPwmFreqHz` of 20 kHz, or the D4184 boards specified to 0–20 kHz — the pump's frequency exactly.
The IRLZ44N is the part [README.md § Pump driver](../README.md#pump-driver) already draws, has no
switching ceiling, and dissipates about 20 mW at 0.45 A. If screw terminals are wanted on the
valve channel, which carries no PWM, a D4184 module ([B0D9VWWLCB](https://www.amazon.com/dp/B0D9VWWLCB),
$5.49) is logic level and fine there.

## 4. Wiring notes

* **Meter every buck before a load touches it.** LM2596 boards ship at an arbitrary setting and the
  trim pot is fiddly. Set 5.0, 3.7 and 4.5 V against a meter, then leave them alone.
* **Run the HX710B at 3.3 V.** Its `OUT` swings to `VCC`, so 5 V there means 5 V into a GPIO. The
  bridge excitation comes off the chip's own regulator, so a 3.3 V supply scales the full-scale
  output down proportionally — calibrate counts to kPa against a reference and the difference
  disappears. If the reading turns out too noisy at 3.3 V and the module has to go to 5 V, divide
  `OUT` with 10 k in series and 20 k to ground; no level shifter board is needed either way.
* **The analogue side gets its own ground.** Feed the HX710B from the AMS1117, put 100 µF and
  0.1 µF at its output, and keep the pump's current return out of the sensor's path. A 24-bit
  converter sharing a ground with a brushed motor reads every brush.
* **Load wires land on screw terminals, not the breadboard.** Half an amp through spring contacts
  is a bad idea. Gate, pull-down and signal wiring can stay on the board.
* **Wire the gate network as drawn** — GPIO through 100 Ω to the gate, 10 kΩ gate to source, source
  to the common ground, load between its rail and the drain, diode across the load with the
  cathode to that rail. Pulse-width control on GPIO6, on/off on GPIO5; neither pin ever touches a
  load directly.
* **The fuse goes on the 12 V feed**, 1 A slow-blow. The PD source has its own over-current
  protection, but it answers a fault by dropping the rail and renegotiating from scratch, which
  browns out the board rather than protecting anything.
* **The OLED is SPI.** Its pin labels say `SCL` and `SDA`; they mean clock and MOSI. Seven wires,
  and nothing on the I²C bus.

## 5. Pneumatics

| Need | Pick | Price | Link |
| --- | --- | --- | --- |
| 3 mm ID tube for the valve and the tees | Quickun silicone, 3 mm ID x 5 mm OD | $5.99 | [B08BRC5FPB](https://www.amazon.com/dp/B08BRC5FPB) |
| 2 mm ID tube for the sensor barb | Quickun silicone, 2 mm ID x 4 mm OD | $9.99 | [B08BRB2LXX](https://www.amazon.com/dp/B08BRB2LXX) |
| 1/8" barbed tee, valve side | 10-pack | $5.98 | [B0F8HN7KC8](https://www.amazon.com/dp/B0F8HN7KC8) |
| 3/32" barbed tee, sensor side | 4-pack | $6.49 | [B0DG2YT19V](https://www.amazon.com/dp/B0DG2YT19V) |
| Catch pot and moisture trap | Master Airbrush inline trap | $8.99 | [B002FDZM08](https://www.amazon.com/dp/B002FDZM08) |

Both tube sizes are needed: the sensor's barb is about 2.5 mm and the valve's ports are 3 mm, and
while silicone stretches, a 3 mm barb into 2 mm bore is a fight every time. The airbrush trap has a
1/4" NPT inlet, so it wants a 1/4" NPT to 3 mm barb reducer — or skip it and use a small jar.

**No chamber is listed, because its size decides whether the rig can pull down at all.** A 3.7 V
pump and a 3 mm valve orifice between them move very little air, so the vessel has to be small:
under about 250 mL, rigid, with a bulkhead barb. A polycarbonate jar or bottle is the cheap answer. A one-litre vessel will take
minutes to pump down and longer to vent, and `VENT_TIMEOUT_MS` in
[Config.h](../libraries/VacuumCore/src/core/Config.h) gives venting only 30 s before the firmware
gives up on it.

## 6. What this build asks of the firmware

Three of the substituted parts need code, and one needs a number re-measured.

* **The sensor driver is a rewrite.** [MprlsSensor.cpp](../libraries/VacuumCore/src/hal/impl/MprlsSensor.cpp)
  speaks I²C at `0x18` to an MPRLS and reports kPa absolute. The fitted module is an HX710B: two
  plain GPIOs, a bit-banged clock-and-data protocol, 24-bit counts, and gauge rather than absolute
  pressure. That is a new class in [hal/impl/](../libraries/VacuumCore/src/hal/impl) implementing
  the same [PressureSensor](../libraries/VacuumCore/src/hal/PressureSensor.h) interface, pointed at
  from [App.h](../libraries/VacuumCore/src/app/App.h). The gauge element makes ambient zero by
  construction, which is the assumption `captureAmbient()` and `vacuumKpa()` are built around and
  will need rethinking rather than renumbering.
* **The display needs its interface moved.** [App.cpp](../libraries/VacuumCore/src/app/App.cpp)
  attaches an I²C SH1106 on `&Wire` with no reset line; the panel on hand is 7-pin SPI.
  [Config.h](../libraries/VacuumCore/src/core/Config.h) has no SPI pins, and GPIO10/11 are
  `BTN_OK`/`BTN_BACK`, so the S3's SPI defaults are not all free. This is the open item already
  recorded in [bom.md § 5](./bom.md#5-open-items).
* **The valve inverts.** [App.h](../libraries/VacuumCore/src/app/App.h) fits `NormallyOpenValve`
  because that is the fail-safe; the part on hand is normally closed, so it becomes
  `NormallyClosedValve`. Two consequences: a power loss now holds the vacuum instead of venting it,
  and the coil is energised whenever the chamber is open, including at boot, at 0.5 A continuously.
* **The pump's floor needs measuring.** `PUMP_MAX_SUCTION_KPA` in
  [Config.h](../libraries/VacuumCore/src/core/Config.h) is the 12 V winding's measured 60 kPa. The
  fitted pump is the 3 V/3.7 V one at 0.45 A and will not reach it. That constant also sets
  `CONTROL_FLOOR_KPA` and the `min` and `max` the wire publishes for `plant target`, so it is worth
  measuring before trusting any target.

## 7. Not needed

Relay modules, H-bridge or motor-driver boards, external ADCs, level-shifter boards, STEMMA QT
cables, and anything for the battery. The pump is unidirectional, so a MOSFET is the correct
switch; the HX710B is already the ADC; and nothing in this build runs 5 V logic.
