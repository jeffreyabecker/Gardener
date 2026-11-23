# What does a finished headless board look like on JLCPCB?

One 60 × 40 mm two-layer board, fabricated and assembled by JLCPCB, with nothing on it that is a
module: the ESP32-C3-MINI-1-N4, a USB-C socket that powers it and programmes it in place, a 5 V to
3.3 V LDO, two JST-XH for the pump and the valve, two discrete MOSFETs as their switches, and the
pressure sensor. Headless — no panel and no keys beyond the two that recover a blank chip. Batches
are 5 to 10 boards.

Where each part has to sit on that board, what the layout has to satisfy and what to decide before
it starts are in [pcb_considerations.md](./pcb_considerations.md).

## Parts

JLCPCB loads a basic part for nothing and charges about $3 for each extended part number.

| Part | Part number | LCSC | Class | Qty | Unit | KiCad footprint | For |
| --- | --- | --- | --- | --- | --- | --- | --- |
| ESP32-C3 module | ESP32-C3-MINI-1-N4 | C2838502 | extended | 1 | $3.07 | `Espressif:ESP32-C3-MINI-1` | the controller |
| USB-C socket | TYPE-C-31-M-12 | C165948 | extended | 1 | $0.19 | `Connector_USB:USB_C_Receptacle_HRO_TYPE-C-31-M-12` | power in, programming, console |
| Pressure sensor | WF100E 1BG S6 DA 0.2N4.7 AT | C22388669 | extended | 1 | $1.30 | `EasyEDA:SOP-6_L7.0-W7.0-P2.54-LS10.3-TL-CW` | −100…0 kPa, analogue out |
| JST-XH connector | XH-2P | C688873 | extended | 2 | $0.07 | `EasyEDA:CONN-SMD_XH-2P` | pump and valve leads |
| ESD array | TPUSBLC6-4SC6 | C558419 | extended | 1 | $0.04 | `Package_TO_SOT_SMD:SOT-23-6` | ESD on D+ and D− |
| Polyfuse | BSMD1206-110-16V | C2803346 | extended | 1 | $0.06 | `Fuse:Fuse_1206_3216Metric` | load-rail fuse |
| N-channel MOSFET | AO3400A | C20917 | basic | 2 | $0.08 | `Package_TO_SOT_SMD:SOT-23` | low-side switch per load |
| Schottky diode | 1N5819WS | C191023 | basic | 2 | $0.01 | `Diode_SMD:D_SOD-323` | flyback across each coil |
| LDO | AMS1117-3.3 | C6186 | basic | 1 | $0.20 | `Package_TO_SOT_SMD:SOT-223-3_TabPin2` | 5 V to 3.3 V |
| TVS diode | SMBJ6.5CA | C7420377 | basic | 1 | $0.04 | `Diode_SMD:D_SMB` | VBUS clamp at the socket |
| Capacitor 10 µF 0805 25 V | CL21A106KAYNNNE | C15850 | basic | 2 | $0.03 | `Capacitor_SMD:C_0805_2012Metric` | VBUS, 3V3 at the module |
| Capacitor 100 µF 1210 10 V | CL32A107MPVNNNE | C23742 | extended | 1 | $0.28 | `Capacitor_SMD:C_1210_3225Metric` | load reservoir |
| Capacitor 100 nF 0402 16 V | CL05B104KO5NNNC | C1525 | basic | 4 | $0.003 | `Capacitor_SMD:C_0402_1005Metric` | module, LDO in, LDO out, sensor |
| Capacitor 1 µF 0603 50 V | CL10A105KB8NNNC | C15849 | basic | 2 | $0.01 | `Capacitor_SMD:C_0603_1608Metric` | EN's RC, sensor supply |
| Capacitor 1 nF 0603 50 V | CL10B102KB8NNNC | C1588 | basic | 1 | $0.006 | `Capacitor_SMD:C_0603_1608Metric` | sensor output filter |
| Resistor 10 kΩ 0603 | 0603WAF1002T5E | C25804 | basic | 7 | $0.002 | `Resistor_SMD:R_0603_1608Metric` | gate pulldowns, EN, GPIO2, GPIO8, divider |
| Resistor 5.1 kΩ 0603 | 0603WAF5101T5E | C23186 | basic | 2 | $0.002 | `Resistor_SMD:R_0603_1608Metric` | CC1, CC2 |
| Resistor 22 Ω 0603 | 0603WAF220JT5E | C23345 | basic | 2 | $0.002 | `Resistor_SMD:R_0603_1608Metric` | USB series |
| Resistor 100 Ω 0603 | 0603WAF1000T5E | C22775 | basic | 2 | $0.002 | `Resistor_SMD:R_0603_1608Metric` | gates |
| Resistor 1 kΩ 0603 | 0603WAF1001T5E | C21190 | basic | 1 | $0.002 | `Resistor_SMD:R_0603_1608Metric` | LED |
| Tact switch | TS-1187A-B-A-B | C318884 | basic | 2 | $0.02 | `EasyEDA:SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5` | BOOT and RESET |
| LED | NCD0805R1 | C84256 | basic | 1 | $0.01 | `LED_SMD:LED_0805_2012Metric` | power |
| Header, 0.1" 3-pin | — | — | — | 1 | — | `Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical` | UART0 console, unpopulated |

**$5.70 a board in parts**; the seven extended numbers add about $21 of loading fees an order, plus
the stencil and setup, so a batch lands near **$9–10 a board**.

## Notes on specific parts

- **ESP32-C3-MINI-1-N4** — the antenna keepout is at one end and has to be at a board edge; whether
  the module overhangs it is decided in the layout notes.
- **WF100E** — 0.2–4.7 V analogue output, halved by two 10 kΩ into the C3's 2.5 V ADC input; 1 nF
  from output to ground.
- **TPUSBLC6-4SC6** — 5 V standoff at 0.35 pF; the basic TVS parts clamp at 10.5–11.2 V, too high
  for a 3.3 V pair.
- **AMS1117-3.3** — pin 1 ground, pin 2 the tab and the output, pin 3 in.
