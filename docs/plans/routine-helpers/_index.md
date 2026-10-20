# Berry page abstractions: `Page` lifecycle + `Chrome` display (and a state-machine comparison)

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
abstraction to make: a display object that wholly owns that layout. That piece is its own
sub-plan, [chrome.md](./chrome.md).

Separately, `static.be`'s `advance()` is the only phase machine in the tree. The request asked
whether a `StateMachine` helper would simplify it; per the project's "one caller means no
interface" / "no speculative generality" rules it likely would not — but we will prototype one
and compare, keeping the simpler.

## Decision

1. **Extract a `Page` base class** into a new module `firmware/gardener/data/modules/page.be`
   and have all four routines extend it (chosen scope: full extraction).
2. **Extract a `Chrome` component framework** — a `Chrome` in
   `firmware/gardener/data/modules/chrome.be` that composes drawing components (`FixedText`,
   `DynamicText`, `PressureDisplay`, `ProgressBar`, `Scrollbar` in `components.be`), so `default`
   and `static` reduce `draw_page()` to `self.chrome.draw(vacuum.state())` (readout pages only;
   `browser` and `system` keep their list layouts). Its own sub-plans: [chrome.md](./chrome.md)
   and [components.md](./components.md).
3. **Prototype a `StateMachine`** for `static.be`, compare it against the hand-rolled
   `advance()`, and keep whichever is genuinely simpler (expectation: the hand-rolled version
   wins; the loser is deleted, not deprecated).

## The `Page` base class (`modules/page.be`)

`Page` owns the shared skeleton; pages override only what differs. Defaults keep every hook a
no-op so a page implements only what it uses.

- fields: `last_draw`, `unit`, `fmt`, `editor`
- `init()` — zero the shared fields
- `load_unit()` — `self.unit = screen.unit(); self.fmt = "%." + str(self.unit.decimals) + "f"`
- `number(kpa)` — `string.format(self.fmt, kpa * self.unit.per_kpa)`
- `value(kpa)` — `self.number(kpa) + " " + self.unit.suffix` (the labelled form for a target line)
- `take_events()` — the drain loop, calling `self.handle(ev)`; returns whether anything acted
- `handle(ev)` — routes to the settings editor when `self.editor != nil`, else `handle_page(ev)`;
  on editor close sets `editor = nil` and calls `on_editor_closed()`
- `handle_page(ev)` — no-op default; each page's current `handle()` body moves here
- `open_settings(name)` — constructs `Settings(name)`, opens it, stores it in `editor`
- `on_editor_closed()` — no-op default (static overrides it to re-read settings + retarget)
- `draw()` — `draw_clear()` → editor or `draw_page()` → `flush()` → `last_draw = vacuum.now_ms()`
- `draw_page()` — no-op default; each page's current draw body moves here
- `poll(now)` — `acted = take_events(); tick(now); if acted || now - last_draw >= 250: draw()`
  (this is `static`'s existing structure generalized; for the other three pages `tick` is a no-op,
  so behaviour is identical)
- `tick(now)` — no-op default (static overrides it for the stopped-notice + `advance()`)

## The `Chrome` display class

The readout display is its own piece: [components.md](./components.md) specifies the drawing
primitives (`Component` + `FixedText`/`DynamicText`/`PressureDisplay`/`ProgressBar`/`Scrollbar`)
and the font mapping; [chrome.md](./chrome.md) specifies how a routine's `Chrome` composes them.

## Per-page changes

- **default.be** `Readout : Page` — keep `restore()`/`start()`; drop `unit`/`fmt`/`last_draw`,
  `take_events`, `poll`, `draw`; rename `handle` → `handle_page`; drop `text(kpa)` in favour of
  inherited `number`/`value`. Rendering moves to `ReadoutChrome` — see [chrome.md](./chrome.md).
- **static.be** `Static : Page` — keep the cycle (`begin`/`advance`/`abandon`), settings and
  button logic; drop `unit`/`fmt`/`last_draw`/`editor`, `take_events`, `poll`, `draw`, `number`,
  `text`, `open_settings`; rename `handle` → `handle_page` (editor branch removed), override
  `tick(now)` (stopped + advance), override `on_editor_closed()` (re-read settings + retarget).
  Rendering moves to `StaticChrome` — see [chrome.md](./chrome.md).
- **browser.be** `Browser : Page` — keep `names`/`selected`/`rows`; drop `last_draw`/`editor`,
  `take_events`, `poll`, `draw`, `open_settings`; rename `handle_list` → `handle_page` (editor
  branch removed), `draw_list` → `draw_page`. No Chrome — it is a list layout.
- **system.be** `System : Page` — keep its inline (`editing` boolean) editor and all logic; drop
  `unit`/`fmt`/`last_draw`, `take_events`, `poll`, `draw`; rename `handle` → `handle_page`; move
  draw body to `draw_page()`; `read()` uses `load_unit()`; replace `text(kpa)` with `number(kpa)`.
  No Chrome — it is a list/edit layout.
- **settings.be** — untouched: it is a widget that draws into the page's buffer, not a page.

## State-machine prototype (compare, then keep or discard)

Candidate: `modules/statemachine.be` defining `StateMachine` with `phase`, `mark`, a
`steps` map (phase → closure), `on(name, step)`, `to(name, now)`, `waited(seconds, now)` and
`advance(now)`. `static.be` would register one closure per phase and reduce `advance()` to
`self.machine.advance(now)`.

Compare against the current 30-line `advance()` on: total lines in `static.be`, and whether a
reviewer can still read the cycle and its two shared edges (fault/IDLE → abandon) as directly.
Key implementation risk to verify first: whether Berry closures reliably capture the enclosing
`self` (fallback: pass the owner explicitly into the machine). **Expect to keep the hand-rolled
version**; the prototype is deleted unless it is clearly simpler.

## Docs

- `docs/specs/routines.md` — "Shared code in `/modules`": document `page.be`/`Page`,
  `components.be`/`Component` and `chrome.be`/`Chrome` beside `settings.be`; a routine may extend
  `Page` to inherit the skeleton and compose `Chrome` components to own its readout. Keep the
  `activate()`/`poll(now)` contract unchanged (a page still satisfies it, via the base).
- `README.md` — the "Shared Berry code lives in `/modules/`" paragraph: stop calling the editor
  "the one module"; mention the page-skeleton and display modules.

## Validation

- `python scripts/arduino.py --task build` — C++ build sanity check. Note: the `.be` files ship
  as data (`--task data`) and are interpreted at boot; the build does **not** parse Berry, and the
  repo has no host-side Berry validator. Verification is therefore the build plus careful review
  of the Berry files; on-device boot logs (`vm: … compile/load`) are the only runtime check, and
  they need hardware. Changes are kept mechanical and conservative to compensate.

## Notes

- Load order within `/modules/` is unspecified; `page.be`, `components.be`, `chrome.be` and
  `statemachine.be` define classes only at top level, and `Page.open_settings()` constructs
  `Settings` at call time, so no module depends on another at load.
- Behaviour must be identical after the refactor: the `poll` restructure preserves `static`'s
  `advance()`-every-beat, and the other pages' early-return is provably equivalent to the
  `acted || elapsed >= 250` gate.
