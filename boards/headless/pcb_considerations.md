# PCB design considerations for the headless board

The board is 60 × 40 mm, two layers, 1.6 mm, every part on the top — one assembly side, one stencil,
one JLCPCB setup — with four M2 holes in the corners. None of it is placed yet. The 42 footprints in
`headless.kicad_pcb` sit in a working spread about 188 × 137 mm across, the outline drawn there is a
71.5 × 43 mm rectangle that no part sits in, and DRC reports 128 unconnected pads, so what follows
is what a layout has to satisfy rather than a description of one that exists.

## The frame

- The outline is 60 × 40 mm with 1.5 mm corner radii. JLC's router does 1 mm, so 1.5 is comfortable
  and keeps the corners clear of the holes.
- Four M2 mounting holes, 2.2 mm drill, centred 3.5 mm in from each edge. Plain rather than plated:
  a plastic standoff cannot ground anything, and a plated hole only earns its keep if the screw
  lands on a metal boss.
- Keep 1 mm of no copper around each hole and 3 mm of component-free space, so the 3.8 mm screw head
  and a driver fit. Nothing may sit on a hole — not the module, not a connector, not the antenna
  strip.
- That leaves about 53 × 33 mm of usable interior once the four keepouts are taken out, which is
  enough for the arrangement below.
- The Edge.Cuts has to be redrawn to that frame before the checks mean anything: copper-to-edge,
  courtyard overlap and the unconnected count all use it, and the project already asks 0.5 mm from
  copper to edge.

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
forbids tracks, vias, pads, copper pour and other footprints on every copper layer. Keep your own
stitching vias out of the strip as well — the zone stops the pour, not you.

### Overhang, or keep the copper away

Espressif's layout guidance is to place the module's antenna outside the base board with the feed
point close to the board edge. Where it cannot go outside, it asks for the board to be cut away on
both sides of the antenna and below it, not merely cleared of copper, and it warns against a module
in the middle of the board with clearance hollowed on all four sides. It also asks 15 mm of clear
air inside the housing in every direction.

The difference between the two versions is the FR4 under the antenna. The keepout removes the
copper, not the dielectric, and the dielectric both detunes the antenna and absorbs some of what it
radiates. So a keepout alone is the weaker option, and by how much is a decibel or two rather than
a working link and a dead one.

- Overhanging by 2–3 mm is the version the datasheet wants, and it costs nothing mechanically here:
  the module has no pads at that end, so the part that hangs over is plastic module PCB. It does
  grow the finished assembly to about 62–63 × 40, which is the number the box has to take.
- If 60 × 40 is the box's inside dimension rather than the board's, the module stays flush: its end
  on the edge, the keepout clear of copper and vias on both layers, and the nearest metal 15 mm
  away wherever the box allows. For a board this size inside a box, that is the default, and the
  overhang is the upgrade if the enclosure has the length.
- The datasheet's fallback — flush, with the board routed away either side of the antenna — buys
  little at 40 mm: the antenna is 13.2 mm wide, so the slots are 13 mm long each side and the
  material beside the antenna is already outside the keepout.
- The enclosure is the real antenna environment. No metal lid, no foil, no pump body or valve coil
  within 15 mm of that end, and route the two JST leads away from it rather than past it.
- Fifteen millimetres each side of the antenna does not fit on a 40 mm edge, so the datasheet's
  clearance cannot be met exactly. Measure the range in the assembled box rather than assuming it.

Whatever is chosen, the rest holds:

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
the valve about 180 mA, so the pair on 5 V is about 1 A with a surge over it — and F1 is a 1.1 A
hold part fed from whatever USB port is on the other end, so a weak port may limit before the fuse
does.

Each loop wants to be small, and the two want their own return:

    +5V ── F1 ── /VLOAD ──┬── C3 ── GND
                          ├── J2 pin 1 ── [pump] ── J2 pin 2 ── Q1 drain
                          └── D1 cathode ───────────────── Q1 drain (anode)

- D1 and D2 go across their own connector, at the connector, not at the FET. The diode's job is the
  winding's stored current, and the loop it makes should be the smallest on the board.
- The reservoir is one 100 µF 1210 at C3: put it between the two connectors, and put each
  connector's ground and its FET's source on the same copper.
- The drain traces (`/PUMP_DRAIN`, `/VALVE_DRAIN`) carry the full load current and switch hard:
  0.5 mm wide at least, short, and not under the module or beside `/SENSOR_ADC`.
- R12, R13 and R4, R5 belong at their FET's gate. `/PUMP_CTRL` and `/VALVE_CTRL` are a logic signal
  and can be thin and long; the gate node cannot.
- The mouth of both XH footprints is the −y end, the side the two signal pads run out from. In the
  present placement J2 and J3 have their mouths facing the FETs, so a cable would leave across the
  board; they need a half turn before they meet an edge.
- Silk the pin 1 end of both connectors. A reversed plug puts the pump across `/VLOAD` and the
  drain, which the FET's body diode blocks, so nothing burns — the pump just does not run, and that
  is a five-minute mystery with nothing on the board to explain it.

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
  of a hundred square millimetres.
- The 3.3 V rail's bulk is C2's 10 µF at the module, which is fine for the module but under what the
  AMS1117 datasheet asks of its output: 22 µF, and it says so for stability, not inrush. Either put
  a second 10 µF at the output pin or accept a rail that is out of spec and check it with a scope.
- The netlist has one 100 nF on 3.3 V and three on 5 V, while the parts table names four jobs —
  module, LDO in, LDO out, sensor. Settle which of them sits at the regulator's output before they
  are placed.
- LED1 and R14 are a power indicator on 3.3 V: put them where the board's state can be read with
  the lid on.
- SW1 and SW2 are a short from the pin to ground and nothing else, so their ground should be the
  nearest plane, and their position is about fingers and poking, not electrically about anything.
- J4 is unpopulated and through-hole, so it can sit anywhere a 2.5 mm pin can be reached; keep it
  out of the USB plug's overmold.
- 0402 at the module, 0603 and 0805 elsewhere, 1210 for the reservoir, all on the top.

## Board-level practices

- **Ground.** B.Cu is one solid pour, F.Cu filled where the parts leave room, and the two stitched
  together. Stitch every few millimetres around the module's perimeter and the USB shell, keep the
  stitching out of the antenna strip, and make sure each FET's source sits in copper rather than on
  a single spoke, because the pour is the return for about an amp.
- **Netclasses.** All Default at 0.2 mm. `/VLOAD` and `+5V` want 0.5 mm, the drains want 0.5 mm,
  and the USB pair wants its own 0.2 mm / 0.25 mm gap. Set them before routing.
- **Vias.** 0.6 / 0.3 mm is the default and the smallest the rules allow is 0.5 / 0.3, so there is
  no need to go below the default anywhere.
- **Silkscreen.** The project defaults are 1 mm text at 0.1 mm thickness; JLC prints 0.15 mm line
  width, so raise the thickness before plotting and keep text off pads and mask openings.
- **Mask and finish.** 0.05 mm mask expansion and lead-free HASL; nothing here has a pitch that
  wants ENIG.
- **Assembly.** One side, one stencil. J2 and J3 carry the through-hole attribute in their footprint
  despite having no holes in their pads; set them to SMD so the position file lists them like every
  other part on the side.
- **Panel.** JLC adds rails for assembly, and the three edges that overhang — USB, JSTs and the
  module if it overhangs — should not be the railed ones, or the panel has to be tabbed around them.
  Settle it when ordering.
- **Fiducials and test points.** None. JLC puts fiducials on the panel, and the UART header is the
  console. At this size and batch that is defensible, and the cost is that a bare board cannot be
  checked before the module is on it.

## Three courtyards that understate their parts

The three EasyEDA footprints were converted from library data and their courtyards do not contain
their pads: the XH-2P's covers −3.03 to 5.57 mm in y while its signal pads reach −6.06, and the
SOP-6's covers ±3.5 while its pads reach ±5.63. The switch's is the same story, ±2.55 against pads
at ±3.5. DRC's courtyard test will therefore not catch a part stood on the pad side of any of them,
so keep that clearance by eye, or widen the courtyards before the layout starts.

## Before the layout starts

- **The outline** in the board file is a 71.5 × 43 mm rectangle, not the 60 × 40 frame above, and no
  part sits in it. It has to be redrawn with the 1.5 mm corners and the four holes before anything
  is arranged inside it.
- **J5** is a 1 × 8 header in the schematic and on the board, with five GPIOs, 5 V, ground and the
  two strap junctions on it, and it is not in the parts table. Nothing headless uses it. Either drop
  it or add it to the parts list and find it 20 mm of edge: it is the largest part after the module,
  and it decides the right-hand end of the board.
- **The mounting holes** need their hardware decided with them: M2 machine screws into printed
  bosses, or M2 self-tappers, which want a different drilling. The 2.2 mm above suits a machine
  screw and a nut.
- **The 60 mm length is the box's, not just the board's**, because an overhanging module adds two or
  three millimetres to what has to fit.
