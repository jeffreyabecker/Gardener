#include "app/SerialLink.h"

#include <Arduino.h>

#include "app/LineGrammar.h"

namespace gardener {
namespace {

constexpr const char* kDrawOps =
    "clear|pixel|line|hline|vline|rect|fillrect|roundrect|fillroundrect|circle|fillcircle|"
    "ellipse|fillellipse|triangle|filltriangle|rotatedrect|fillrotatedrect|text";

}  // namespace

// --- drawing ---------------------------------------------------------------
//
// The panel half of the protocol: `draw <op> <numbers>`, and `flush` to put the frame on the
// glass. This is a display driver reached over the wire rather than through a call, which is why
// the op list reads like the verbs on hal/Display.h and has to be kept in step with them.

// Takes the numbers an op asks for and refuses anything left over, so `draw pixel 1 2 3` is an
// error rather than a pixel. The reply lives in apply() so the sixteen ops that take coordinates
// cannot each get it slightly wrong.
bool SerialLink::coords(Tokens& tokens, size_t count, int16_t* out) {
  for (size_t i = 0; i < count; i++) {
    char* token = tokens.next();
    if (!token || !parseCoord(token, out[i])) return false;
  }
  return true;
}

void SerialLink::draw(Tokens& tokens) {
  tokens.next();  // the verb, already recognised

  char* opToken = tokens.next();
  if (!opToken) {
    err("bad_argument");
    return;
  }
  lower(opToken);
  const char* op = opToken;

  if (same(op, "help")) {
    String payload = "ops=";
    payload += kDrawOps;
    ok(payload.c_str());
    return;
  }
  if (same(op, "clear")) {
    if (tokens.rest() != nullptr) {
      err("bad_argument");
      return;
    }
    display_.clear();
    ok("drawn");
    return;
  }
  if (same(op, "pixel")) {
    apply(tokens, 2, [](Display& d, const int16_t* a) { d.pixel(a[0], a[1]); });
    return;
  }
  if (same(op, "line")) {
    apply(tokens, 4, [](Display& d, const int16_t* a) { d.line(a[0], a[1], a[2], a[3]); });
    return;
  }
  if (same(op, "hline")) {
    apply(tokens, 3, [](Display& d, const int16_t* a) { d.hLine(a[0], a[1], a[2]); });
    return;
  }
  if (same(op, "vline")) {
    apply(tokens, 3, [](Display& d, const int16_t* a) { d.vLine(a[0], a[1], a[2]); });
    return;
  }
  if (same(op, "rect")) {
    apply(tokens, 4, [](Display& d, const int16_t* a) { d.rect(a[0], a[1], a[2], a[3]); });
    return;
  }
  if (same(op, "fillrect")) {
    apply(tokens, 4, [](Display& d, const int16_t* a) { d.fillRect(a[0], a[1], a[2], a[3]); });
    return;
  }
  if (same(op, "roundrect")) {
    apply(tokens, 5,
          [](Display& d, const int16_t* a) { d.roundRect(a[0], a[1], a[2], a[3], a[4]); });
    return;
  }
  if (same(op, "fillroundrect")) {
    apply(tokens, 5,
          [](Display& d, const int16_t* a) { d.fillRoundRect(a[0], a[1], a[2], a[3], a[4]); });
    return;
  }
  if (same(op, "circle")) {
    apply(tokens, 3, [](Display& d, const int16_t* a) { d.circle(a[0], a[1], a[2]); });
    return;
  }
  if (same(op, "fillcircle")) {
    apply(tokens, 3, [](Display& d, const int16_t* a) { d.fillCircle(a[0], a[1], a[2]); });
    return;
  }
  if (same(op, "ellipse")) {
    apply(tokens, 4, [](Display& d, const int16_t* a) { d.ellipse(a[0], a[1], a[2], a[3]); });
    return;
  }
  if (same(op, "fillellipse")) {
    apply(tokens, 4, [](Display& d, const int16_t* a) { d.fillEllipse(a[0], a[1], a[2], a[3]); });
    return;
  }
  if (same(op, "triangle")) {
    apply(tokens, 6, [](Display& d, const int16_t* a) {
      d.triangle(a[0], a[1], a[2], a[3], a[4], a[5]);
    });
    return;
  }
  if (same(op, "filltriangle")) {
    apply(tokens, 6, [](Display& d, const int16_t* a) {
      d.fillTriangle(a[0], a[1], a[2], a[3], a[4], a[5]);
    });
    return;
  }
  if (same(op, "rotatedrect")) {
    apply(tokens, 5,
          [](Display& d, const int16_t* a) { d.rotatedRect(a[0], a[1], a[2], a[3], a[4]); });
    return;
  }
  if (same(op, "fillrotatedrect")) {
    apply(tokens, 5,
          [](Display& d, const int16_t* a) { d.fillRotatedRect(a[0], a[1], a[2], a[3], a[4]); });
    return;
  }
  if (same(op, "text")) {
    int16_t args[4];
    if (!coords(tokens, 4, args)) {
      err("bad_argument");
      return;
    }
    if (args[2] < 1 || args[2] > 8) {
      err("out_of_range");
      return;
    }
    const char* value = tokens.rest();
    if (!value) {
      err("bad_argument");
      return;
    }
    display_.text(args[0], args[1], value, (uint8_t)args[2], args[3] != 0);
    ok("drawn");
    return;
  }

  err("unknown_command");
}

}  // namespace gardener
