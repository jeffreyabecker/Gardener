#include <stdio.h>

#include "Panel.h"
#include "panel/Fonts.h"
#include "panel/Units.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"

namespace gardener {
namespace berry {
namespace {

Display& panel() { return gContext->display(); }

// The face named by an optional argument; nil and "builtin" both mean the built-in 5x7, which is
// also what an absent argument means. `function` is the Berry spelling used in the error, so a
// mistake names the call that made it. The messages are stack buffers because be_raise longjmps
// past any C++ destructor a String would need here.
const GFXfont* argFont(bvm* vm, int index, const char* function) {
  if (be_top(vm) < index || be_isnil(vm, index)) return nullptr;
  if (!be_isstring(vm, index)) {
    char message[64];
    snprintf(message, sizeof(message), "screen.%s: font name expected", function);
    be_raise(vm, "type_error", message);
  }
  const GFXfont* face = nullptr;
  const char* name = argText(vm, index);
  if (!fonts::find(name, face)) {
    char message[64];
    snprintf(message, sizeof(message), "screen.%s: unknown font %s", function, name);
    be_raise(vm, "value_error", message);
  }
  return face;
}

int screenClear(bvm* vm) {
  panel().clear();
  be_return_nil(vm);
}

int screenFlush(bvm* vm) {
  panel().display();
  be_return_nil(vm);
}

int screenPixel(bvm* vm) {
  panel().pixel(argInt(vm, 1), argInt(vm, 2));
  be_return_nil(vm);
}

int screenLine(bvm* vm) {
  panel().line(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4));
  be_return_nil(vm);
}

int screenHLine(bvm* vm) {
  panel().hLine(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3));
  be_return_nil(vm);
}

int screenVLine(bvm* vm) {
  panel().vLine(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3));
  be_return_nil(vm);
}

int screenRect(bvm* vm) {
  panel().rect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4));
  be_return_nil(vm);
}

int screenFillRect(bvm* vm) {
  panel().fillRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4));
  be_return_nil(vm);
}

int screenRoundRect(bvm* vm) {
  panel().roundRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4), argInt(vm, 5));
  be_return_nil(vm);
}

int screenFillRoundRect(bvm* vm) {
  panel().fillRoundRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4),
                        argInt(vm, 5));
  be_return_nil(vm);
}

int screenCircle(bvm* vm) {
  panel().circle(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3));
  be_return_nil(vm);
}

int screenFillCircle(bvm* vm) {
  panel().fillCircle(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3));
  be_return_nil(vm);
}

int screenEllipse(bvm* vm) {
  panel().ellipse(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4));
  be_return_nil(vm);
}

int screenFillEllipse(bvm* vm) {
  panel().fillEllipse(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4));
  be_return_nil(vm);
}

int screenTriangle(bvm* vm) {
  panel().triangle(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4), argInt(vm, 5),
                   argInt(vm, 6));
  be_return_nil(vm);
}

int screenFillTriangle(bvm* vm) {
  panel().fillTriangle(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4), argInt(vm, 5),
                       argInt(vm, 6));
  be_return_nil(vm);
}

int screenRotatedRect(bvm* vm) {
  panel().rotatedRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4), argInt(vm, 5));
  be_return_nil(vm);
}

int screenFillRotatedRect(bvm* vm) {
  panel().fillRotatedRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4),
                          argInt(vm, 5));
  be_return_nil(vm);
}

int screenText(bvm* vm) {
  if (be_top(vm) < 5) {
    be_raise(vm, "type_error", "screen.draw_text(x, y, size, inverted, text[, font])");
  }
  // Berry's order is the drawing's: x, y, size, inverted, text, and the face is the call's rather
  // than the display's. The Display takes the string before the size, so the last two arguments
  // swap on the way through.
  const GFXfont* face = argFont(vm, 6, "draw_text");
  panel().setFont(face);
  panel().text(argInt(vm, 1), argInt(vm, 2), argText(vm, 5), (uint8_t)argInt(vm, 3),
               be_tobool(vm, 4));
  // Restoring the built-in face is what lets a page hand the panel to the shared settings editor
  // without the face it drew in following it there.
  panel().setFont(nullptr);
  be_return_nil(vm);
}

// The ink box of a string in the face it will be drawn in, which is what centring a proportional
// face or placing a suffix after it needs. The record is the same two numbers Display::textBounds
// answers.
int screenBounds(bvm* vm) {
  if (be_top(vm) < 2) {
    be_raise(vm, "type_error", "screen.text_bounds(text, size[, font])");
  }
  const GFXfont* face = argFont(vm, 3, "text_bounds");
  int16_t width = 0;
  int16_t height = 0;
  panel().setFont(face);
  panel().textBounds(argText(vm, 1), (uint8_t)argInt(vm, 2), width, height);
  panel().setFont(nullptr);

  const int record = beginRecord(vm, "bounds");
  if (record < 0) be_return_nil(vm);
  setInt(vm, record, "w", width);
  setInt(vm, record, "h", height);
  endRecord(vm);
  be_return(vm);
}

// The unit the panel prints, as the same fields panel/Units.h holds: a page formats and steps a
// target in the unit on the glass without carrying a table of its own.
int screenUnit(bvm* vm) {
  const DisplayUnit unit = gContext->settings().unit();
  const DisplayUnitInfo& info = displayUnitInfo(unit);
  const int record = beginRecord(vm, "unit");
  if (record < 0) be_return_nil(vm);
  setText(vm, record, "name", info.name);
  setText(vm, record, "suffix", info.suffix);
  setReal(vm, record, "per_kpa", info.perKpa);
  setReal(vm, record, "whole_step", info.wholeStep);
  setInt(vm, record, "decimals", (bint)displayDecimals(unit));
  endRecord(vm);
  be_return(vm);
}

const bnfuncinfo kScreenLibrary[] = {
    {"draw_clear", screenClear},
    {"draw_pixel", screenPixel},
    {"draw_line", screenLine},
    {"draw_hline", screenHLine},
    {"draw_vline", screenVLine},
    {"draw_rect", screenRect},
    {"draw_fillrect", screenFillRect},
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
    {"flush", screenFlush},
    {"unit", screenUnit},
    {nullptr, nullptr},
};

}  // namespace

void installScreenModule(bvm* vm) { installModule(vm, "screen", kScreenLibrary); }

}  // namespace berry
}  // namespace gardener
