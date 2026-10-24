# Epic 2 — text

The v2 `TextV2` in `system/components/v2/text.be`, exercised by `tools/berry/tests/v2.test.be`.

- 2.1 — `TextV2` tries the compiled face at 24, 18, 12 and 9 px, bold when the style is bold, and
  the built-in 5×7 only last, fitting once per string and caching the face.
- 2.2 — a string wider than its box at the built-in face is cut short with a trailing ellipsis,
  nothing drawn outside the box.
- 2.3 — a bold built-in line is drawn and its ink box inverted, since the built-in face has no
  bold.

Verified: `python tools/berry/check.py --test v2` passes three cases — compiled-first fit,
truncation, and the bold built-in path — and the shipped pages still render unchanged.
