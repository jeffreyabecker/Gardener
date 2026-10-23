#include "GestureCallout.h"

#include <string.h>

#include "BitmapPanel.h"

namespace gardener {

namespace {

std::string uppercase(std::string text) {
  for (char& c : text) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
  }
  return text;
}

bool isDigits(const std::string& text) {
  if (text.empty()) return false;
  for (char c : text) {
    if (c < '0' || c > '9') return false;
  }
  return true;
}

std::string keyLabel(const std::string& key) {
  if (key == "ok") return "OK";
  if (key == "cancel") return "CANCEL";
  if (key == "up") return "UP";
  if (key == "down") return "DOWN";
  if (key == "forward") return "FORWARD";
  if (key == "back") return "BACK";
  return uppercase(key);
}

// One part of a gesture, key:form: single keys name themselves, a double appends the times sign, a
// hold its seconds, and anything else falls back to the raw token uppercased.
std::string partLabel(const std::string& part) {
  const size_t colon = part.find(':');
  const std::string key = part.substr(0, colon);
  const std::string form = colon == std::string::npos ? "" : part.substr(colon + 1);

  std::string label = keyLabel(key);
  if (form.empty() || form == "single") {
    return label;
  }
  if (form == "double") {
    return label + "\xD7" "2";  // OK×2
  }
  if (isDigits(form)) {
    return label + " " + form + "s";  // CANCEL 5s
  }
  return uppercase(key + ":" + form);
}

// Whether the classic face has no ink for a character, so a callout can fall back rather than
// print a blank where a glyph is missing in whichever GFX library check.py found.
bool glyphBlank(uint8_t c) {
  const uint8_t* font = BitmapPanel::classicFont();
  if (c >= 176) c++;
  for (int i = 0; i < 5; i++) {
    if (font[c * 5 + i] != 0) return false;
  }
  return true;
}

void setPacked(uint8_t* packed, int16_t width, int16_t height, int16_t x, int16_t y) {
  if (x < 0 || y < 0 || x >= width || y >= height) return;
  packed[y * ((width + 7) / 8) + x / 8] |= (uint8_t)(0x80 >> (x & 7));
}

// The classic 5x7 face, drawn into a packed frame the same way BitmapPanel draws it onto the
// panel: five columns of eight rows, least-significant bit the top.
void drawGlyph(uint8_t* packed, int16_t width, int16_t height, int16_t x, int16_t y, uint8_t c) {
  if (glyphBlank(c)) c = 'x';
  const uint8_t* font = BitmapPanel::classicFont();
  if (c >= 176) c++;
  for (int i = 0; i < 5; i++) {
    uint8_t line = font[c * 5 + i];
    for (int j = 0; j < 8; j++, line >>= 1) {
      if (line & 1) setPacked(packed, width, height, x + i, y + j);
    }
  }
}

}  // namespace

std::string gestureLabel(const std::string& gesture) {
  std::string out;
  size_t start = 0;
  while (start <= gesture.size()) {
    size_t end = gesture.find('+', start);
    if (end == std::string::npos) end = gesture.size();
    if (!out.empty()) out += " + ";
    out += partLabel(gesture.substr(start, end - start));
    start = end + 1;
  }
  return out;
}

void drawGestureStrip(uint8_t* packed, int16_t width, int16_t height, const std::string& label) {
  const int16_t rowBytes = (width + 7) / 8;
  constexpr int16_t kStripHeight = 10;  // one pad, the 8-row face, one pad
  const int16_t stripRows = kStripHeight < height ? kStripHeight : height;
  for (int16_t y = 0; y < stripRows; y++) {
    memset(packed + y * rowBytes, 0, (size_t)rowBytes);
  }

  const int16_t labelWidth = 6 * (int16_t)label.size();  // five columns plus the spacer
  int16_t x = (width - labelWidth) / 2;
  const int16_t y = 1;
  for (char c : label) {
    drawGlyph(packed, width, height, x, y, (uint8_t)c);
    x += 6;
  }
}

}  // namespace gardener
