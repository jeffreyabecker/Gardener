# Where the parts have to go on the headless board

The board is 55 × 35 mm, two layers, 1.6 mm, and every part is on the top: one assembly side, one
stencil, one JLCPCB setup. None of it is placed yet. The 47 footprints in `headless.kicad_pcb` sit
in a working spread about 188 × 137 mm across, there is no Edge.Cuts outline, and DRC reports the
malformed outline and 138 unconnected pads, so what follows is what a layout has to satisfy rather
than a description of one that exists.

The order is the order the layout is easiest to do in: the parts that are pinned to an edge, the
module's antenna, the two switching loops, the analogue pair, then everything that can move last.

## The edges decide the rest

Four parts cannot go anywhere, and the rest is arranged around them.

| Part | Where | Because |
| --- | --- | --- |
| U1, antenna end | any edge | the antenna strip sits over the edge, clear of copper |
| J1 USB-C | mouth over an edge | the plug has to seat; the shell stands proud of the board |
| J2, J3 XH-2P | mouth over an edge | the cable leaves sideways, and the housing is 7 mm tall |
| SW1, SW2 | an edge, or a lid hole | recovery buttons; reachable with the board boxed |

A USB cable brings an overmold about 12 mm wide and 10 mm deep that lies against the board, and each
XH housing stands 8.6 × 7 mm above it. So the socket wants a long edge to itself and the JSTs the
opposite one, or the two cables leave the same side of the box into each other. The lid then has to
clear the XH bodies where they sit, and nothing tall — the 3-pin UART header, a switch — should end
up beside the USB socket where the plug's body lands.

One arrangement that fits all of that: the module in the top-left with its antenna over the left
short edge, U4 to its right where pin 12 is, J1 in the bottom edge under the module's USB pins, J2
and J3 on the top edge, U3 between them, and the switches, J4 and the LED at the right end.

## The module, and the strip nothing may enter

The ESP32-C3-MINI-1's footprint is 13.6 × 17.0 mm, and its antenna lives at the local −y end inside
a keepout 13.2 mm wide and 5.4 mm deep (local y −8.3 to −2.9). The keepout zone in the footprint
forbids tracks, vias, pads, copper pour and other footprints on every copper layer, which is the
minimum the datasheet asks: the antenna end must be at the board edge, may overhang it, and must not
be covered by copper, a part or a metal enclosure wall. Keep your stitching vias out of the strip as
well — the zone stops the pour, not you.

- Place the module so the −y end hangs 1–2 mm past the Edge.Cuts, and nothing else on the board is
  within the keepout, including the bottom layer.
- Pads 26/27 are the USB pair and come out of the +x side; the USB socket belongs on the edge that
  side faces, not on the antenna's.
- Pin 3 is 3.3 V: C2's 10 µF and C9's 100 nF go within 3 mm of it, not at the regulator.
- Pin 8 is EN: R1's 10 k and C13's 1 µF go at the pin, and SW2 hangs off the same node.
- Pins 5 and 22 are the straps: R2 and R3 at the pins, short, and away from the drain nodes.
- Every ground pad plus the 3 × 3 grid of pad 49 needs a via to the bottom pour, so the module's
  ground is the plane and not a track.

## The USB socket and the pair

J1 is a 16-pad receptacle with four through-hole stakes at the local ±x sides and the mouth at −y.
Set the Edge.Cuts so the board ends between the front pads' ends (local y −4.77) and the courtyard
front (−5.27) — about −5.0: the pads keep a quarter millimetre to the edge and the shell stands
some 0.3 mm proud of it, which is what the plug's overmold needs. The four SH stakes are
through-hole, so they must land inside the board and keep the back side clear under them.

- D3's SMBJ6.5CA sits across VBUS and GND within about 5 mm of the socket, with its ground end on
  the same copper as the shield stakes.
- U2's TPUSBLC6 straddles the pair right behind the socket: its pins 1 and 3 are the D− and D+ side,
  pin 2 is ground, pin 5 is VBUS. It only does its job if the ground return is shorter than the
  pair.
- R10 and R11, the 22 Ω in series, go at the module end. They are there to damp the trace, so put
  them where the trace is longest, which is next to pins 26 and 27.
- R8 and R9, 5.1 k on CC1 and CC2, can be anywhere near the socket.
- C1's 10 µF is the socket's VBUS reservoir, and F1 goes after it, so the logic rail stays up if the
  fuse opens.

Run `/USB_DP*` and `/USB_DN*` as one pair on one layer with no vias, as short as the board allows.
Full speed is not impedance critical, so the pair's job here is to stay away from the drains and to
keep the two nets the same length, not to hit 90 Ω on a 1.6 mm two-layer stack.

## The pump and the valve loops

The loads hang on the fused 5 V rail, not on 3.3 V: `/VLOAD` is F1's output and feeds J2 pin 1, J3
pin 1 and the two flyback cathodes. Q1 and Q2 switch the low side from `/PUMP_CTRL` and
`/VALVE_CTRL` through 100 Ω, with 10 k holding each gate down. The pump is rated 0.45 A at 3.7 V and
the valve about 180 mA, so the pair on 5 V is about 1 A with a surge over it.

Each loop wants to be small, and the two want their own return:

    +5V ── F1 ── /VLOAD ──┬── C3..C8 ── GND
                          ├── J2 pin 1 ── [pump] ── J2 pin 2 ── Q1 drain
                          └── D1 cathode ───────────────── Q1 drain (anode)

- D1 and D2 go across their own connector, at the connector, not at the FET. The diode's job is the
  winding's stored current, and the loop it makes should be the smallest on the board.
- The six 10 µF of C3–C8 are the reservoir for both loads: put them between the two connectors, and
  put each connector's ground and its FET's source on the same copper.
- The drain traces (`/PUMP_DRAIN`, `/VALVE_DRAIN`) carry the full load current and switch hard:
  0.5 mm wide at least, short, and not under the module or beside `/SENSOR_ADC`.
- R12, R13 and R4, R5 belong at their FET's gate. `/PUMP_CTRL` and `/VALVE_CTRL` are a logic signal
  and can be thin and long; the gate node cannot.
- The mouth of both XH footprints is the −y end, the side the two signal pads run out from. In the
  present placement J2 and J3 have their mouths facing the FETs, so a cable would leave across the
  board; they need a half turn before they meet an edge.
- Silk the pin 1 end of both connectors. `/VLOAD` on pin 1 and the drain on pin 2 is not something
  to get wrong twice.

## The sensor's front end

U4 runs from 5 V and puts 0.2–4.7 V on `/SENSOR_RAW`; R6 and R7 halve it into `/SENSOR_ADC`, and
C15's 1 nF is the filter. Pin 12 is the ADC, and it is at the end of the module away from the
antenna, so the sensor and its divider belong at that end.

- R6, R7 and C15 go at pin 12 rather than at the sensor. The filter's job is the ADC's input, and a
  10 k divider into a 1 nF cap is a network that wants to be short.
- The sensor's own 1 µF and 100 nF go at its VDD pin.
- Keep `/SENSOR_RAW` and `/SENSOR_ADC` out of the drain traces' company and off the pump's ground
  return; the sensor's ground joins the module's ground, not Q1's and Q2's source path.
- The port is the part's one mechanical feature. Decide from the datasheet which face the barb comes
  out of before the rotation is fixed, then leave room to push a 2.5 mm ID tube on and dress it
  without a kink. Nothing tall should sit next to it.

## What is left

- U3's AMS1117 is the logic rail only — the pump and the valve are on 5 V — but the module's WiFi
  peaks make it the one part here that needs copper for heat: give the tab (pin 2, also VOUT) a pour
  of a hundred square millimetres. The rail's bulk is C2's 10 µF at the module's pin 3, so the
  regulator wants one of the 100 nF at its input and one at its output; the netlist puts one 100 nF
  on 3.3 V and three on 5 V while the parts table names four jobs, so settle which cap lands where
  before they are placed.
- LED1 and R14 are a power indicator on 3.3 V: put them where the board's state can be read with
  the lid on.
- SW1 and SW2 are a short from the pin to ground and nothing else, so their ground should be the
  nearest plane, and their position is about fingers and poking, not electrically about anything.
- J4 is unpopulated and through-hole, so it can sit anywhere a 2.5 mm pin can be reached; keep it
  out of the USB plug's overmold.
- 0402 at the module, 0603 and 0805 elsewhere, all on the top.

## Three courtyards that understate their parts

The three EasyEDA footprints were converted from library data and their courtyards do not contain
their pads: the XH-2P's covers −3.03 to 5.57 mm in y while its signal pads reach −6.06, and the
SOP-6's covers ±3.5 while its pads reach ±5.63. The switch's is the same story, ±2.55 against pads
at ±3.5. DRC's courtyard test will therefore not catch a part stood on the pad side of any of them,
so keep that clearance by eye, or widen the courtyards before the layout starts.

## Before the layout starts

- **J5** is a 1 × 8 header in the schematic and on the board, with five GPIOs, 5 V, ground and the
  two strap junctions on it, and it is not in the parts table. Nothing headless uses it. Either drop
  it or add it to the parts list and find it 20 mm of edge: it is the largest part after the module,
  and it decides the right-hand end of the board.
- **Mounting holes** do not exist yet, and the board has to be fixed in a box with two tubes on it.
  Decide the hole pattern before the zones above are fixed: two or four M2 holes cost pour area and
  cannot go near the antenna.
- **Netclasses** are all Default at 0.2 mm. `/VLOAD` and `+5V` want 0.5 mm, the drains want 0.5 mm,
  and the USB pair wants its own 0.2 mm / 0.25 mm. Set them before routing rather than after.
- **No test points and no fiducials.** The UART header is the console, so that is defensible; the
  cost is that a bare board cannot be checked before the module is on it.
