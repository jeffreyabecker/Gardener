# Plan: a display interface, and Adafruit behind it

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

This plan puts an abstract base class in front of it: `hal/Display.h` for the verbs a monochrome
panel is asked for, and `hal/AdafruitDisplay` implementing it over Adafruit with the colours
supplied.

`DECISION:` this reintroduces a display abstraction that was deleted earlier in the project's
life, and the difference is worth stating. The deleted one was a concrete pass-through — it
added a hop in front of every call and no seam, and the page still had to be retargeted anyway
when it went. This one is an interface with the colour policy inside it, and it exists so that
`StatusPage` has a second implementation to be tested against. If that second implementation is
never written, this is a wrapper with one implementation and should be deleted again.

## 1. The interface

`hal/Display.h`, no colour arguments anywhere, and eight groups of verbs:

| Group | Verbs |
| --- | --- |
| the panel | `begin`, `flush` |
| geometry | `width`, `height`, `rotation`, `setRotation` |
| the screen | `clear`, `invert` |
| pixels and lines | `pixel`, `clearRect`, `line`, `hLine`, `vLine` |
| shapes | `rect`, `fillRect`, `roundRect`, `fillRoundRect`, `circle`, `fillCircle`, `ellipse`, `fillEllipse`, `triangle`, `fillTriangle`, `rotatedRect`, `fillRotatedRect` |
| images | `bitmap` |
| text | `setFont`, `text`, `textBounds` |

`DECISION:` this is the library's whole drawing surface, not the eight calls the page makes
today, so that a page written later does not have to come back and widen it. Rev 1 proposed the
narrow list and was wrong about what the abstraction is for.

`DECISION:` (rev 3, by hand) text is stateless apart from the font. The rev 2 interface carried
`Print`, `setCursor`, `setTextSize`, `setTextWrap`, `cp437` and `xBitmap`; all six are gone, and
what is left is `setFont` plus `text`/`textBounds` taking the scale at the call. `textWidth` went
too, so measurement is one method returning both dimensions.

Two things follow from that, and they are consequences rather than objections:

* **`GFXfont` is now named in the header**, and it cannot be forward declared, because
  `gfxfont.h` typedefs it from an anonymous struct. So `hal/Display.h` includes
  `<Adafruit_GFX.h>`. Rev 2 listed `setFont` as deliberately absent for exactly this reason; a
  custom font is worth more than the rule, and the include is the price. See R6.
* **`size` stops meaning anything once a font is loaded.** Adafruit ignores the text size when a
  bitmap font is set, because the font carries its own. `text` and `textBounds` still take a
  size, so the parameter is conditional — see Q15.

`DECISION:` paper has two spellings — `clear()` for the whole panel and `clearRect()` for part of
it — because a page that blinks or updates a field needs to erase without repainting everything.

`DECISION:` `text` and its measurement are not `const`, because `getTextBounds` is not. Documented
rather than cast away.

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

* `Adafruit_GrayOLED` rather than `Adafruit_GFX`, because `SH1106G : Adafruit_GrayOLED :
  Adafruit_GFX` and only the middle one has both the draw verbs and `begin`/`display`. It also
  means any GrayOLED panel — SSD1306, SH1106, SSD1315 — drops in without the adapter changing.
* Every verb in §1 is one line here. The shapes, the rotated rects, the ellipses and the bitmaps
  are all public on `Adafruit_GFX`, virtual or not, so the adapter only forwards; the two
  `setTextSize` overloads and the two bitmap spellings are the only places a name has to be
  chosen.
* `Print` is gone with rev 3's text rework, so the adapter no longer implements `write(uint8_t)`
  as a bridge. `setFont` forwards to `Adafruit_GFX::setFont`, which is what Adafruit's fonts
  already expect.
* `setFont` takes the panel's own `GFXfont`, so this class and the header share one library type
  between them and a host fake has to define its own — see R6.
* `App` still constructs the panel, so it stays the only place that knows *which* panel is
  fitted, which is a property its own header already claims.
* The `false` in `begin` is not incidental: the STEMMA QT breakout has no reset line, so the
  library must not poke one. That reason moves into the adapter with the call.
* `kInk`/`kPaper` land here, which is the point: the page stops knowing that 1 means lit.

## 3. What the page keeps

* The alignment helpers. Adafruit has no opinion about alignment, so `textRight` and `centered`
  still earn their place — this change is about colours and verbs, not layout.
* Every layout constant, row position and size. The page looks the same on the glass.
* `widthOf()` collapses to one line over `textBounds`, taking the width and ignoring the height.

## 4. What it buys

1. `StatusPage` compiles and runs against a `FakeDisplay` that records what was drawn, so the
   arithmetic can be checked on the host without a panel. This is the payoff, and it is why the
   interface should exist at all.
2. Colour policy in one place. A second panel with inverted pixels is a different adapter, not a
   sweep through layout code.

## 5. Phases

| # | Phase | Content | Verify |
| --- | --- | --- | --- |
| 1a | The interface | `hal/Display.h` alone: the abstract class, no implementation and no caller yet | the header compiles (checked with a temporary include, which is then removed), layer rule ok |
| 1b | The adapter and the retarget | `hal/AdafruitDisplay.{h,cpp}`; `StatusPage` takes `Display&`; App constructs the adapter and draws through it | green, layer rule ok, flash and RAM deltas reported, the page's output unchanged |
| 2 | A fake and a host test | `tests/fake_display.h` plus a test for the page's arithmetic, run by a new `scripts/arduino.ps1` host task | the test fails when the arithmetic is broken |
| 3 | Docs | README: the display section and the architecture note, since `hal/` now holds the panel | green |

## 6. Risks

| id | Risk | Mitigation |
| --- | --- | --- |
| R1 | an abstraction with one implementation is speculative, and this project deleted one already | the second implementation is phase 2, and the plan says plainly that without it this should be deleted again |
| R2 | a fat interface is a commitment: every verb has to be implemented by every implementation, and a fake has to fake all of them | accepted deliberately, because the alternative was coming back to widen it. Two things keep it honest: the list is the library's surface rather than invention, and the absences are written down with reasons so the next reader sees a decision and not an omission |
| R2b | a verb nobody draws earns nothing and still has to be implemented | the adapter implementations are one line each, so the cost is the diff, not the design. If a verb turns out to be wrong, deleting it is a smaller change than adding it to a shipped interface |
| R3 | a virtual call per primitive, about fifty per render, and the page renders every loop | measured and reported in phase 1. The I2C flush next to it is orders of magnitude larger, but the claim gets a number rather than an assurance |
| R4 | `textBounds` cannot be `const` because Adafruit's measurement is not, which looks like an oversight | stated in the header comment, so the next reader sees the reason rather than a mistake |
| R5 | App keeps the panel and passes a `GrayOLED&` into the adapter, which is two objects for one display | accepted: App is the composition root and it is the only place that knows which panel is fitted. The alternative, an adapter that constructs the panel itself, hides the choice in `hal/` but takes the I2C address, the reset flag and `Wire` with it |
| R6 | a host fake needs `GFXfont` to implement `setFont`, and the type is a typedef of an anonymous struct, so no forward declaration exists to hide behind | the fake defines its own matching struct and does not include Adafruit at all, which keeps the test free of the library but couples it to the shape of one small type. Accepted for rev 3's font support; the alternative is dropping `setFont` back out |

## 7. Questions

Recommendations are marked, so "all as recommended" is a valid answer.

| # | Question | Recommendation |
| --- | --- | --- |
| Q1 | Place: `hal/Display.h` with `hal/AdafruitDisplay.{h,cpp}`? | **yes**: `hal/` is one file per physical device, and other drivers there already wrap an Adafruit library |
| Q2 | Is the interface the page's own calls, or the library's whole drawing surface? | **the library's surface** (answered in rev 2, against rev 1): a page written later should not have to come back and widen it |
| Q3 | No colour parameter anywhere in the interface: ink and paper are the adapter's constants and inversion is a `bool`? | **yes**: that is the leak this change is for |
| Q4 | The adapter wraps `Adafruit_GrayOLED&` and App keeps owning the concrete panel? | **yes**: App stays the only place that knows which panel is fitted, as its header claims |
| Q5 | One measurement method, `textBounds(value, size, w, h)`, rather than `textWidth` plus bounds? | **yes** (decided by hand in rev 3): one method that returns both, and the page ignores the height it does not use |
| Q6 | A virtual destructor on the base, even though nothing deletes through it today? | **yes**: it is an interface, and the cost is a vtable entry |
| Q7 | Phase 2: a `FakeDisplay` and a host test for the page's arithmetic, wired into `scripts/arduino.ps1`? | **yes**: this is the only reason the interface is worth having, and it is the first host test in the project |
| Q8 | Does the page keep its layout constants and alignment helpers untouched? | **yes**: the abstraction is verbs and colour only, so nothing about the layout moves |
| Q9 | Keep the name `Display`, rather than `Panel`, `Screen` or `MonoDisplay`? | **yes**: it is the name this project used before, and the role is "the thing we draw on" |
| Q10 | No firmware version bump, since nothing on the wire changes? | **yes**: `fw` now marks wire changes, and this is not one |
| Q11 | Are the exclusions right — no grayscale or RGB bitmaps, no `startWrite` family, no `setFont`? | **revised by hand in rev 3**: `setFont` is in after all, at the cost of including `<Adafruit_GFX.h>` for a type that cannot be forward declared. The other two hold, and `xBitmap` went with them |
| Q12 | Do the text verbs also take a `const char*` so the page's literals (`"--:--"`, `"NO SENSOR"`) stop allocating a `String` on every render? | **still open, and narrower now**: two methods, `text` and `textBounds`. The page draws literals every loop iteration, so it is the one place the "prefer String" guidance costs something measurable |
| Q13 | `size` is ignored by Adafruit once a bitmap font is loaded, so the parameter in `text`/`textBounds` is conditional. Document it, or drop it? | **document it**: that is the library's behaviour, the page draws the built-in font today, and removing the parameter would mean text scale becomes state again, which rev 3 just took out |

## 8. Status

| Revision | Change | Sections |
| --- | --- | --- |
| rev 1 | initial plan: `Display` interface, Adafruit adapter, optional host test as phase 2 | all |
| rev 2 | the interface is the library's drawing surface rather than the page's eight calls; `Display` gains `Print`; the deliberate absences are written down; the `const char*` question is raised | §1, §2, §6, §7 |
| rev 3 | the header was reworked by hand: `Print`, `setCursor`, `setTextSize`, `setTextWrap`, `cp437`, `textWidth` and `xBitmap` out, `setFont` in. The header therefore includes `<Adafruit_GFX.h>` for `GFXfont`, which cannot be forward declared; R6 and Q13 record what that costs | §1, §2, §3, §6, §7 |
