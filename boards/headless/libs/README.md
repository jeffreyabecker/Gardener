# Vendored libraries

## Espressif

Two files, taken from [espressif/kicad-libraries](https://github.com/espressif/kicad-libraries) at
commit `d5b68fd9269c639b3a60681e00b91df33b4db4a3`:

- `Espressif.kicad_sym` — the `ESP32-C3-MINI-1` symbol, extracted from `symbols/Espressif.kicad_sym`.
- `Espressif.pretty/ESP32-C3-MINI-1.kicad_mod` — the footprint, unchanged except for the 3D model
  path, which now points at `${KIPRJMOD}/libs/Espressif.3dshapes/ESP32-C3-MINI-1.STEP`.

Licensed CC BY-SA 4.0, the same terms as KiCad's own libraries: designs made with them are not
derivative works, by the waiver in the license. The notice is at
[the repository's LICENSE.md](https://github.com/espressif/kicad-libraries/blob/main/LICENSE.md).

The STEP model is not vendored — it is 7.5 MB. Drop `ESP32-C3-MINI-1.STEP` into
`libs/Espressif.3dshapes/` to see the module in the 3D viewer.

## EasyEDA

`EasyEDA.kicad_sym` and `EasyEDA.pretty` are converted from the EasyEDA library data with
[easyeda2kicad](https://github.com/uPesy/easyeda2kicad) 1.0.1:

| LCSC | Symbol | Footprint |
| --- | --- | --- |
| C22388669 | `WF100E1BGS6DA0.2N4.7AT` | `SOP-6_L7.0-W7.0-P2.54-LS10.3-TL-CW` |
| C688873 | `XH-2P` | `CONN-SMD_XH-2P` |
| C318884 | `TS-1187A-B-A-B` | `SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5` |

The connector's symbol was renamed from the catalogue's `XH-2P卧贴` to `XH-2P`; the footprint names
are the generator's, which spell out the geometry. The sensor's land pattern is 2.54 mm pitch in a
7 × 7 mm body, which is why no standard KiCad `SO-6` fits it.
