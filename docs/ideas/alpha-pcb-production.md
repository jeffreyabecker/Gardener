# The production build: a carrier board at 100-300 units

## Summary

A 5 V bench vacuum controller for a 100-300 unit run: USB-C 5 V in, a 5 V pump and a 5 V valve on
one bare DRV8833, the ESP32-S3 Super Mini as the MCU, the 1.3" 128x64 SH1106 panel, and six keys
through a 74HC165 — no buck, no PD. Three boards — carrier, button module, faceplate — go into an
off-the-shelf plastic box with a PCB faceplate, and everything is AliExpress except the DigiKey
sensor.

**Parts land around $28-42 a unit at 100-300 pieces.** The pump is the biggest line ($8-12); the
valve ($4-6) and the sensor ($5.25) follow; and the electronics — MCU, panel and both assembled
boards — are under $15 together. Assembly, test and finishing are the other half of the cost and are
not in that figure.

The POC build is a breadboard with everything bought as a module; the alpha board commits one of
those modules — the DRV8833 — to copper and leaves the rest plugged in. At a 100-300 unit run the
arithmetic changes: hand-assembly, module cost and enclosure labour dominate, so what integrates and
what stays a module is worth reopening. This document is that pass, under these constraints: one build, 5 V, fed from USB-C with a 3 A
budget; everything is AliExpress except the XGZP6847A sensor; the panel stays the 1.3" 128x64
SH1106; and the custom PCBs are a carrier board that everything plugs into — bulk capacitors
included — plus button modules. The enclosure is an off-the-shelf box with a PCB faceplate.

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

## The button module: a shift register

The module is six keys on a small PCB behind the faceplate, read through one 74HC165: the eight
inputs are latched and shifted in over three GPIOs (SH/LD, CLK, Q7), the six keys take six inputs
in panel order and the two spares are tied high, and the register rides with the switches so the
carrier sees a five-wire header (3V3, GND, SH/LD, CLK, Q7). This is exactly the POC's `Buttons` —
the part has no change output, so the register is shifted on every read and a level is accepted only
once it holds still, on the panel task's 10 ms beat. The only cost of more keys later is a second
register daisy-chained onto the same three GPIOs.

Six single-key modules are only for a faceplate that scatters the keys; with one register, one
six-key board behind the faceplate is the natural shape.

## The two PCBs, itemised

The run's boards are three — the carrier, the button module, and the faceplate — and what lands on
each is a parts decision as much as a layout one. Prices
are LCSC/JLCPCB catalog bands to pin at order time — JLCPCB assembly adds its loading fee and a
per-joint cost on top of the part price, and a basic part carries no extended-library fee.

### The carrier

| Part | Unit | Note |
| --- | --- | --- |
| DRV8833PWPR | ~$0.50-0.90 | the driver, HTSSOP-16 |
| Bulk 1000 µF | ~$0.15-0.40 | SMD — see below |
| Piezo buzzer | ~$0.15-0.30 | SMD, passive, on GPIO12 |
| Connectors | ~$0.30-0.50 | sensor socket, pump, valve, OLED, button header |
| Passives | ~$0.20-0.40 | the driver's four caps, the sensor's 100 nF + 10 k |

Fabrication and assembly at a few hundred boards — the bare PCB, the SMT joints and the amortised
setup — run about $0.60-1.20 on top of those parts, so an assembled carrier lands around **$2-4**.

**Can the bulk cap be SMD?** Yes, and it should be. The POC's part was a 25 V through-hole can
because that build had a 12 V rail; this rail is 5 V, so 16 V is already three times the working
voltage. A 1000 µF 16 V SMD aluminium electrolytic — a "V-chip", about 10 × 10.5 mm — reflows with
everything else and costs a few tens of cents. MLCCs do not reach 1000 µF at 16 V at any price worth
paying, and a polymer or tantalum SMD part gets there but costs more than the electrolytic for
nothing on a surge reservoir.

### The button module

| Part | Unit | Note |
| --- | --- | --- |
| 74HC165 | ~$0.06-0.15 | the shift register, rides with the switches |
| Six SMD tact switches | ~$0.10-0.20 | 6 × 6 mm |
| Connector | ~$0.05 | five pins to the carrier |
| Six button caps | ~$0.12-0.30 | mechanical, snap on through the faceplate |

The caps are not soldered — they are a mechanical part that fits over each switch through the
faceplate, bought with the enclosure rather than placed by the assembler. Fabrication and assembly
on a board this small are about $0.30-0.60, so the assembled module lands around **$0.50-1.00**, with
the six caps another ~$0.12-0.30 on top.

### The faceplate

A bare board, no components: the OLED window and the six key holes are routed cut-outs, and the
labels are silkscreen. Fabrication is the whole cost — about $0.20-0.50 at a few hundred boards.

## The valve: normally open or normally closed

The POC and the machine spec assume a normally open valve: de-energised is open, so the chamber
vents on any fault, and running a vacuum means energising the coil for the whole pull and the whole
hold. The firmware already covers both — [the machine spec](../specs/vacuum-control.md) names the
fitted direction in the `valve` setting, and `NormallyClosedValve` in the machine library inverts
the pin — so the choice is a parts and product decision, not a firmware one.

| | Normally open | Normally closed |
| --- | --- | --- |
| De-energised | vents (chamber to air) | seals (chamber holds) |
| Coil current | the whole pull and hold, ~100-180 mA at 5 V | only while venting |
| Fail-safe | a fault vents — never holds vacuum unexpectedly | a fault seals — holds through a power loss |
| Failure mode | power loss releases the chamber | pump running while the valve driver is dead keeps pulling |

Normally open is the safe direction for a bench instrument: the thing it must never do is hold a
vacuum nobody asked for, and the coil's continuous ~0.5-0.9 W is the price of that guarantee. Its cost is
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

## Enclosure

An off-the-shelf plastic box with a PCB faceplate. The box is a few dollars in lots; the faceplate
is a third board, with the 1.3" OLED window and the six key holes routed into it and the labels
printed in silkscreen — same fab as the other boards, so the holes land exactly where the button
module's switches sit and the silkscreen is already paid for. That also settles the button module:
one six-key board behind the faceplate is the natural form, and single-key modules stay on the
shelf.

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
| Carrier PCB, assembled | JLCPCB | $2-4 | driver, bulk cap, piezo, connectors — itemised above |
| Button module, assembled | JLCPCB | $0.60-1.30 | register, switches, connector, caps |
| Enclosure: box + faceplate PCB | AliExpress + JLCPCB | $1.50-3.50 | a few-dollar box, the face is a bare board |
| Tube, tee, barbs | AliExpress | $1.50-2.50 | the wetted path |

Two things fall out of the numbers. The pump is now the biggest line, and the sensor is a mid-pack
part: at $5.25 it no longer sets the BOM, which instead follows the pump and the valve. The sensor
is still the one non-AliExpress part with no obvious drop-in, so its supply is still the thing to
hedge — but it is a supply concern now, not a cost one. And the "smart" half of the device stays the
cheap half: the Super Mini, the OLED and the three boards together are under $15.

A unit lands around $28-42 in parts. That is parts only: hand assembly, test and finishing are the
other half of the cost and are not priced here.

## What is still unknown

* **Whether the valve is normally open or normally closed** — which is a product question, not an
  engineering one, and decides the fail-safe.
* **The sensor's second source.** The XGZP6847A is the one non-AliExpress part, and cheap at $5.25
  from DigiKey but with no obvious drop-in — its continuity is the run's single point of failure
  until a second channel is confirmed.

## Where I land

The 300-unit build is the POC with six decisions already made — USB-C for the rail, the 1.3"
SH1106 kept, AliExpress except the sensor, the Super Mini as the MCU, an off-the-shelf box with a
PCB faceplate, and a 74HC165 shift register for the keys. The one decision still open is the valve
direction, and I lean normally open: it keeps the fail-safe that a fault vents, and only a
hold-through-power-loss requirement would justify the normally closed part.
