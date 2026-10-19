# Parking lot

Work that has been considered and deliberately deferred. An entry records what would change, what
it would buy, and what has to be true before it is worth doing. Nothing here is a commitment: an
item leaves when it is done and the result is in the tree, or when it is dropped and the reason is
in git.

## Firmware size

Measured on 2026-10-19 with `-Task build`: the image is 566,997 bytes, 43% of the 1,310,720-byte app
slot. An empty sketch with the same FQBN builds to 298,069 bytes (22%), so the firmware adds 269 KB
on top of the platform floor. The two entries below are the size levers that were identified and
not taken.

### Revisit the partition scheme

**What.** Change the pinned `PartitionScheme` in [scripts/arduino.ps1](../scripts/arduino.ps1) from
`default` — two 1.25 MB OTA slots and a 1.375 MB LittleFS partition — to `huge_app`: one 3 MB app
partition and 896 KB of LittleFS.

**What it buys.** The same image reports ~18% instead of 43%. The second app slot stops being
reserved for an update path that does not exist: uploads are over USB and there is no `Update` or
OTA code in the firmware.

**What it costs.** There is no OTA slot if that changes. The data partition shrinks from 1.375 MB to
896 KB; the current `firmware/gardener/data` tree is 33 KB, so it fits with room to spare.
`-Task data` reads the `spiffs` row from the table the build pins, so the script follows the change
without edits — but the offset and size move, so the first upload after the switch has to write the
data image again.

**Before doing it.** Confirm OTA is not planned, re-check the size of `data/`, and update the build
note in [README.md](../README.md) that says the scheme is pinned.

### Try `-fno-exceptions`

**What.** Add `-fno-exceptions` to the build properties for the code this project compiles — the
sketch, VacuumCore, RLEBitmap and the third-party C++ libraries — without editing vendored sources.

**What it buys.** The image carries 36 KB of `.eh_frame` and `.gcc_except_table`, the metadata for
C++ unwinding. About 17 KB of it belongs to objects this build compiles: VacuumCore 10.0 KB,
Adafruit 3.6 KB, LittleFS/FS 2.0 KB, Wire/SPI 1.3 KB. The rest is in `core.a`, libstdc++ and the
IDF libraries, which are prebuilt. The exact saving needs a build, because the metadata is
per-function and depends on what the linker keeps.

**What it costs.** No exception unwinding in project code, so a failed `new` aborts instead of
throwing. There are no `try`, `catch` or `throw` sites today, and allocation failure is already
treated as "this part is off" rather than recoverable. Any vendored C++ added later would have to
make the same choice.

**Before doing it.** Build through `-Task build` and compare the reported size; flash and monitor
once to confirm the failure paths still behave. This does not touch Berry (C) and does not change
the newlib printf cost, so do not expect the two levers to add up.
