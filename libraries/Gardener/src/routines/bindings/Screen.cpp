#include "Build.h"

#if GARDENER_PANEL

#include <stdio.h>

#include "PanelContext.h"
#include "gfx/Fonts.h"
#include "hal/Display.h"
#include "panel/Units.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"
#include "storage/Config.h"
#include "storage/DeviceConfig.h"

namespace gardener {
namespace berry {
namespace {

Display& panel() { return context->display(); }

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
// face argument's does.
uint8_t argColor(bvm* vm, int index, const char* function, uint8_t fallback) {
  if (be_top(vm) < index || be_isnil(vm, index)) return fallback;
  const bool integer = be_isint(vm, index);
  const bint value = integer ? be_toint(vm, index) : 0;
  if (!integer || (value != 0 && value != 1)) {
    char message[64];
    snprintf(message, sizeof(message), "screen.%s: colour 0 or 1 expected", function);
    be_raise(vm, integer ? "value_error" : "type_error", message);
  }
  return static_cast<uint8_t>(value);
}

int screenClear(bvm* vm) {
  panel().clear(argColor(vm, 1, "draw_clear", Paper));
  be_return_nil(vm);
}

int screenInvert(bvm* vm) {
  panel().invert(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4));
  be_return_nil(vm);
}

int screenFlush(bvm* vm) {
  panel().display();
  be_return_nil(vm);
}

int screenWidth(bvm* vm) {
  be_pushint(vm, panel().width());
  be_return(vm);
}

int screenHeight(bvm* vm) {
  be_pushint(vm, panel().height());
  be_return(vm);
}

int screenPageWidth(bvm* vm) {
  be_pushint(vm, panel().pageWidth());
  be_return(vm);
}

int screenPageHeight(bvm* vm) {
  be_pushint(vm, panel().pageHeight());
  be_return(vm);
}

int screenPixel(bvm* vm) {
  panel().pixel(argInt(vm, 1), argInt(vm, 2), argColor(vm, 3, "draw_pixel", Ink));
  be_return_nil(vm);
}

int screenLine(bvm* vm) {
  panel().line(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4),
               argColor(vm, 5, "draw_line", Ink));
  be_return_nil(vm);
}

int screenHLine(bvm* vm) {
  panel().hLine(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3),
                argColor(vm, 4, "draw_hline", Ink));
  be_return_nil(vm);
}

int screenVLine(bvm* vm) {
  panel().vLine(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3),
                argColor(vm, 4, "draw_vline", Ink));
  be_return_nil(vm);
}

int screenRect(bvm* vm) {
  panel().rect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4),
               argColor(vm, 5, "draw_rect", Ink));
  be_return_nil(vm);
}

int screenFillRect(bvm* vm) {
  panel().fillRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4),
                   argColor(vm, 5, "draw_fillrect", Ink));
  be_return_nil(vm);
}

int screenRoundRect(bvm* vm) {
  panel().roundRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4), argInt(vm, 5),
                    argColor(vm, 6, "draw_roundrect", Ink));
  be_return_nil(vm);
}

int screenFillRoundRect(bvm* vm) {
  panel().fillRoundRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4),
                        argInt(vm, 5), argColor(vm, 6, "draw_fillroundrect", Ink));
  be_return_nil(vm);
}

int screenCircle(bvm* vm) {
  panel().circle(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3),
                 argColor(vm, 4, "draw_circle", Ink));
  be_return_nil(vm);
}

int screenFillCircle(bvm* vm) {
  panel().fillCircle(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3),
                     argColor(vm, 4, "draw_fillcircle", Ink));
  be_return_nil(vm);
}

int screenEllipse(bvm* vm) {
  panel().ellipse(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4),
                  argColor(vm, 5, "draw_ellipse", Ink));
  be_return_nil(vm);
}

int screenFillEllipse(bvm* vm) {
  panel().fillEllipse(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4),
                      argColor(vm, 5, "draw_fillellipse", Ink));
  be_return_nil(vm);
}

int screenTriangle(bvm* vm) {
  panel().triangle(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4), argInt(vm, 5),
                   argInt(vm, 6), argColor(vm, 7, "draw_triangle", Ink));
  be_return_nil(vm);
}

int screenFillTriangle(bvm* vm) {
  panel().fillTriangle(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4), argInt(vm, 5),
                       argInt(vm, 6), argColor(vm, 7, "draw_filltriangle", Ink));
  be_return_nil(vm);
}

int screenRotatedRect(bvm* vm) {
  panel().rotatedRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4), argInt(vm, 5),
                      argColor(vm, 6, "draw_rotatedrect", Ink));
  be_return_nil(vm);
}

int screenFillRotatedRect(bvm* vm) {
  panel().fillRotatedRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4),
                          argInt(vm, 5), argColor(vm, 6, "draw_fillrotatedrect", Ink));
  be_return_nil(vm);
}

int screenText(bvm* vm) {
  if (be_top(vm) < 5) {
    be_raise(vm, "type_error", "screen.draw_text(x, y, size, inverted, text[, font])");
  }
  // Berry's order is the drawing's: x, y, size, inverted, text, and the face is the call's rather
  // than the display's. The Display takes the string before the size, so the last two arguments
  // swap on the way through.
  const int16_t x = argInt(vm, 1);
  int16_t y = argInt(vm, 2);
  const uint8_t size = (uint8_t)argInt(vm, 3);
  const bool inverted = be_tobool(vm, 4);
  const char* value = argText(vm, 5);
  const gfx::Font* face = argFont(vm, 6, "draw_text");
  panel().setFont(face);
  // `y` is the top of the glyph box whichever face is set. The built-in face is drawn from its top
  // already; a compiled face is drawn from its baseline, so the ink box's height is what moves the
  // corner down to the top — the same height text_bounds answers.
  if (face != nullptr) {
    int16_t width = 0;
    int16_t height = 0;
    panel().textBounds(value, size, width, height);
    y += height;
  }
  panel().text(x, y, value, size, inverted);
  // Restoring the built-in face is what lets a page hand the panel to the shared settings editor
  // without the face it drew in following it there.
  panel().setFont(nullptr);
  be_return_nil(vm);
}

// The ink box of a string in the face it will be drawn in, which is what centring a proportional
// face or placing a suffix after it needs. The record is a `bounds`: the same two numbers
// Display::textBounds answers.
int screenBounds(bvm* vm) {
  if (be_top(vm) < 2) {
    be_raise(vm, "type_error", "screen.text_bounds(text, size[, font])");
  }
  const gfx::Font* face = argFont(vm, 3, "text_bounds");
  int16_t width = 0;
  int16_t height = 0;
  panel().setFont(face);
  panel().textBounds(argText(vm, 1), (uint8_t)argInt(vm, 2), width, height);
  panel().setFont(nullptr);

  const int record = beginRecord(vm, "bounds");
  if (record < 0) be_return_nil(vm);
  setInt(vm, record, "w", width);
  setInt(vm, record, "h", height);
  be_return(vm);
}

// One glyph of the built-in 5x7 face by its code, for the control-range symbols a page cannot
// reach through draw_text: print() reads them as control characters. Size defaults to 1.
int screenGlyph(bvm* vm) {
  if (be_top(vm) < 3) {
    be_raise(vm, "type_error", "screen.draw_glyph(x, y, code[, size])");
  }
  const uint8_t size = (be_top(vm) >= 4 && !be_isnil(vm, 4)) ? (uint8_t)argInt(vm, 4) : 1;
  panel().glyph(argInt(vm, 1), argInt(vm, 2), (uint8_t)argInt(vm, 3), size);
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
  panel().bitmap(argInt(vm, 1), argInt(vm, 2), bits);
  be_return_nil(vm);
}

// The unit the panel prints, as the same fields panel/Units.h holds: a page formats and steps a
// target in the unit on the glass without carrying a table of its own.
int screenUnit(bvm* vm) {
  DisplayUnit unit = DisplayUnit::Kpa;
  parseDisplayUnit(context->config().get<String>(DeviceSection, UnitKey, "").c_str(), unit);
  const DisplayUnitInfo& info = displayUnitInfo(unit);
  const int record = beginRecord(vm, "unit");
  if (record < 0) be_return_nil(vm);
  setText(vm, record, "name", info.name);
  setText(vm, record, "suffix", info.suffix);
  setReal(vm, record, "per_kpa", info.perKpa);
  setReal(vm, record, "whole_step", info.wholeStep);
  setInt(vm, record, "decimals", (bint)displayDecimals(unit));
  be_return(vm);
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
    {"unit", screenUnit},
    {nullptr, nullptr},
};

}  // namespace

void installScreenModule(bvm* vm) { installModule(vm, "screen", ScreenLibrary); }

}  // namespace berry
}  // namespace gardener
#endif
