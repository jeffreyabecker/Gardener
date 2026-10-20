# Routine helpers — backlog

The task list for the plans in this directory, in dependency order. Each item is a commit-worthy
unit. Work in this order: `Page` → components → `List` → page refactors → docs.

## 1. `Page` — the lifecycle base (`modules/page.be`)

- [ ] Create `Page` with fields `last_draw`, `editor`, `gestures`; `init()` zeroing them.
- [ ] `take_events()` — the button drain loop calling `self.handle(ev)`, returning whether anything acted.
- [ ] `handle(ev)` — editor-first routing: when `editor != nil`, delegate and close on `!is_open()` with `on_editor_closed()`; else `handle_page(ev)`.
- [ ] `on(btn, presses, fn)` + press matching, and the default `handle_page(ev)` dispatcher (first match that acts wins; a false result falls through).
- [ ] `open_settings(name)` — construct `Settings(name)`, open it, store it in `editor`.
- [ ] `on_editor_closed()` — no-op default.
- [ ] `draw()` — `draw_clear()` → editor or `draw_page()` → `flush()` → `last_draw = vacuum.now_ms()`.
- [ ] `draw_page()` — no-op default.
- [ ] `poll(now)` — `acted = take_events(); tick(now); if acted || now - last_draw >= 250: draw()`.
- [ ] `tick(now)` — no-op default.

## 2. Components (`modules/components.be`)

- [ ] `Component` base — `x`/`y`/`w`/`h`/`margin`/`border`/`padding`, `init()`, `content()`, empty `draw()`.
- [ ] Font resolution — family/style → ordered face list, and the largest-face-that-fits helper over `text_bounds`.
- [ ] `FixedText` — config `value`/`family`/`style`; fit once, cache, `draw()`.
- [ ] `DynamicText` — config `value`/`family`/`style`; `draw(value)` re-fits; `nil` → config `value`.
- [ ] `PressureDisplay` — config `show_units`/`family`/`style`; `draw(kpa)` reads `screen.unit()` itself, converts, fits; `nil` → `--`.
- [ ] `ProgressBar` — `draw(value)` over 0–100: frame on the border box, fill across the content rect.
- [ ] `ProgressBubbles` — config `max_count`/`bubble_style`/`unfilled`/`fill_direction`; `draw(count)`: filled bubbles, rest outline or none; multi-line, gap to 0 then 2×2/1×1 solid squares; cache the computed layout.
- [ ] `Scrollbar` — config `invert`; `draw(index, count, visible)`.
- [ ] `Triangle` — config `direction`/`style`; `draw()` via `screen.draw_triangle`/`draw_filltriangle`.
- [ ] `Icon` base — `w`/`h`, `init(w, h)`, empty `draw(x, y)`.

## 3. `List` widget (`modules/list.be`)

- [ ] `List` — fields `rows` (`[icon, label]`), `selected`, `visible`, `scrollbar`; `init(visible)`.
- [ ] `set(rows)` — store the rows, clamp the selection.
- [ ] `move(delta)` — modulo wrap; false when empty.
- [ ] `first()` — the first visible row.
- [ ] `draw(x, y)` — visible window, selected row inverted, icon then label, feed the `scrollbar`.

## 4. Page refactors

- [ ] `default.be` → `Readout : Page` — `on(…)` gestures; components for the readout (`PressureDisplay` big number, `FixedText` + `PressureDisplay` target line, `DynamicText` header); keep `restore()`/`start()`.
- [ ] `static.be` → `Static : Page` — `on(…)` gestures; `tick(now)` (stopped-notice + `advance()`); `on_editor_closed()`; components for the readout; keep the cycle.
- [ ] `browser.be` → `Browser : Page` — `on(…)` gestures; a `List` + `Scrollbar`.
- [ ] `system.be` → `System : Page` — `on(…)` gestures with a `handle_page` override for the edit/list modes; a `List` with `visible = 3`; keep the edit face and its `unit`/`fmt`.
- [ ] `settings.be` — adopt a `List` (rows with per-kind icons); keep `open()`/`close()`, `describe()`, store reads/writes and the two-stage edit face.

## 5. Docs

- [ ] `docs/specs/ui/routine.md` — document `page.be`/`Page`, `components.be`/`Component` and `list.be`/`List` in the "Shared code in `/modules`" section.
- [ ] `README.md` — update the "Shared Berry code lives in `/modules/`" paragraph: page-skeleton, display and list modules.

## 6. Validation

- [ ] `python scripts/arduino.py --task build` passes; review every changed `.be` — the build does not parse Berry and there is no host validator, so on-device boot logs (`vm: … compile/load`) are the runtime check.
