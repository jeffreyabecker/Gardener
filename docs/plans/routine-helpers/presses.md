# Presses: the gesture dispatch

Part of the routine-helpers plan — see [_index.md](./_index.md) for the whole effort. `Page` owns
the button drain; this piece replaces each page's nested `if ev.btn == … / if ev.press == …` chain
with declarative `on(btn, presses, fn)` registrations dispatched by the base.

## Problem

Every page's `handle(ev)` is the same shape: for each button, check the press gesture, run an
action, return whether it was handled. Five copies (`default`, `static`, `browser`, `system`,
`settings`), and most of the lines are the dispatch and the `return true/false` plumbing, not the
action.

## The shape

`Page` holds a list of gestures; a page registers them in `init()` and the base `handle_page()`
dispatches. `presses` is one gesture token or a list of them — never "any", because the device's
own `long` Back hold must not reach the page.

```berry
class Page
  var gestures

  def init()
    self.gestures = []
  end

  def on(btn, presses, fn)          # presses: a token, or a list of tokens
    self.gestures.push([btn, presses, fn])
  end

  def handle_page(ev)               # default; pages rarely override
    for g in self.gestures
      if ev.btn == g[0] && self.press_matches(ev.press, g[1])
        if g[2]()                   # first match that acts wins; false falls through
          return true
        end
      end
    end
    return false
  end
end
```

## Before and after

`default.be` goes from a ~35-line nested chain to:

```berry
def init()
  super(self).init()
  self.on("ok", "double", / -> routines.set_active("system") end)
  self.on("ok", "short",  / -> self.start() end)
  self.on("back", ["short", "double"], / -> self.stop() end)
  self.on("up", "double", / -> routines.set_active("browser") end)
  self.on("down", "double", / -> routines.set_active("system") end)
end
```

## Rules

- **Order is the contract**: registrations are tried in order, so `double` is registered before
  `short` (the classifier already withholds the first short, so a double is never two shorts), and
  a specific press before a broad list.
- **A handler that returns false falls through**, so a guarded action can decline — `static`'s OK
  becomes `/ -> self.phase == "ready" ? self.begin() : false end`, and `browser`'s empty-list guard
  lives in the lambda.
- **Mode-switching pages override `handle_page`**: `system` keeps a small override that consults
  one gesture list while editing and another while listing.
- **`presses` is explicit** — `["short", "double"]` on Back keeps the reserved `long` hold out of
  the page.

## Notes

- Lives on `Page` (no new module): it is part of the page lifecycle, alongside `take_events()` and
  the editor delegation.
- Actions are lambdas closing over `self`, the page instance.
