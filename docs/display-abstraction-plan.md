# Plan: a host-drawn display

`StatusPage` draws straight into an `Adafruit_GFX&`. It works, and it was the right call when the
alternative was a wrapper of our own, but it leaves the page bound to a third-party library in
four ways:

* it names monochrome pixels itself (`kInk = 1`, `kPaper = 0`), and passes those to every call
* it knows that inversion is spelled `setTextColor(ink, paper)` rather than
  `setTextColor(ink)`, which is a library detail leaking into layout code
* `widthOf()` has to set the text size, then ask `getTextBounds`, because the measurement reads
  "the current size" instead of taking one
* it cannot be compiled or run without Adafruit, so the one file in the project full of
  arithmetic — centring, the battery fill clamp, the `mm:ss` splice — is the one file that
  cannot be tested on the host

Revs 1 to 3 answered that by putting an interface in front of the page. **Rev 4 goes further and
takes the page out**: `StatusPage` is deleted, and the host draws the panel over the same serial
line it already uses. What is left on the device is the panel, an interface over it, and an
overlay that puts the battery on every frame.

`DECISION:` this reintroduces a display abstraction that was deleted earlier in the project's
life, and the difference is worth stating. The deleted one was a concrete pass-through — a hop in
front of every call and no seam. This one is an interface with the colour policy inside it, and
it is now the seam between the wire and the glass. If the fake in phase 5 is never written, this
is a wrapper with one implementation and should be deleted again.

## 1. The interface

`hal/Display.h`, no colour arguments anywhere, in eight groups:

| Group | Verbs |
| --- | --- |
| the panel | `begin`, `flush` |
| geometry | `width`, `height`, `rotation`, `setRotation` |
| the screen | `clear`, `invert` |
| pixels and lines | `pixel`, `clearRect`, `line`, `hLine`, `vLine` |
| shapes | `rect`, `fillRect`, `roundRect`, `fillRoundRect`, `circle`, `fillCircle`, `ellipse`, `fillEllipse`, `triangle`, `fillTriangle`, `rotatedRect`, `fillRotatedRect` |
| images | `bitmap` |
| text | `text`, `textBounds` |

`DECISION:` this is the library's whole drawing surface, not the calls a page happens to make, so
that a host drawing something new does not have to wait for a firmware change. Rev 1 proposed the
narrow list and was wrong about what the abstraction is for; rev 4 makes the mistake worse to
repeat, because the set of things a host might draw is not knowable from here.

`DECISION:` (rev 3, by hand; settled in rev 5) text is stateless: `text` and `textBounds` taking
the scale at the call, and nothing else. `setFont` was in and is now out, because the host has no
business choosing a font: it has no way to supply the font data, so the built-in 5x7 font scaled
by `size` is the whole of what the wire can ask for.

That removes the one library type the header ever named:

* **`hal/Display.h` includes nothing from Adafruit.** Rev 4 had it including `<Adafruit_GFX.h>`
  for `GFXfont`, which cannot be forward declared because `gfxfont.h` typedefs it from an
  anonymous struct. With the font gone the include goes, and R6 with it: a host fake implements
  this header with no library and no stub structs.
* **`size` means one thing again**, the scale of the built-in font, so Q13 stops being a question
  and "the parameter is conditional" stops being true.

`DECISION:` paper has two spellings — `clear()` for the whole panel, `clearRect()` for part of it
— because a host that blinks or updates a field wants to erase without repainting everything.

## 2. The Adafruit implementation

`hal/AdafruitDisplay.{h,cpp}`, holding the panel by reference:

```cpp
class AdafruitDisplay : public Display {
 public:
  explicit AdafruitDisplay(Adafruit_GrayOLED& panel) : panel_(panel) {}

  bool begin() override;   // panel_.begin(pins::OLED_I2C_ADDR, false)
  void flush() override;   // panel_.display()
  ...
 private:
  static constexpr uint16_t kInk = 1;
  static constexpr uint16_t kPaper = 0;
  Adafruit_GrayOLED& panel_;
};
```

* It holds the concrete `Adafruit_SH1106G`, because bring-up is the one thing the hierarchy does
  not share: the drawing verbs come from `Adafruit_GFX`, `display()` from `Adafruit_GrayOLED`, and
  `begin()` from the panel class itself. Rev 6 claimed `Adafruit_GrayOLED` would do for all three;
  it does not, so the adapter names the panel it can actually bring up.
* Every verb in §1 is one line here: the shapes, the rotated rects, the ellipses and the bitmap
  are all public on `Adafruit_GFX`, virtual or not, so the adapter forwards.
* `kInk`/`kPaper` land here, which is the point: nothing outside this file knows that 1 means lit.
* `App` still constructs the panel, so it stays the only place that knows *which* panel is fitted.
* The `false` in `begin` is not incidental: the STEMMA QT breakout has no reset line, so the
  library must not poke one.

## 3. AppDisplay, the battery overlay

Landed already as `app/AppDisplay.{h,cpp}`: a `Display` that wraps another `Display&`, draws the
battery from the snapshot on the way to `flush()`, and forwards the other 29 verbs.

`DECISION:` the overlay composites at flush rather than the page drawing a battery, and rev 4 is
what makes that the only correct shape. The panel now has two authors — the host, and whatever
the device thinks has to be on screen regardless — and flush is the one moment they meet. A host
cannot erase the battery, because the battery is not there yet when the host draws.

## 4. StatusPage goes away

Everything else it did moves to the host: the pressure and target rows, the countdown, the duty
bar, the `FAULT` and `NO SENSOR` banners, the alignment helpers, the row constants and
`twoDigits`.

The splash does not move to the host and does not disappear. It becomes `App::splash()`, drawn
from `App::begin()` — which is also the path a `reboot` takes, since that command restarts the
device and `begin()` runs again. So "at startup or reboot" is one method and one call site.

`DECISION:` the splash is the device's own, and it is the exception that proves §4's rule. The
host owns the panel after boot, but before a host has said anything the panel is the only thing
that can tell a person at the rig that this device is alive at all, and which firmware it is
running. It draws the same two lines `StatusPage::splash()` draws today, through the interface,
and flushes through the overlay so it gets the battery as well:

```cpp
void App::splash() {
  display_.clear();
  display_.text(0, 0, "Eggplan Vacuum", 1, false);
  display_.text(0, 12, String("fw ") + cfg::FIRMWARE_VERSION, 1, false);
  display_.flush();
}
```

The one thing that does *not* come across is the page's `kPaper`/`kInk`: `clear()` is paper and
`text` is ink, which is the whole point of the interface underneath.

`StatusPage.{h,cpp}` are deleted, along with its `render()` call in `App`.

`DECISION:` the device boots to its own splash and nothing more: cleared, splash drawn, flushed,
and then no further drawing until a host asks for some. Rev 4 said "boots to a blank panel",
which this supersedes — the honest statement is that the panel shows the boot screen and then
holds it. There is no timeout on it, because a timeout would be a device clock and this device
does not keep one.

`DECISION:` nothing else is annunciated locally, not even a fault. A fault, a low battery and a
vent timeout all live in `status`. The battery icon and the boot screen are the only two things
the device draws for itself, and the person standing at the rig sees what the host chose to draw
— or, if no host ever drew, whatever the last device-drawn frame was, while the chamber vents.
That is a real loss and it is R8.

## 5. The drawing commands

The panel is a peripheral, so the host draws it. This is the part of the plan with the most
detail, because it is a wire format and the wire format is the contract.

### 5.1 Shape of the commands

`DECISION:` drawing goes under one verb, `draw <op> <args…>`, rather than two dozen flat verbs.
The protocol already has a namespaced verb in `set <key> <value>`, so this is the established
shape rather than a new one, and `help` grows by one word instead of a wall of drawing verbs.

| Command | Reply | Notes |
| --- | --- | --- |
| `draw help` | `ok ops=clear\|pixel\|line\|hline\|vline\|rect\|fillrect\|roundrect\|fillroundrect\|circle\|fillcircle\|ellipse\|fillellipse\|triangle\|filltriangle\|rotatedrect\|fillrotatedrect\|text` | the op list, asked for rather than announced |
| `draw clear` | `ok drawn` | the whole panel to paper |
| `draw pixel x y` | `ok drawn` | |
| `draw line x0 y0 x1 y1` | `ok drawn` | |
| `draw hline x y w` / `draw vline x y h` | `ok drawn` | |
| `draw rect x y w h` / `draw fillrect x y w h` | `ok drawn` | and the round, ellipse, triangle and rotated families with their own arguments |
| `draw text x y size inverted <the rest of the line>` | `ok drawn` | |
| `flush` | `ok flushed` | pushes the buffer to the glass, and the only command that does |
| `set rotation <0-3>` | `ok rotation=2` | |
| `set invert <on\|off>` | `ok invert=on` | the whole panel, for a host that wants attention |

`DECISION:` `draw text` takes the rest of the line verbatim, spaces included. The values-are-space-free
rule exists so a reply can be parsed by splitting on spaces; a string being drawn has no such
constraint, and quoting or `_`-escaping it would be inventing an escape mechanism for one command.
The cost is that the line splitter has to be able to hand back the tail, which is a change to
`splitLine` rather than a new parser.

`DECISION:` geometry is not range checked. Adafruit clips to the panel, so `draw rect -40 -40 300 300`
draws the visible part and nothing else; a coordinate that is nonsense is a client bug that the
panel absorbs rather than an error, and refusing half a rectangle is worse than clipping it. What
*is* checked: the op name, the argument count, and that each argument parses as a number.

### 5.2 Frames

`DECISION:` `flush` is its own command, and it is the frame boundary. Drawing ops only touch the
panel's buffer, so a host draws a whole page and then sends one `flush` — one burst over I2C, no
half-drawn frame on the glass. It is also the only moment the battery overlay can land, which is
what makes §3 work.

`DECISION:` the host owns the frame rate. The device redraws nothing on its own and sends no
unsolicited output, so a host that stops drawing leaves the last frame frozen on the glass.

### 5.3 Cost

A full 128x64 page is roughly 15 to 30 drawing ops. A drawing op line is 15 to 30 bytes, so a
frame is under a kilobyte: at 4 Hz that is 4 KB/s against 11.5 KB/s of headroom at 115200 8N1, and
the host's own `status` polling shares it. Worth knowing before someone draws a bar graph in 40
`fillrect` calls at 30 Hz.

`DECISION:` bitmaps are not on the wire in this change. `bitmap` stays an interface verb with a
forwarding implementation, and the wire gets it when something needs it: it needs an encoding for
the bits and a value that is much longer than anything else on the line.

## 6. Consequences

1. **The host owns every pixel except the battery and the boot screen.** Any layout, any language,
   any redraw rate.
2. **The device shrinks**: no page, no layout constants, no text metrics of our own, no
   `twoDigits`. Roughly 200 lines leave with `StatusPage`, and what replaces them is `App::splash()`
   and a command table.
3. **The interface is still worth having**, but its justification moves. It was "so `StatusPage`
   can be tested"; it is now "so the drawing commands can be tested against a fake, and so the
   panel can be swapped". Phase 5 is that test, and it is now testing the wire rather than a page.
4. **Colour policy stays in one place**, which matters more with an unbounded set of host-drawn
   content.
5. **The wire format is now a drawing API**, so it needs the same care as the rest of the
   protocol: documented, versioned by `fw`, and stable.
6. **The fat interface costs real flash once something implements it.** Referencing every verb in
   a vtable pulls in every Adafruit_GFX primitive the adapter wraps, used or not: instantiating
   the adapter in phase 1d took the image from 336,028 to 344,344 bytes. Phase 2 uses most of them
   anyway, but the number belongs in the plan rather than in a surprise.

## 7. Phases

| # | Phase | Content | Verify |
| --- | --- | --- | --- |
| 1a | The interface | `hal/Display.h` — **done** | header compiles, layer rule ok |
| 1b | The battery overlay | `app/AppDisplay.{h,cpp}` — **done** | green; deleting one forward fails the build |
| 1c | The adapter | `hal/AdafruitDisplay.{h,cpp}`, one line per verb, colours supplied | green, layer rule ok, flash reported |
| 1d | Wire the display in and draw the splash | App constructs the panel, the adapter and the overlay; `App::splash()` draws the boot screen from `begin()`, which a `reboot` re-enters; nothing else draws | green; the panel shows the splash at startup and holds it with no host |
| 2 | The drawing commands | `SerialLink` gains `draw` and `flush`, `set rotation` and `set invert`; `splitLine` learns to hand back the rest of the line; the argument cap rises from 3 | green; each op answers one reply, and `draw text` keeps its spaces |
| 3 | StatusPage goes away | delete `StatusPage.{h,cpp}`; App keeps only `splash()` and loses `render()`; README's display section says the host draws | green, layer rule ok, flash reported |
| 4 | The protocol doc | `docs/serial-protocol.md`: the `draw` family, the frame model, the cost arithmetic, `fw` to 0.4.0 | green |
| 5 | The fake and a host test | `tests/fake_display.h` and a test for the command layer: parsing, the rest-of-line tail, and the calls each op makes | the test fails when an op is miswired |

## 8. Risks

| id | Risk | Mitigation |
| --- | --- | --- |
| R1 | an abstraction with one implementation is speculative, and this project deleted one already | the second implementation is phase 5; without it, delete the interface again |
| R2 | a fat interface is a commitment: every verb must be implemented by every implementation, and a fake has to fake all of them | accepted, and rev 4 strengthens the case for it: the host can ask for anything the library can draw. `AppDisplay`'s `static_assert` fails the build if a verb is added and not forwarded |
| R3 | a virtual call per primitive, and now a *host-driven* number of them | a frame is one `flush` plus a few dozen calls; the I2C burst next to it is orders of magnitude larger, and the counts are in §5.3 |
| R4 | `textBounds` cannot be `const` because Adafruit's measurement is not | stated in the header |
| R5 | App keeps the panel and passes a `GrayOLED&` into the adapter: two objects for one display | accepted: App is the composition root and the only place that knows which panel is fitted |
| R6 | ~~a host fake needs `GFXfont` to implement `setFont`~~ | **dissolved in rev 5**: `setFont` is out of the interface, so the header names no library type and the fake needs no stub |
| R7 | a host that floods the line starves the control loop: `poll()` drains every buffered line before returning, and the pump's loop is what keeps pressure sane | cap the lines serviced per poll (Q18), so a burst is spread over iterations instead of blocking one |
| R8 | nothing is annunciated locally any more: a fault used to invert a banner on the panel, and now the panel says whatever the last frame said | accepted and documented. The device keeps the one indication it can be trusted to produce alone — the valve releasing — and the fault is in `status` on the next poll |
| R9 | the panel holds whatever was drawn last: the splash at boot, then the host's last frame, with nothing to correct either | accepted: it is the same trade as the clock, and the alternative is a page the device has to maintain. The splash is the one screen the device owns outright, and it stays until the host draws over it |
| R10 | the wire now carries geometry, so a malformed line is a drawing instruction | the op name, the argument count and the numeric parse are all checked; coordinates are clipped by the library, and nothing a host can draw can reach the pump or the valve |

## 9. Questions

Q1–Q13 are answered as recorded. Q12 and Q13 are re-argued below because rev 4 changed their
context.

| # | Question | Answer / recommendation |
| --- | --- | --- |
| Q1 | Place: `hal/Display.h` with `hal/AdafruitDisplay.{h,cpp}`? | yes |
| Q2 | The page's calls, or the library's whole drawing surface? | the library's surface (rev 2) |
| Q3 | No colour parameter anywhere; inversion is a `bool`? | yes |
| Q4 | The adapter wraps `Adafruit_GrayOLED&` and App keeps the concrete panel? | yes |
| Q5 | One measurement method, `textBounds`? | yes (by hand, rev 3) |
| Q6 | A virtual destructor on the base? | yes |
| Q7 | A `FakeDisplay` and a host test? | yes — and rev 4 moves its target from the page to the command layer, which is now where the logic is |
| Q8 | The page keeps its layout and helpers untouched? | **superseded**: the page is deleted, so this is moot |
| Q9 | Keep the name `Display`? | yes |
| Q10 | No firmware version bump? | **superseded by Q20**: the wire gains commands, so `fw` moves |
| Q11 | Are the exclusions right? | settled in rev 5: `setFont` is **out** — the host cannot supply font data, so offering it invites a command nobody can serve — `xBitmap` out, grayscale/RGB bitmaps and the `startWrite` family out |
| Q12 | Do the text verbs also take a `const char*`? | **now clearly yes, and for a better reason**: with the page gone the only caller is the wire, which always has a `const char*` from the line buffer. A `String` overload would convert on every text op for nothing |
| Q13 | `size` is ignored once a font is loaded: document or drop? | **moot in rev 5**: with no font to load, `size` scales the built-in font and always applies |
| Q14 | One `draw <op> …` verb, or a flat verb per op? | **one verb**: it matches `set <key> <value>`, keeps `help` readable, and the op list is a second question the host can ask |
| Q15 | Does `draw text` take the rest of the line verbatim, spaces and all? | **yes**: inventing an escape for one command is worse than teaching the splitter to hand back a tail |
| Q16 | Is `flush` its own command and the only thing that updates the glass? | **yes**: it is the frame boundary, and the only place the battery overlay can land |
| Q17 | Bitmaps on the wire in this change? | **no**: the interface keeps the verb, the wire gets it when something needs it |
| Q18 | Cap the lines serviced per poll so a flood cannot starve the control loop? | **yes, at 8**: a burst then spreads over a few iterations, and the sensor and the PID keep their cadence |
| Q19 | Does the device draw its own splash at startup, then hold it until a host draws? | **yes**: `App::splash()` from `begin()`, which a `reboot` re-enters. It is the one screen the device owns, and it has no timeout because a timeout would be a device clock |
| Q20 | `fw` to 0.4.0? | **yes**: with no `proto` left, `fw` is the only marker, and the wire is gaining a command family |
| Q23 | With the page gone, `set hold <seconds>` sets a value nothing draws. Delete it, or leave it? | **leave it**: the countdown is the host's job, and that is the end of it. No device-side work either way |
| Q21 | Is the splash reachable as a command too, so a host can bring it back? | **no**: it is what the device says about itself before anyone asks. A host that wants a boot screen can draw one, and then it is the host's screen and not the device's |
| Q22 | The splash flushes through the overlay, so the battery is drawn on it — and the gauge may not have sampled yet, so the number could be its initial value. Sample once before drawing? | **yes**: sample the battery once in `begin()` before the splash, so the number on the boot screen is a reading rather than a placeholder. If it cannot be sampled that early, the splash flushes straight to the adapter instead, without a battery |

## 10. Status

| Revision | Change | Sections |
| --- | --- | --- |
| rev 1 | initial plan: `Display` interface, Adafruit adapter, optional host test as phase 2 | all |
| rev 2 | the interface is the library's drawing surface rather than the page's eight calls; `Display` gains `Print`; the absences are written down | §1, §2, §6, §7 |
| rev 3 | the header reworked by hand: `Print`, `setCursor`, `setTextSize`, `setTextWrap`, `cp437`, `textWidth` and `xBitmap` out, `setFont` in, so the header includes `<Adafruit_GFX.h>` for `GFXfont` | §1, §2, §3, §6, §7 |
| rev 4 | `StatusPage` is deleted and the host draws the panel: the `draw` command family, `flush` as the frame boundary, the battery overlay compositing at flush, and the consequences of nothing being annunciated locally | §1, §3, §4, §5, §6, §7, §8, §9 |
| rev 5 | `setFont` is deleted from the interface: the host cannot supply font data, so the built-in font scaled by `size` is the whole of what the wire can ask for. `hal/Display.h` therefore includes nothing from Adafruit, which dissolves R6, and Q13 stops being a question | §1, §8, §9 |
| rev 6 | the splash does not go with the page: it becomes `App::splash()`, drawn from `begin()` and therefore re-drawn after a `reboot`, and it is the one screen the device owns. Rev 4's "boots to a blank panel" is superseded | §4, §6, §7, §8, §9 |

## 11. Assumption

This plan now depends on the serial interface, not just assumes it survives: the panel is drawn
over that line, so dropping the transport would take the display with it. If the serial protocol
goes, so does §5, and the device's panel becomes either a local page again or nothing.
