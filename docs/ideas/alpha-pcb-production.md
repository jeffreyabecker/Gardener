# The production build: a carrier board at 100-300 units

The POC build is a breadboard with everything bought as a module; the alpha board commits one of
those modules — the DRV8833 — to copper and leaves the rest plugged in. At a 100-300 unit run the
arithmetic changes: hand-assembly, module cost and enclosure labour dominate, so what integrates and
what stays a module is worth reopening. This document is that pass, under these constraints: one build, 5 V, fed from USB-C with a 3 A
budget; everything is AliExpress except the XGZP6847A sensor; the panel stays the 1.3" 128x64
SH1106; and the custom PCBs are a carrier board that everything plugs into — bulk capacitors
included — plus button modules. Enclosure options are at the end.

The parts and wiring are in [the POC build guide](../guides/poc-parts.md), and the
valve's role in the machine is in [the vacuum-control spec](../specs/vacuum-control.md).

## The build

The build is the POC's loop with the breadboard replaced by the carrier, and one simplification on
top: the pump and the valve are 5 V parts, so the DRV8833 runs straight off the USB 5 V rail and no
buck is needed. USB-C 5 V in, the pump and valve on the DRV8833, the MCU on the same 5 V rail
through its own regulator, and the sensor, OLED and buttons on the MCU's 3.3 V. `PwmPump` owns the
pump's LEDC channel and the valve is the part it has always been; a different pump or rail later is
a `Board` block, not a library.

## Power

USB-C at 5 V/3 A is 15 W, and the build — a 5 V pump, a 5 V valve and the MCU — sits well inside it,
with the 1000 µF absorbing the pump's start surge. The port is the MCU's own USB-C, which is also
the serial control interface, so one socket carries both data and power and no PD is involved.

There is no buck. The pump and valve are 5 V parts, so the DRV8833's `VM` is the USB 5 V rail
itself, and the only 3.3 V on the board is the MCU's own, for the sensor, OLED and buttons. Two
rails still stay two rails — 5 V for the loads, `3V3_SENSE` for the sensor and panel — but the load
rail is now the USB rail, and the one converter the POC needed is gone.

## The carrier board

The carrier is the POC's breadboard turned into copper: every off-board part lands on a connector or
socket, and the parts that were always going to be on a board — the bulk cap and the driver's
passives — live there. What moves onto it, and what does not:

| Part | On the carrier |
| --- | --- |
| DRV8833 (bare IC) | pump + valve, `VM` on the 5 V rail |
| Bulk 1000 µF | on the 5 V load rail |
| Driver passives | VM bypass, VCP, VINT |
| Sensor | 6-DIP socket, 100 nF + 10 k at it |
| OLED | 4-pin header |
| Buttons | header to the button module |
| Fuse | polyfuse footprint |
| Power in | Super Mini USB-C |

Two rails stay two rails, exactly as the POC guide insists: `3V3_SENSE` (the MCU's 3.3 V) feeds the
sensor, OLED and buttons; the 5 V load rail feeds only the driver. The sensor is analog and
ratiometric, so it stays on `3V3_SENSE` with its own quiet ground return and its decoupling at the
socket — never on the rail the pump dips. One star ground, the driver's return carrying the load
currents.

The DRV8833 integrates as the bare `DRV8833PWPR` with its PowerPAD on copper, which commits the board
to reflow or a hot plate rather than an iron. The Super Mini is the MCU and stays a module on two
female headers; its USB-C is the serial interface and the power input.

## The button module: shift register or GPIO expander

The module itself is decided — six keys on a small PCB behind the faceplate, because the physical
layout needs it. The open question is how the carrier reads it: a shift register, which the POC
already uses, or an I2C GPIO expander. The trade is polling against interrupts, and five wires
either way.

| | 74HC165 shift register | I2C GPIO expander |
| --- | --- | --- |
| Reads | 3 GPIOs shift the eight inputs in | 2 GPIOs address 8 or 16 lines |
| Cable | 5 wires: 3V3, GND, SH/LD, CLK, Q7 | 5 wires: 3V3, GND, SDA, SCL, /INT |
| Change | none — polled and shifted every read | edge interrupt, read on change only |
| More keys | daisy-chain a second register, same 3 GPIOs | one chip to 16 lines, two on a bus |
| Bus | none, bit-banged | I2C: shares the panel's Wire1 or takes its own |
| Firmware | already written (`Buttons`) | a new driver and bus planning |

The POC's `Buttons` has no change output to latch, so the register is shifted on every read and a
level is accepted only once it holds still — the panel task already does this on a 10 ms beat. An
expander inverts that: its interrupt fires on a key change, the MCU reads only then, and a sleeping
device can wake on the edge. For a bench instrument that never sleeps the difference is small; for
a battery build that idles, the interrupt is the point of the expander.

**Chip prices, JLCPCB/LCSC at a few hundred pieces** (catalog bands, pin at order time — JLCPCB adds
its assembly loading fee on top, and a basic part carries no extended-library fee):

| Part | Package | Unit | What it buys |
| --- | --- | --- | --- |
| 74HC165 | SOP-16 | ~$0.06-0.15, basic part | 8 inputs on 3 GPIOs, daisy-chainable |
| PCF8574 / PCF8574A | SOP-16 | ~$0.15-0.30 | 8 I2C lines, quasi-bidirectional, /INT |
| TCA9535 | TSSOP-24 | ~$0.40-0.70 | 16 I2C lines, true interrupt |
| MCP23017 | SSOP-28 | ~$0.80-1.20 | 16 lines, internal pull-ups |
| AW9523 | QFN-24 | ~$0.30-0.60 | 16 lines plus an LED driver — overkill here |

The spread is ten cents to a dollar twenty, so over 300 units the choice is tens of dollars at worst
— not the axis. What decides is interrupts and bus. The shift register is the cheapest thing that
already works, and the only cost of more keys is a second register on the same three GPIOs. The
expander is the answer if the build ever sleeps, wants a key press to wake it, or needs more lines
than a register chain wants to give — at the price of a new driver and a decision about which bus it
lives on (the panel's `Wire1` has room at a second address; its own bus costs two more GPIOs).

Either chip rides on the button module with the switches, so the carrier sees the same header. Six
single-key modules are only for a faceplate that scatters the keys; with one driver chip, one
six-key board is the natural shape.

## The valve: normally open or normally closed

The POC and the machine spec assume a normally open valve: de-energised is open, so the chamber
vents on any fault, and running a vacuum means energising the coil for the whole pull and the whole
hold. The firmware already covers both — [the machine spec](../specs/vacuum-control.md) names the
fitted direction in the `valve` setting, and `NormallyClosedValve` in the machine library inverts
the pin — so the choice is a parts and product decision, not a firmware one.

| | Normally open | Normally closed |
| --- | --- | --- |
| De-energised | vents (chamber to air) | seals (chamber holds) |
| Coil current | the whole pull and hold, ~180 mA at 3.3 V | only while venting |
| Fail-safe | a fault vents — never holds vacuum unexpectedly | a fault seals — holds through a power loss |
| Failure mode | power loss releases the chamber | pump running while the valve driver is dead keeps pulling |

Normally open is the safe direction for a bench instrument: the thing it must never do is hold a
vacuum nobody asked for, and the coil's continuous 0.6 W is the price of that guarantee. Its cost is
coil heat over long holds — the spec's fifteen-minute hold cap exists as much for the solenoid as
for the pump.

Normally closed is the choice for a product that *should* hold through a power event — a vacuum pot
that stays sealed, a chamber that keeps its pull. It also draws nothing while holding, so long holds
cost no coil power. Its cost is the inverted failure: if the valve driver dies while the pump runs,
the chamber keeps pulling toward the pump's ultimate, which can collapse the tube-as-chamber or pin
the sensor. The spec calls this out plainly — `NormallyClosedValve` "removes the fail-safe" — so
choosing it means accepting that the machine's stop path no longer vents on every fault.

The plumbing differs too: a three-port body used as two-way flips its pairing, so the capped port
moves to the other branch. Same valve family, different wiring.

Stay normally open unless hold-through-power-loss is a product requirement. That is the one thing
that justifies giving up the fail-safe; if it is not on the feature list, the safer valve is also
the simpler one.

## Enclosures

* **Off-the-shelf plastic box + laser-cut acrylic faceplate.** A few dollars a box in lots, and the
  faceplate carries the 1.3" OLED window and the six key holes. The standard 100-300 unit answer:
  the enclosure is a commodity, and all the bespoke work lives on one cheap acrylic sheet.
* **Full laser-cut acrylic stack** — two or three sheets and standoffs. Cheapest tooling, a bespoke
  look, but scratch-prone and less drop-tolerant. Fine for a bench tool, not for a thing carried.
* **3D-printed, FDM or SLA.** In-house flexibility, and at 300 units the constraint is print time and
  finish, not the printer. SLA gives a clean face but is brittle; FDM is cheap and rough. A good
  bridge if the box is still moving while the boards are fixed.
* **Extruded aluminium with milled end plates.** The most durable, and the dearest; the OLED window
  still needs a plate, so the faceplate work does not go away.
* **Injection moulding.** Ruled out at this volume: tooling of a few thousand dollars only pays for
  itself at ten times the run.

The acrylic-faceplate-on-a-box is the recommendation, and it decides the button module: with a
faceplate, a single six-key board behind it is the natural form, and single-key modules stay on the
shelf unless the keys must scatter.

## What a unit costs

Order-of-magnitude, AliExpress non-promotional lots of 100-300, pin at order time; landed cost adds
freight and duty, and a sample order pins a seller better than a listing does. The sensor is the
one non-AliExpress line.

| Part | Source | Est. unit | Note |
| --- | --- | --- | --- |
| XGZP6847A sensor | DigiKey | $5.25 | single-unit price; the one non-AliExpress line |
| 5 V micro diaphragm pump | AliExpress | $8-12 | 5 V winding, no buck needed |
| Solenoid valve, 5 V | AliExpress | $4-6 | normally open or normally closed alike |
| ESP32-S3 Super Mini | AliExpress | $2.50-3.50 | the MCU, as a module |
| 1.3" 128x64 SH1106 OLED | AliExpress | $2.50-3.50 | non-promo bulk figure |
| Carrier + button PCBs | JLCPCB | $2-4 | assembled, at a few hundred boards |
| Enclosure + acrylic face | AliExpress + local cut | $3-6 | the recommended box-plus-face |
| Tube, tee, barbs | AliExpress | $1.50-2.50 | the wetted path |
| 74HC165 + six switches | AliExpress | $1.00-1.50 | the button module's parts |
| DRV8833PWPR + passives | AliExpress/LCSC | ~$0.60 | bare IC plus its four capacitors |
| Connectors, headers, misc | AliExpress | $1.50-2.50 | plus the sensor's 100 nF and 10 k |
| 1000 µF bulk cap | AliExpress | ~$0.20 | the surge reservoir |
| Piezo buzzer | AliExpress | ~$0.15 | on GPIO12 |

Two things fall out of the numbers. The pump is now the biggest line, and the sensor is a mid-pack
part: at $5.25 it no longer sets the BOM, which instead follows the pump, the valve and the
enclosure. The sensor is still the one non-AliExpress part with no obvious drop-in, so its supply is
still the thing to hedge — but it is a supply concern now, not a cost one. And the "smart" half of
the device stays the cheap half: the OLED, the driver, the register, the passives and the
two PCBs together are under $15.

A unit lands around $32-48 in parts. That is parts only: hand assembly, test and finishing are the
other half of the cost and are not priced here.

## What is still unknown

* **Whether the valve is normally open or normally closed** — which is a product question, not an
  engineering one, and decides the fail-safe.
* **The button driver** — shift register or GPIO expander. The 74HC165 already works and polls at
  10 ms; an I2C expander pays a new driver for an interrupt that only matters if the build sleeps.
* **The enclosure medium**, which decides the button-module form and the faceplate tooling.
* **The sensor's second source.** The XGZP6847A is the one non-AliExpress part, and cheap at $5.25
  from DigiKey but with no obvious drop-in — its continuity is the run's single point of failure
  until a second channel is confirmed.

## Where I land

The 300-unit build is the POC with four decisions already made — USB-C for the rail, the 1.3"
SH1106 kept, AliExpress except the sensor, and the Super Mini as the MCU — and these still open. On
the open ones I lean: one six-key button module, driven by the 74HC165 that
already works unless the build ever sleeps; an off-the-shelf box with an acrylic faceplate; and a
normally open valve. The one that could still flip is the valve, if hold-through-power-loss turns
out to be the product.
