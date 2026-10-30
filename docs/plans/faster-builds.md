# Faster builds

Two thirds of the firmware compile is code that does not change when the firmware changes: NimBLE is
189 of 354 translation units and `libraries/berry` is 41. arduino-cli already caches their object
files — what destroys the cache is the wrapper's build options, which change on every rig retune, and
arduino-cli cleaning the build path when they do. This plan gives both libraries prebuilt archives
that arduino-cli skips, and takes the rig numbers out of the compiler flags so that retuning a rig
stops costing a full rebuild.

## What is wrong now, and where

A full build is 354 translation units and about 3m45s here, measured from `build.options.json` being
written to the last object landing in `firmware/gardener/build/compile`. By source, 189 of those
units are NimBLE-Arduino 2.5.1 (its whole `src`), 41 are `libraries/berry`, about 70 are the
first-party libraries under `libraries/`, and the remaining 55 are the ESP32 core and the libraries
it bundles — `Wire`, `FS`, `LittleFS`.

The objects are already cached. `scripts/arduino.py` keeps `build/compile` between runs precisely so
that arduino-cli can skip a file whose object and `.d` are newer than its sources, and the comment
over `COMPILE_DIR` says so. What that cache does not survive is a change of *build options*:
arduino-cli compares `build.options.json` on every run and cleans the build path when any field
differs — `additionalFiles`, `compiler.optimization_flags`, `customBuildProperties`, `fqbn`,
`hardwareFolders`, `otherLibrariesFolders`, `sketchLocation`.

Rig numbers arrive through that path. `--define PUMP_FLOW_LPM=2.0f` becomes part of
`--build-property build.defines=…` (`build_defines` in `scripts/arduino.py`), so it lands in
`customBuildProperties` and wipes the build path; it also expands into `build.extra_flags` in the
ESP32 platform (`platform.txt:102`), which every compile recipe carries, so it is a flag change on
all 354 units as well. Nothing about `PUMP_FLOW_LPM` can reach NimBLE: NimBLE includes neither
`Machine.h` nor anything that includes it. The 189 NimBLE objects a retune discards are discarded for
nothing, and so are Berry's 41.

The two libraries also sit in different places. Berry is vendored at `libraries/berry`, so its
`library.properties` is the repository's to change. NimBLE is installed by `--task libs` into
`Documents\Arduino\libraries\NimBLE-Arduino`, outside the tree, and is replaced there by
`arduino-cli lib install` or a version bump.

arduino-cli 1.4.1 offers nothing of its own: `compile` takes `--build-path`, `--build-property`,
`--clean`, `-j`, `--libraries`, `--library` and `--only-compilation-database`, and nothing that
reuses objects across build paths.

## The shape of the change

**A prebuilt archive per library.** The library specification's `precompiled=full` is the mechanism
for this: an archive in `src/{build.mcu}/` means the library's sources are not compiled and the
archive is linked instead, and if no archive exists for the board the sources compile as a fallback.
The fallback is what makes the design safe — a fresh clone, a new toolchain or a deleted archive is
slow, never broken. For this board `build.mcu` is `esp32s3` (`boards.txt:555`), so the archives are
`libraries/berry/src/esp32s3/libberry.a` and, in the staged copy below, `src/esp32s3/libNimBLE.a`.
Whether the platform also needs `ldflags=-l<name>` in `library.properties` is one of the unknowns the
spike settles.

**A staged copy of NimBLE the repository owns.** The archive has to sit inside a `library.properties`
the build answers to, and the installed copy is not the repository's to edit. `--task libs` stages
the pinned version into `firmware/gardener/build/third_party/NimBLE-Arduino/` — gitignored,
regenerated from the pinned version — and `compile_sketch()` passes a second `--libraries` for it,
which arduino-cli accepts more than once. The copy in `Documents` stays as the source of the staging,
not as the library the sketch compiles against; `otherLibrariesFolders` stops depending on whatever
else that folder holds, which is a small reproducibility gain of its own.

**Packing from objects that were just compiled.** After a build in which a precompiled library's
sources did compile, `build/compile/libraries/<Name>/` holds exactly the objects a correct archive
needs, and `gcc-ar cr` over them cannot drift from the flags because arduino-cli produced them with
those flags. Beside each archive, `archive.json` records the toolchain version, the compile-flag
string and a digest of every file under the library. The wrapper recomputes all three before it
invokes arduino-cli and **deletes** the archive when any of them differ, so a stale archive is never
linked; that build then takes the fallback path and re-packs at the end. `--task archive` does the
same thing on demand, using `--only-compilation-database` to get the per-file commands without
compiling anything.

**Rig numbers in a generated header.** `--define` writes `firmware/gardener/rig_options.h`
(gitignored) and stops being passed to arduino-cli. `Machine.h` includes it behind
`#if __has_include("rig_options.h")`, above its existing `#ifndef` defaults. The sketch folder is
already on the include path for every translation unit (`-I{build.source.path}` in the platform's
`compiler.cpreprocessor.flags`), so the flags do not change across rigs; the defaults stay in
`Machine.h`, so a tree without the header, the host builds under `tools/` and a build made outside
the wrapper all behave exactly as today.

**One build path per configuration.** `COMPILE_DIR` gains a short hash of the FQBN, `build.defines`
and the core version, so switching a board or a panel no longer destroys the other configuration's
97 MB of objects. `--task clean` removes all of them; `build/littlefs.bin` stays where it is.

What remains after this is the core's ~55 units and the first-party ~70 — what an edit to the
firmware genuinely touches.

### Files it touches

| File | Change |
| --- | --- |
| `scripts/arduino.py` | rig header, staged libraries, `--task archive`, per-configuration build path, verify-then-pack around the existing `compile_sketch` |
| `scripts/archive.py` | new: pack, manifest verify, compile from `--only-compilation-database` |
| `libraries/Gardener/src/machine/Machine.h` | `__has_include("rig_options.h")` above the defaults |
| `libraries/berry/library.properties` | `precompiled=full`, and `ldflags=-lberry` if the spike says so |
| `firmware/gardener/build/third_party/NimBLE-Arduino/library.properties` | the same, written by `--task libs` |
| `.gitignore` | `firmware/gardener/rig_options.h`, the `src/esp32s3/` archives, `build/third_party/` |
| `README.md`, `docs/guides/poc-parts.md` | the archive cache and how to refresh it; rig numbers still come from `--define` |

## The alternatives, and why not

**ccache in front of the compiler.** The standard answer, and it needs no library-properties surgery:
a directory named as `compiler.path` holding masquerade copies of `ccache.exe` for
`xtensa-esp32s3-elf-gcc`/`g++` and hardlinks for `gcc-ar`/`size`, silent when ccache is not
installed. It was set aside because it caches the flags with the objects: a rig retune still misses
on every unit and NimBLE still recompiles, so the rig-header change is needed either way, and with it
the archives remove the work instead of replaying it. It remains the fallback if `precompiled=full`
turns out not to work on this platform, and the two combine without undoing each other.

**A build path per rig, keeping the flags.** Cheaper than the header, and it does stop one retune
from destroying another, but it fixes nothing for `--task clean`, a core upgrade or a fresh clone,
and multiplies 97 MB per rig.

**Vendoring NimBLE into `libraries/`.** It gives the repository the `library.properties` and the
archive directory outright, at the price of 189 third-party files in the tree. Staging reaches the
same ownership without carrying code the project does not own.

**Committing the archives.** A binary that must be rebuilt on every core or toolchain change, in a
repository whose target is a chip rather than a package. Both are gitignored and regenerated; the
fallback path makes their absence a cost, not a failure.

**ESP-IDF with CMake and Ninja.** A genuine build-system improvement — a prebuilt core shared between
projects, first-class ccache, options in a CMake cache instead of a hash that wipes a directory — and
a migration of the sketch, the library layout and the data-image task for a three-minute problem.

**Amalgamating Berry's sources.** Berry is vendored unmodified apart from `berry_conf.h` and
`be_port.c`; folding it into fewer translation units is a fork of the vendored library.

## The entry points it has to hold on

- `scripts/arduino.py`: `build`, `upload`, `data`, `libs`, `clean`, `monitor`. `upload` shares
  `compile_sketch`, so the packing has to hang off `compile_sketch` rather than off `build`, or
  `--task upload` builds without it.
- `--task clean` keeps the archives, because they live beside the libraries rather than in `build/`;
  `--task libs` must invalidate and re-stage rather than keep an old copy.
- The host builds — `tools/unit`, `tools/panel`, `tools/gfx/check.py` — read the same library sources
  without arduino-cli, so an archive appearing beside them must change nothing for them.
- `build_opt.h` must stay out of this. The platform feeds it to every recipe as a response file
  (`@{build.opt.path}`), which is the same full-rebuild shape as `build.defines`; rig numbers going
  there would keep the wipe and lose the win.
- `precompiled=full` on `esp32:esp32:esp32s3` is the one unknown: whether the platform accepts an
  archive at all, whether `src/{mcu}` needs `ldflags` to link it, and whether the sources really are
  skipped are settled by a throwaway library before any of the above is written.

## How it is known to have worked

- **The spike:** a two-file library with `precompiled=full` and an archive in `src/esp32s3/`,
  included from the sketch. `--verbose` shows no compile line for its source and a symbol from the
  archive appears in the `.elf`.
- A repeat build once the archives exist: `--verbose` carries no `NimBLE-Arduino` or `berry` compile
  lines, and the `.elf` links. The same after `--task clean` — the archives are outside `build/`.
- A rig retune, `--define PUMP_FLOW_LPM=2.0f`: `build.options.json` is unchanged, every NimBLE object
  keeps its mtime, and only the `Machine.h` includers and their dependents recompile.
- Deleting an `archive.json`, or editing a Berry source: the next build drops that archive, compiles
  the sources and re-packs. The staleness path observed, not assumed.
- On the device: `--task upload`, the panel comes up and the `[E]` lines appear on the serial
  console. That is the only check that an archived BLE stack actually runs; a wrong archive links
  silently, and this is where it would show.
