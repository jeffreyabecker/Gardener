#include "app/serial/SerialLink.h"

#include <Arduino.h>

#include "app/serial/LineGrammar.h"
#include "app/serial/SerialSurface.h"

namespace gardener {
namespace {

// The eighteen operators, and the only place they are listed: the name, the coordinates that
// follow, whether a string follows those, and the call itself. Seventeen of them are the same
// shape, which is what makes a table worth having over seventeen blocks that each have to remember
// to refuse a stray argument; the eighteenth is `text`, whose last argument is a string to the end
// of the line and whose scale is checked before anything is drawn.
const DrawOp kOps[] = {
    {"clear", 0, false, [](Display& d, const int16_t*, const char*) { d.clear(); }},
    {"pixel", 2, false, [](Display& d, const int16_t* a, const char*) { d.pixel(a[0], a[1]); }},
    {"line", 4, false,
     [](Display& d, const int16_t* a, const char*) { d.line(a[0], a[1], a[2], a[3]); }},
    {"hline", 3, false, [](Display& d, const int16_t* a, const char*) { d.hLine(a[0], a[1], a[2]); }},
    {"vline", 3, false, [](Display& d, const int16_t* a, const char*) { d.vLine(a[0], a[1], a[2]); }},
    {"rect", 4, false,
     [](Display& d, const int16_t* a, const char*) { d.rect(a[0], a[1], a[2], a[3]); }},
    {"fillrect", 4, false,
     [](Display& d, const int16_t* a, const char*) { d.fillRect(a[0], a[1], a[2], a[3]); }},
    {"roundrect", 5, false,
     [](Display& d, const int16_t* a, const char*) { d.roundRect(a[0], a[1], a[2], a[3], a[4]); }},
    {"fillroundrect", 5, false, [](Display& d, const int16_t* a, const char*) {
       d.fillRoundRect(a[0], a[1], a[2], a[3], a[4]);
     }},
    {"circle", 3, false, [](Display& d, const int16_t* a, const char*) { d.circle(a[0], a[1], a[2]); }},
    {"fillcircle", 3, false,
     [](Display& d, const int16_t* a, const char*) { d.fillCircle(a[0], a[1], a[2]); }},
    {"ellipse", 4, false,
     [](Display& d, const int16_t* a, const char*) { d.ellipse(a[0], a[1], a[2], a[3]); }},
    {"fillellipse", 4, false,
     [](Display& d, const int16_t* a, const char*) { d.fillEllipse(a[0], a[1], a[2], a[3]); }},
    {"triangle", 6, false, [](Display& d, const int16_t* a, const char*) {
       d.triangle(a[0], a[1], a[2], a[3], a[4], a[5]);
     }},
    {"filltriangle", 6, false, [](Display& d, const int16_t* a, const char*) {
       d.fillTriangle(a[0], a[1], a[2], a[3], a[4], a[5]);
     }},
    {"rotatedrect", 5, false, [](Display& d, const int16_t* a, const char*) {
       d.rotatedRect(a[0], a[1], a[2], a[3], a[4]);
     }},
    {"fillrotatedrect", 5, false, [](Display& d, const int16_t* a, const char*) {
       d.fillRotatedRect(a[0], a[1], a[2], a[3], a[4]);
     }},
    // `x y size inverted <string>`: the string is the rest of the line, and `size` scales the font
    // the device ships rather than choosing one.
    {"text", 4, true, [](Display& d, const int16_t* a, const char* text) {
       d.text(a[0], a[1], text, (uint8_t)a[2], a[3] != 0);
     }},
};

}  // namespace

const DrawOp* drawOpTable(uint8_t& count) {
  count = (uint8_t)(sizeof(kOps) / sizeof(kOps[0]));
  return kOps;
}

// `panel draw <op> …`: the op is the next token, and everything after it belongs to the op. The
// coordinates are positional by design — a frame is twenty lines or more, and spelling the geometry
// `key=value` would multiply the bytes on the one command where volume matters.
void SerialLink::drawOps(char* rest) {
  if (!rest) {
    errBadArgument(nullptr);
    return;
  }

  Tokens tokens(rest);
  char* name = tokens.next();
  lower(name);

  uint8_t count = 0;
  const DrawOp* ops = drawOpTable(count);
  for (uint8_t i = 0; i < count; i++) {
    if (!same(name, ops[i].name)) continue;
    applyOp(ops[i], tokens);
    return;
  }
  errUnknown(name);
}

void SerialLink::applyOp(const DrawOp& op, Tokens& tokens) {
  int16_t args[6];  // the widest op is a triangle
  for (uint8_t i = 0; i < op.args; i++) {
    char* token = tokens.next();
    if (!token || !parseCoord(token, args[i])) {
      errBadArgument(token);
      return;
    }
  }

  const char* text = nullptr;
  if (op.string) {
    // The one op with a trailing string is also the one with a scale to check, so the range check
    // lives here rather than in a column of the table nothing else would read.
    if ((float)args[2] < DRAW_SIZE_MIN || (float)args[2] > DRAW_SIZE_MAX) {
      String value = String((int)args[2]);
      errOutOfRange("size", value.c_str(), &DRAW_SIZE_MIN, &DRAW_SIZE_MAX);
      return;
    }
    text = tokens.rest();
    if (!text) {
      errBadArgument(nullptr);
      return;
    }
  } else if (tokens.rest() != nullptr) {
    // An argument left over is an error rather than a second drawing: `panel draw pixel 1 2 3` is
    // not a pixel.
    errBadArgument(tokens.rest());
    return;
  }

  op.call(display_, args, text);

  Items out;
  out.add("result", "draw");
  out.add("op", op.name);
  ok(out.text());
}

}  // namespace gardener