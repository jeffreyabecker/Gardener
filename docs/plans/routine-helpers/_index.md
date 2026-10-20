# Berry page abstractions: `Page`, `Component` and `List`

## Problem

The four shipped routines — `static`, `default`, `browser`, `system` — each hand-roll the same
page skeleton: the button drain loop, the `poll()` repaint cadence, the `draw()` clear/flush +
`last_draw` wrapper, the `unit`/`fmt` setup and `number(kpa)` formatting, and (in two of them)
the shared-settings-editor delegation. That is 4 copies of the drain loop, 3 copies of the poll
cadence plus one variant, 4 draw wrappers, and 4–5 copies of the unit formatting. The codebase
already names the remedy: shared Berry code lives in `/modules/`, and `settings.be` proves a
shared class there works.

The readout pages (`default`, `static`) also redraw the same layout by hand — header line, big
number, `TGT …` line, and (static) status/hint lines — so there is a second, higher-level
abstraction to make: a display framework of drawing components a page composes. That piece is its
own sub-plan, [components.md](./components.md).

The list pages (`browser`, `system`) and the settings editor each hand-roll a menu: a selected
index walked with Up/Down, wrapped at the ends, drawn with one highlighted row and a scroll window.
That is three copies of the same three rules, which a stateful `List` widget makes one — its own
sub-plan, [list.md](./list.md).

## Decision

1. **Extract a `Page` base class** into a new module `firmware/gardener/data/modules/page.be`
   and have all four routines extend it (chosen scope: full extraction).
2. **Extract a `Component` display framework** — drawing components (`FixedText`, `DynamicText`,
   `PressureDisplay`, `ProgressBar`, `Scrollbar`, `Triangle`) plus an `Icon` base class for
   intrinsic-size glyphs, in `firmware/gardener/data/modules/components.be`. A readout page
   composes them as fields and feeds them state in `draw_page()`; `default` and `static` adopt it,
   and `List` rows carry icons. Its own sub-plan: [components.md](./components.md).
3. **Extract a stateful `List` widget** into `firmware/gardener/data/modules/list.be` — it owns a
   selected index, the Up/Down wrap, the scroll window and the highlight, adopted by `browser`,
   `system` and the `settings` editor. Its own sub-plan: [list.md](./list.md).
4. **Register gestures declaratively on `Page`** — `on(btn, presses, fn)` in `init()` replaces
   each page's `handle()` if/elif chain, and the base dispatches. Its own sub-plan:
   [presses.md](./presses.md).

## The `Page` base class (`modules/page.be`)

`Page` owns the shared lifecycle skeleton only — the button drain, the poll cadence, the draw
wrapper and the editor delegation. It knows nothing about units or fonts: pressure formatting
lives in `PressureDisplay`, and the one page that still formats its own text (`system`) keeps the
`unit`/`fmt` fields it already has.

- fields: `last_draw`, `editor`, `gestures`
- `init()` — zero the shared fields
- `take_events()` — the drain loop, calling `self.handle(ev)`; returns whether anything acted
- `handle(ev)` — routes to the settings editor when `self.editor != nil`, else `handle_page(ev)`;
  on editor close sets `editor = nil` and calls `on_editor_closed()`
- `on(btn, presses, fn)` — register a gesture; `presses` is one token or a list of them
- `handle_page(ev)` — default dispatch of the registered gestures (first match that acts wins);
  pages override it only to switch between modes
- `open_settings(name)` — constructs `Settings(name)`, opens it, stores it in `editor`
- `on_editor_closed()` — no-op default (static overrides it to re-read settings + retarget)
- `draw()` — `draw_clear()` → editor or `draw_page()` → `flush()` → `last_draw = vacuum.now_ms()`
- `draw_page()` — no-op default; each page's current draw body moves here
- `poll(now)` — `acted = take_events(); tick(now); if acted || now - last_draw >= 250: draw()`
  (this is `static`'s existing structure generalized; for the other three pages `tick` is a no-op,
  so behaviour is identical)
- `tick(now)` — no-op default (static overrides it for the stopped-notice + `advance()`)

## The `Component` display framework

The readout display is its own piece: [components.md](./components.md) specifies the drawing
primitives (`Component` + `FixedText`/`DynamicText`/`PressureDisplay`/`ProgressBar`/`Scrollbar`/
`Triangle`, and the lighter `Icon` glyph), the font mapping, and how a readout page composes them
into its `draw_page()`.

## The `List` widget

The selectable menu is its own piece: [list.md](./list.md) specifies the stateful `List` —
selection, wrap, scroll window, highlight and per-row icons — and how `browser`, `system` and
`settings` adopt it.

## The gesture handler

The button dispatch is its own piece: [presses.md](./presses.md) specifies `Page.on(btn, presses,
fn)` — declarative gesture registration replacing each page's `handle()` if/elif chain.

## Per-page changes

- **default.be** `Readout : Page` — keep `restore()`/`start()`; drop `unit`/`fmt`/`last_draw`,
  `take_events`, `poll`, `draw`, `text(kpa)`; replace `handle` with `on(…)` registrations.
  Rendering becomes
  component fields fed in `draw_page()` — a `PressureDisplay` for the big number, a `FixedText` +
  `PressureDisplay` for the target line, a `DynamicText` for the header — see
  [components.md](./components.md).
- **static.be** `Static : Page` — keep the cycle (`begin`/`advance`/`abandon`), settings and
  button logic; drop `unit`/`fmt`/`last_draw`/`editor`, `take_events`, `poll`, `draw`, `number`,
  `text`, `open_settings`; replace `handle` with `on(…)` registrations (editor branch removed),
  override
  `tick(now)` (stopped + advance), override `on_editor_closed()` (re-read settings + retarget).
  Rendering becomes component fields fed in `draw_page()` — a `PressureDisplay`, a `FixedText` +
  `PressureDisplay` target line, and `DynamicText`s for status/hint — see
  [components.md](./components.md).
- **browser.be** `Browser : Page` — keep `names`; drop `last_draw`/`editor`, `take_events`,
  `poll`, `draw`, `open_settings`, `selected`/`rows`/`first`/`handle_list`/`draw_list`; replace
  `handle` with `on(…)` registrations. The list becomes a `List` set with `routines.names()` on
  activate;
  Up/Down are `move(±1)`, OK/Back stay the page's — see [list.md](./list.md).
- **system.be** `System : Page` — keep its inline (`editing` boolean) editor, its own `unit`/`fmt`
  and all formatting; drop `last_draw`, `take_events`, `poll`, `draw`, the hardcoded three-row
  wrap; replace `handle` with `on(…)` registrations. The three rows become a `List` with
  `visible = 3`; the
  edit face is unchanged — see [list.md](./list.md).
- **settings.be** — adopt a `List` for its rows and `index`; keep `open()`/`close()`, `describe()`,
  the store reads/writes and the two-stage edit face — see [list.md](./list.md).

## Docs

- `docs/specs/ui/routine.md` — "Shared code in `/modules`": document `page.be`/`Page`,
  `components.be`/`Component` and `list.be`/`List` beside `settings.be`; a routine may extend
  `Page` to inherit the skeleton, compose components to own its readout, and hold a `List` for a
  menu. Keep the `activate()`/`poll(now)` contract unchanged (a page still satisfies it, via the
  base).
- `README.md` — the "Shared Berry code lives in `/modules/`" paragraph: stop calling the editor
  "the one module"; mention the page-skeleton, display and list modules.

## Validation

- `python scripts/arduino.py --task build` — C++ build sanity check. Note: the `.be` files ship
  as data (`--task data`) and are interpreted at boot; the build does **not** parse Berry, and the
  repo has no host-side Berry validator. Verification is therefore the build plus careful review
  of the Berry files; on-device boot logs (`vm: … compile/load`) are the only runtime check, and
  they need hardware. Changes are kept mechanical and conservative to compensate.

## Notes

- Load order within `/modules/` is unspecified; `page.be`, `components.be` and `list.be` define
  classes only at top level, and `Page.open_settings()` constructs `Settings` at call time, so no
  module depends on another at load.
- Behaviour must be identical after the refactor: the `poll` restructure preserves `static`'s
  `advance()`-every-beat, and the other pages' early-return is provably equivalent to the
  `acted || elapsed >= 250` gate.

## Backlog

The work, as an ordered task list: [backlog.md](./backlog.md).
