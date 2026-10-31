# Library layers

The steps of [laying the library out by layer](../plans/laying-the-library-out-by-layer.md).

- [x] `core/` declares `gardener::core` inline, and `firmwareversion.h` joins it.
- [x] `machine/` and `storage/` declare `gardener::machine` and `gardener::storage` inline.
- [x] `panel/`, `panel::parts` and `panel::page` name their folders. The tree already held this.
- [x] `routines/bindings/` becomes `bindings/` declaring `gardener::bindings`, with `BerryScene` in it.
- [x] The headers that opened `namespace gardener` twice collapse to one block, its declarations at
      the head and the layer opened below them.
- [x] `routines/` declares `gardener::routines`, opening after its declarations.
- [x] The three ticks move to `beats/` declaring `gardener::beats`.
- [x] `serial/` declares `gardener::serial`, and its domains `gardener::serial::domains`.
- [x] `storage::SettingKind` takes the document half of a settings row, and `RoutineSettingStore`
      stops naming `panel`.
- [x] The three greps hold: no `routines/` include under `panel/`, no `panel/` include under
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
the layer's brace goes below the declarations, not above them, and the header keeps one
`namespace gardener` block.

The same rule catches a declaration of the layer's own type written in the enclosing namespace: with
the layer inline, `class Routines;` at `gardener` scope names a second type and the two are
ambiguous wherever both are visible. It belongs inside `inline namespace routines`.

## Why the inlines stay

The plan said the inline was a way in and would be dropped once a layer's uses were qualified. It is
not being dropped. Measured on `core` alone, qualifying every use is 370 sites across 83 files, and
the six characters of `core::` push a hundred of them past the column limit, so the diff rewrites
half the tree for no behaviour to show for it. The definition names the layer, the folder and the
namespace agree, and the include rule is what holds the edges pointing down; a file may name a layer's
type flat from inside `gardener`, and a file that is being changed anyway may qualify what it uses.

## Where it stands

All three greps hold: nothing under `panel/` includes `routines/`, nothing under `storage/` includes
`panel/`, and every header but the umbrella names its folder. The edges left run downward.

The unit suite was green at 239 cases and 980 assertions up to the second item. It has not been run
since: from the third item onward this machine's linker has stopped producing executables, failing
with `collect2.exe: error: ld returned 1 exit status` and no diagnostic, at every output path and on
the untouched tree as well. Everything after that point is compiled by `tools/unit/check.py` and by
`scripts/arduino.py --task build`, both of which reach the link and report no error before it, but it
is compiled and not run. Run the suite before anything else is built on top of it, and once it is
green this file and the plan beside it have done their job and can go.
