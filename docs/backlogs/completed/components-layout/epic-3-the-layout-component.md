# Epic 3 — the layout component

The v2 `LayoutV2` in `system/components/v2/layout.be`, exercised by `tools/berry/tests/v2.test.be`.

- 3.1 — `LayoutV2` holds children and places measured children along the fill direction with a
  `spacing`; components gain a `measure` call, and `TextV2` measures its string at the face that
  fits the cross size.
- 3.2 — a child marked `grow` takes an equal share of the space left after the measured children,
  and every child stretches to the layout's cross size.
- 3.3 — `wrap` carries a child that no longer fits onto the next line or column, each line as tall
  as its tallest child.
- 3.4 — the remaining fill directions `rl-tb`, `tb-lr` and `bt-lr` place children against the far
  end when the direction is reversed.
- 3.5 — a layout reports its natural size — the sum of its children's sizes plus the spacing, and
  the largest child across — so a parent layout measures it like any child.

Verified: `python tools/berry/check.py --test v2` passes ten cases, and the shipped pages still
render unchanged.
