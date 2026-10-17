# Alpha PCB scope

Which parts of [the POC build](./poc-parts.md) are worth committing to a board, and which stay as
bought modules. One module moves onto the board — the DRV8833. Everything else is either a retained
module or a mechanical part; the Super Mini and the 3.3 V buck are explicitly retained and plug into
headers.

## The short list

1. **DRV8833** — replace the module with the bare `DRV8833PWPR` (16-pin HTSSOP) and its four
   datasheet capacitors.
2. **1000 µF bulk capacitor** — on the 3.3 V load rail, at the driver.
3. **Connectors** — pump, valve, MPRLS, OLED, buttons, buck, and the Super Mini headers.
4. **100 nF at the MPRLS header**, plus one DNP pair of I²C pull-ups.

That is the whole board. Everything else is either a retained module or a mechanical part. On the
buy list, the DRV8833 module and the breadboard/jumper kit drop off; the PCB, the bare driver and
the connectors replace them.

## What moves, and why

| Part | Alpha decision | Reason |
| --- | --- | --- |
| DRV8833 module | Bare IC on board | The only module worth integrating: two logic inputs, two outputs, VM, ground and four passives. One TSSOP and four capacitors replace it. |
| 1000 µF bulk capacitor | On board | It belongs at the load. Two leads to a connector is the wrong shape for a bulk part. |
| 10 µF, 0.1 µF, 10 nF, 2.2 µF | On board | The driver's own passives: VM bypass, VCP and VINT. The module carries them today. |
| 100 nF | At the MPRLS header | The sensor's supply decoupling, as the POC parts list asks. |
| I²C pull-ups, 2 × 10 k | On board, DNP | The MPRLS breakout already fits a pair; the OLED may too. Fit them only if the bus is bare. |
| Connectors | On board | The point of the board: every off-board part lands on a connector or a socket. |

No flyback diodes, gate resistors or pull-downs: the DRV8833 has body diodes and internally
pulled-down inputs, and its FETs are driven inside the chip. The loads are 0.45 A and 0.18 A,
well inside one channel.

## What stays a module

| Part | Why |
| --- | --- |
| ESP32-S3 Super Mini | Retained by request. Two female headers; USB-C at the board edge. |
| 3.3 V buck | Retained by request. Header pins mate with a 0.1" female header; check pitch and pin order. Its output is the load rail. |
| MPRLS breakout | The bare sensor is the same calibrated part with the same port. The breakout adds only an LDO and level shifter, which are harmless at 3.3 V, and it is already tested. Revisit after the closed-loop vacuum control — sensor, PID, pump — is proven on the carrier board. |
| OLED | Panel part; a 4-pin header is the right amount of integration. |
| Button module | Mechanical; a header carrying 3.3 V, GND and five keys keeps it panel-mountable. The firmware names four keys today. |
| Pump, valve, tube, tee | Off-board by nature. |

The breadboard and jumpers are gone; the board is what replaces them.

## What the board has to get right

* **Two 3.3 V nets, not one.** Call the Super Mini's `3V3` `3V3_SENSE`: it feeds the MPRLS, OLED
  and buttons. Call the buck output `3V3_LOAD`: it feeds only the DRV8833 `VM` and the 1000 µF.
  Joining them puts the pump's dip on the sensor rail, which is the failure the POC parts list
  already calls out.
* **Switch, not bridge.** `AIN2` and `BIN2` to GND; GPIO6 to `AIN1`, GPIO5 to `BIN1`. `nSLEEP` to
  3.3 V. `AISEN`/`BISEN` to GND unless sense resistors are fitted for current limiting. Nothing
  reads `nFAULT`, so leave it open or bring it to a spare pin.
* **The bare IC has no reverse-polarity protection.** Use a keyed connector on the buck output, or
  a keyed or clearly marked header if the buck keeps its 0.1" pins. Add a P-MOSFET if the board
  will be probed live.
* **One star ground.** The driver's GND carries the pump and valve return; join it to the buck and
  Super Mini grounds at one point, and keep the sensor's ground trace off that current path.
* **The PowerPAD.** The `DRV8833PWPR` has a thermal pad and needs copper and vias under it; plan
  for reflow or a hot plate rather than an iron alone.

## What the breadboard let us skip

A breadboard is self-probing and forgiving, so the POC could leave these out. None is needed for
the loop to run, but each is cheaper to add now than to retrofit:

* **Test points** for 5 V, `3V3_SENSE`, `3V3_LOAD`, GND, SDA/SCL and the two driver outputs. The
  breadboard was the probe access; the board has to provide it.
* **A polyfuse footprint** on the 5 V feed. The POC list calls the fuse optional; it is cheap
  insurance once the board is the only thing between the USB port and a miswire.
* **Current limiting** if the DRV8833 module had it. The bare IC needs two sense resistors to keep
  it; with `AISEN`/`BISEN` grounded, the only limit is the chip's own over-current protection.
* **Mounting holes and strain relief** for the tube and the load connectors. Mechanical, but the
  board is where they belong.

## Deferred

* The battery divider: the firmware has the pin and the constants, but no pack is fitted. Check
  the Super Mini variant first — some boards carry a BAT charger and divider, but the GPIO varies
  (GPIO1, GPIO3 and GPIO4 are all seen). If yours has one, use it instead of the external pair.
* Reading the buttons: wired and named, still unread.
* The bare MPRLS and the bare OLED: both are mechanical and panel decisions, not wiring decisions.
