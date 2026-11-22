# Espressif library, vendored

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
