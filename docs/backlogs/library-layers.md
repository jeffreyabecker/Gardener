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
- [ ] The fifteen headers that open `namespace gardener` twice collapse to one block, its
      declarations at the head and the layer opened below them.
- [ ] `routines/` declares `gardener::routines`, opening after its declarations.
- [ ] The three ticks move to `beats/` declaring `gardener::beats`.
- [ ] `serial/` declares `gardener::serial`, and its domains `gardener::serial::domains`.
- [ ] `storage::SettingKind` takes the document half of a settings row, and `RoutineSettingStore`
      stops naming `panel`.
- [ ] `core` drops its inline and its uses are qualified.
- [ ] `machine` and `storage` drop their inline and their uses are qualified.
- [ ] `panel`, `panel::parts` and `panel::page` drop their inline.
- [ ] `bindings`, `routines` and `beats` drop their inline.
- [ ] The three greps hold: no `routines/` include under `panel/`, no `panel/` include under
      `storage/`, and no namespace that does not end in its folder's path.

## The rule the nesting needs

A namespace definition is resolved relative to the namespace around it, so a header that declares a
type it does not include must declare it directly in `gardener`:

    namespace gardener {

    inline namespace core {
    class Logger;
    }
    namespace panel {
    class GestureQueue;
    }

    inline namespace beats {

    class ButtonsTick {
      ...
      panel::GestureQueue& presses_;
    };

    }  // namespace beats
    }  // namespace gardener

Written inside `inline namespace beats`, that `namespace panel { class GestureQueue; }` would declare
`beats::panel::GestureQueue`, and the `panel::GestureQueue` in the member list would bind to it. So
the layer's brace goes *below* the declarations, not above them, and the header keeps one
`namespace gardener` block. Declarations hoisted to a second block above the first work too, and
that is what the first four items did, but they read as the namespace opened twice and leave two
places to put the next one.

## Where it stands

`panel/` no longer reaches up into the Berry layer: BerryScene moved to `bindings/`, and a grep for
`#include "routines/` under `src/panel` comes back empty. `storage/RoutineSettingStore.h` still
includes `panel/page/SettingsRows.h`, which the `SettingKind` item above is what removes.

The unit suite was green at 239 cases and 980 assertions after the second item. It has not been run
since: from the third item onward this machine's linker has stopped producing executables, failing
with `collect2.exe: error: ld returned 1 exit status` and no diagnostic, at every output path and on
the untouched tree as well. Everything after that point is compiled by `tools/unit/check.py` and by
`scripts/arduino.py --task build`, both of which reach the link and report no error before it, but
it is compiled and not run. Run the suite before anything else is built on top of it.
