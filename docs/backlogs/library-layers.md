# Library layers

The steps of [laying the library out by layer](../plans/laying-the-library-out-by-layer.md). A
layer gains its namespace behind `inline` first, so the move lands with no call site touched; the
inline is dropped later, one layer per item, when its uses are qualified.

- [x] `core/` declares `gardener::core` inline, and `firmwareversion.h` joins it.
- [x] `machine/` and `storage/` declare `gardener::machine` and `gardener::storage` inline.
- [x] `panel/` and `panel/parts/` declare their namespaces inline, and `BerryScene`, `Registration`
      and `RoutineRegistry` join `gardener::panel::page`. The tree already held this: those three
      were inside `namespace panel::page` all along, and `GestureQueue`, the parts and every page
      name their folder, so no change was needed.
- [x] `routines/bindings/` becomes `bindings/` declaring `gardener::bindings`, with `BerryScene` in it.
- [ ] The three ticks move to `beats/` declaring `gardener::beats`.
- [ ] `storage::SettingKind` takes the document half of a settings row, and `RoutineSettingStore`
      stops naming `panel`.
- [ ] `routines/` and `serial/` declare `gardener::routines` and `gardener::serial` inline.
- [ ] `core` drops its inline and its uses are qualified.
- [ ] `machine` and `storage` drop their inline and their uses are qualified.
- [ ] `panel`, `panel::parts` and `panel::page` drop their inline.
- [ ] `bindings`, `routines` and `beats` drop their inline.
- [ ] The three greps hold: no `routines/` include under `panel/`, no `panel/` include under
      `storage/`, and no namespace that does not end in its folder's path.

## What the next step needs

Nesting a file in a layer namespace moves its *incomplete* types with it: a
`namespace panel { class GestureQueue; }` written inside `namespace gardener { inline namespace beats`
declares `gardener::beats::panel::GestureQueue`, not the panel's. Five headers under `routines/` and
`beats/` declare their incomplete types that way and have to have those blocks hoisted into a
qualified block above the file's namespace before the layer can wrap them. The mechanical pass that
did the first four items is worth rebuilding with that case in hand rather than run again as it
stands.

## Where it stands

`panel/` no longer reaches up into the Berry layer: BerryScene moved to `bindings/`, and a grep for
`#include "routines/` under `src/panel` comes back empty. `storage/RoutineSettingStore.h` still
includes `panel/page/SettingsRows.h`, which the `SettingKind` item above is what removes.

The unit suite was green at 239 cases and 980 assertions after the first two items. It has not been
run since: from the third item onward this machine's linker stopped producing executables, failing
with `collect2.exe: error: ld returned 1 exit status` and no diagnostic, at every output path and
for the untouched tree as well. Everything after that point is compiled by `tools/unit/check.py` and
by `scripts/arduino.py --task build` -- both get as far as the link and report no error before it --
but it is compiled, not run. Run the suite before anything else is built on top of it.
