#include "Build.h"

#if GARDENER_PANEL

#include <stdio.h>
#include <string.h>

#include <Gfx.h>

#include "PanelContext.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"

namespace gardener {
namespace berry {
namespace {

// The frame every verb writes into: one canvas over the panel's own buffer, shared with the page
// and the band, so the flush a page asks for sends whatever any of them drew.
gfx::Canvas& frame() { return context->canvas(); }

// The face named by an optional argument; nil, an empty name and "builtin" all mean the built-in
// 5x7, which is also what an absent argument means. `function` is the Berry spelling used in the
// error, so a mistake names the call that made it. The messages are stack buffers because be_raise
// longjmps past any C++ destructor a String would need here.
const gfx::Font* argFont(bvm* vm, int index, const char* function) {
  if (be_top(vm) < index || be_isnil(vm, index)) return nullptr;
  if (!be_isstring(vm, index)) {
    char message[64];
    snprintf(message, sizeof(message), "screen.%s: font name expected", function);
    be_raise(vm, "type_error", message);
  }
  const gfx::Font* face = nullptr;
  const char* name = argText(vm, index);
  if (name[0] == '\0') return nullptr;
  if (!gfx::fonts::find(name, face)) {
    char message[64];
    snprintf(message, sizeof(message), "screen.%s: unknown font %s", function, name);
    be_raise(vm, "value_error", message);
  }
  return face;
}

// The colour a drawing call asks for: an optional last argument, and the fallback the call names
// when it is absent or nil. Only 0 — paper — and 1 — ink — are colours, so a call can erase as well
// as draw; nothing else is a third shade. The message names the call that got it wrong, as the
// face argument's does. What comes back is the byte the frame holds, so the verbs pass it straight
// to a shape.
uint8_t argColor(bvm* vm, int index, const char* function, uint8_t fallback) {
  if (be_top(vm) < index || be_isnil(vm, index)) return fallback;
  const bool integer = be_isint(vm, index);
  const bint value = integer ? be_toint(vm, index) : 0;
  if (!integer || (value != 0 && value != 1)) {
    char message[64];
    snprintf(message, sizeof(message), "screen.%s: colour 0 or 1 expected", function);
    be_raise(vm, integer ? "value_error" : "type_error", message);
  }
  return value == 0 ? gfx::ui::Paper : gfx::ui::Ink;
}

int screenClear(bvm* vm) {
  const uint8_t color = argColor(vm, 1, "draw_clear", gfx::ui::Paper);
  gfx::Canvas& canvas = frame();
  canvas.draw(gfx::shapes::FillRect(0, 0, canvas.width(), canvas.height(), color));
  be_return_nil(vm);
}

int screenInvert(bvm* vm) {
  frame().invert(gfx::Rect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4)));
  be_return_nil(vm);
}

int screenFlush(bvm* vm) {
  frame().flush(context->display());
  be_return_nil(vm);
}

int screenWidth(bvm* vm) {
  be_pushint(vm, frame().width());
  be_return(vm);
}

int screenHeight(bvm* vm) {
  be_pushint(vm, frame().height());
  be_return(vm);
}

int screenPageWidth(bvm* vm) {
  be_pushint(vm, frame().width());
  be_return(vm);
}

int screenPageHeight(bvm* vm) {
  be_pushint(vm, frame().height());
  be_return(vm);
}

int screenPixel(bvm* vm) {
  frame().setPixel(argInt(vm, 1), argInt(vm, 2), argColor(vm, 3, "draw_pixel", gfx::ui::Ink));
  be_return_nil(vm);
}

int screenLine(bvm* vm) {
  frame().draw(gfx::shapes::Line(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4),
                                argColor(vm, 5, "draw_line", gfx::ui::Ink)));
  be_return_nil(vm);
}

int screenHLine(bvm* vm) {
  frame().draw(gfx::shapes::HLine(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3),
                                  argColor(vm, 4, "draw_hline", gfx::ui::Ink)));
  be_return_nil(vm);
}

int screenVLine(bvm* vm) {
  frame().draw(gfx::shapes::VLine(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3),
                                  argColor(vm, 4, "draw_vline", gfx::ui::Ink)));
  be_return_nil(vm);
}

int screenRect(bvm* vm) {
  frame().draw(gfx::shapes::Rect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4),
                                 argColor(vm, 5, "draw_rect", gfx::ui::Ink)));
  be_return_nil(vm);
}

int screenFillRect(bvm* vm) {
  frame().draw(gfx::shapes::FillRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4),
                                     argColor(vm, 5, "draw_fillrect", gfx::ui::Ink)));
  be_return_nil(vm);
}

int screenRoundRect(bvm* vm) {
  frame().draw(gfx::shapes::RoundRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4),
                                      argInt(vm, 5),
                                      argColor(vm, 6, "draw_roundrect", gfx::ui::Ink)));
  be_return_nil(vm);
}

int screenFillRoundRect(bvm* vm) {
  frame().draw(gfx::shapes::FillRoundRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3),
                                          argInt(vm, 4), argInt(vm, 5),
                                          argColor(vm, 6, "draw_fillroundrect", gfx::ui::Ink)));
  be_return_nil(vm);
}

int screenCircle(bvm* vm) {
  frame().draw(gfx::shapes::Circle(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3),
                                   argColor(vm, 4, "draw_circle", gfx::ui::Ink)));
  be_return_nil(vm);
}

int screenFillCircle(bvm* vm) {
  frame().draw(gfx::shapes::FillCircle(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3),
                                       argColor(vm, 4, "draw_fillcircle", gfx::ui::Ink)));
  be_return_nil(vm);
}

int screenEllipse(bvm* vm) {
  frame().draw(gfx::shapes::Ellipse(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4),
                                    argColor(vm, 5, "draw_ellipse", gfx::ui::Ink)));
  be_return_nil(vm);
}

int screenFillEllipse(bvm* vm) {
  frame().draw(gfx::shapes::FillEllipse(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3),
                                        argInt(vm, 4),
                                        argColor(vm, 5, "draw_fillellipse", gfx::ui::Ink)));
  be_return_nil(vm);
}

int screenTriangle(bvm* vm) {
  frame().draw(gfx::shapes::Triangle(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4),
                                     argInt(vm, 5), argInt(vm, 6),
                                     argColor(vm, 7, "draw_triangle", gfx::ui::Ink)));
  be_return_nil(vm);
}

int screenFillTriangle(bvm* vm) {
  frame().draw(gfx::shapes::FillTriangle(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3),
                                         argInt(vm, 4), argInt(vm, 5), argInt(vm, 6),
                                         argColor(vm, 7, "draw_filltriangle", gfx::ui::Ink)));
  be_return_nil(vm);
}

int screenRotatedRect(bvm* vm) {
  frame().draw(gfx::shapes::RotatedRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3),
                                        argInt(vm, 4), argInt(vm, 5),
                                        argColor(vm, 6, "draw_rotatedrect", gfx::ui::Ink)));
  be_return_nil(vm);
}

int screenFillRotatedRect(bvm* vm) {
  frame().draw(gfx::shapes::FillRotatedRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3),
                                            argInt(vm, 4), argInt(vm, 5),
                                            argColor(vm, 6, "draw_fillrotatedrect",
                                                     gfx::ui::Ink)));
  be_return_nil(vm);
}

int screenText(bvm* vm) {
  if (be_top(vm) < 5) {
    be_raise(vm, "type_error", "screen.draw_text(x, y, size, inverted, text[, font])");
  }
  // Berry's order is the drawing's: x, y, size, inverted, text, and the face is the call's rather
  // than the panel's, so nothing is left set for the next draw.
  const int16_t x = argInt(vm, 1);
  const int16_t y = argInt(vm, 2);
  const uint8_t size = (uint8_t)argInt(vm, 3);
  const bool inverted = be_tobool(vm, 4);
  const char* value = argText(vm, 5);
  if (value == nullptr) be_return_nil(vm);
  const gfx::Font* face = argFont(vm, 6, "draw_text");
  gfx::Canvas& canvas = frame();

  // The built-in face is drawn from the top of a five-by-seven cell; Gfx Text draws from the
  // baseline seven rows below it.
  if (face == nullptr) {
    const int16_t baseline = static_cast<int16_t>(y + 7 * size);
    if (inverted) {
      const int16_t width = static_cast<int16_t>(6 * size * strlen(value));
      const int16_t height = static_cast<int16_t>(8 * size);
      canvas.draw(gfx::shapes::FillRect(x, y, width, height, gfx::ui::Ink));
      canvas.draw(gfx::shapes::Text(x, baseline, value, nullptr, size, gfx::ui::Paper));
    } else {
      canvas.draw(gfx::shapes::Text(x, baseline, value, nullptr, size, gfx::ui::Ink));
    }
    be_return_nil(vm);
  }

  // A compiled face has no background, so inverted draws the same as ink. `y` is the top of the
  // glyph box either way, which is the ink box's height above the face's baseline.
  const gfx::shapes::TextBox box = gfx::shapes::measureText(face, value, size);
  canvas.draw(gfx::shapes::Text(x, static_cast<int16_t>(y + box.height), value, face, size,
                                gfx::ui::Ink));
  be_return_nil(vm);
}

// The ink box of a string in the face it will be drawn in, which is what centring a proportional
// face or placing a suffix after it needs. The record is a `bounds`: the two numbers
// measureText answers.
int screenBounds(bvm* vm) {
  if (be_top(vm) < 2) {
    be_raise(vm, "type_error", "screen.text_bounds(text, size[, font])");
  }
  const gfx::Font* face = argFont(vm, 3, "text_bounds");
  const gfx::shapes::TextBox box =
      gfx::shapes::measureText(face, argText(vm, 1), (uint8_t)argInt(vm, 2));

  const int record = beginRecord(vm, "bounds");
  if (record < 0) be_return_nil(vm);
  setInt(vm, record, "w", box.width);
  setInt(vm, record, "h", box.height);
  be_return(vm);
}

// One glyph of the built-in 5x7 face by its code, for the control-range symbols a page cannot
// reach through draw_text: print() reads them as control characters. Size defaults to 1.
int screenGlyph(bvm* vm) {
  if (be_top(vm) < 3) {
    be_raise(vm, "type_error", "screen.draw_glyph(x, y, code[, size])");
  }
  const uint8_t size = (be_top(vm) >= 4 && !be_isnil(vm, 4)) ? (uint8_t)argInt(vm, 4) : 1;
  gfx::shapes::drawGlyph(frame(), argInt(vm, 1), argInt(vm, 2), (uint8_t)argInt(vm, 3), size,
                         gfx::ui::Ink);
  be_return_nil(vm);
}

// A packed one-bit image: bits[0] is the width, bits[1] the height, then the pixels row-major,
// most-significant bit first, each row padded to a whole byte. Ink bits are drawn; paper bits leave
// what is under them.
int screenBitmap(bvm* vm) {
  if (be_top(vm) < 3 || !be_isbytes(vm, 3)) {
    be_raise(vm, "type_error", "screen.draw_bitmap(x, y, bytes)");
  }
  size_t length = 0;
  const uint8_t* bits = static_cast<const uint8_t*>(be_tobytes(vm, 3, &length));
  if (bits == nullptr) {
    be_raise(vm, "type_error", "screen.draw_bitmap(x, y, bytes)");
  }
  frame().draw(gfx::shapes::MonoBitmap(argInt(vm, 1), argInt(vm, 2), bits, gfx::ui::Ink));
  be_return_nil(vm);
}

const bnfuncinfo ScreenLibrary[] = {
    {"draw_clear", screenClear},
    {"draw_pixel", screenPixel},
    {"draw_line", screenLine},
    {"draw_hline", screenHLine},
    {"draw_vline", screenVLine},
    {"draw_rect", screenRect},
    {"draw_fillrect", screenFillRect},
    {"draw_invert", screenInvert},
    {"draw_roundrect", screenRoundRect},
    {"draw_fillroundrect", screenFillRoundRect},
    {"draw_circle", screenCircle},
    {"draw_fillcircle", screenFillCircle},
    {"draw_ellipse", screenEllipse},
    {"draw_fillellipse", screenFillEllipse},
    {"draw_triangle", screenTriangle},
    {"draw_filltriangle", screenFillTriangle},
    {"draw_rotatedrect", screenRotatedRect},
    {"draw_fillrotatedrect", screenFillRotatedRect},
    {"draw_text", screenText},
    {"text_bounds", screenBounds},
    {"draw_glyph", screenGlyph},
    {"draw_bitmap", screenBitmap},
    {"width", screenWidth},
    {"height", screenHeight},
    {"page_width", screenPageWidth},
    {"page_height", screenPageHeight},
    {"flush", screenFlush},
    {nullptr, nullptr},
};

}  // namespace

void installScreenModule(bvm* vm) { installModule(vm, "screen", ScreenLibrary); }

}  // namespace berry
}  // namespace gardener
#endif
