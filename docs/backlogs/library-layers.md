# Library layers

The steps of [laying the library out by layer](../plans/laying-the-library-out-by-layer.md). A
layer gains its namespace behind `inline` first, so the move lands with no call site touched; the
inline is dropped later, one layer per item, when its uses are qualified.

- [x] `core/` declares `gardener::core` inline, and `firmwareversion.h` joins it.
- [x] `machine/` and `storage/` declare `gardener::machine` and `gardener::storage` inline.
- [ ] `panel/` and `panel/parts/` declare their namespaces inline, and `BerryScene`, `Registration`
      and `RoutineRegistry` join `gardener::panel::page`.
- [ ] `routines/bindings/` becomes `bindings/` declaring `gardener::bindings`, with `BerryScene` in it.
- [ ] The three ticks move to `beats/` declaring `gardener::beats`.
- [ ] `storage::SettingKind` takes the document half of a settings row, and `RoutineSettingStore`
      stops naming `panel`.
- [ ] `core` drops its inline and its uses are qualified.
- [ ] `machine` and `storage` drop their inline and their uses are qualified.
- [ ] `panel`, `panel::parts` and `panel::page` drop their inline.
- [ ] `bindings`, `routines` and `beats` drop their inline.
- [ ] The three greps hold: no `routines/` include under `panel/`, no `panel/` include under
      `storage/`, and no namespace that does not end in its folder's path.
