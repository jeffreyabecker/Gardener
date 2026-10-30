#include "GestureStrip.h"

#include "BuiltinFont.h"
#include "Canvas.h"
#include "shapes/Text.h"

namespace gardener {

namespace {

// The built-in face's cell: five columns of ink and the blank column its advance leaves, eight rows
// tall.
constexpr int16_t CellWidth = 6;
constexpr int16_t CellHeight = 8;

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
    return label + "\xD7" "2";  // UP×2
  }
  if (isDigits(form)) {
    return label + " " + form + "s";  // CANCEL 5s
  }
  return uppercase(key + ":" + form);
}

// Whether the built-in face has no ink for a code, so a label can fall back rather than print a
// blank where a glyph is missing. The classic face's high half is shifted by one, as Adafruit's
// printer shifts it.
bool glyphBlank(uint8_t c) {
  if (c < gfx::BuiltinFont.first || c > gfx::BuiltinFont.last) return true;
  const gfx::Glyph& glyph = gfx::BuiltinFont.glyph[c - gfx::BuiltinFont.first];
  const uint8_t* bitmap = gfx::BuiltinFont.bitmap;
  uint16_t offset = glyph.bitmapOffset;
  uint8_t bits = 0;
  uint8_t bit = 0;
  for (uint8_t row = 0; row < glyph.height; row++) {
    for (uint8_t column = 0; column < glyph.width; column++) {
      if ((bit++ & 7) == 0) bits = bitmap[offset++];
      if ((bits & 0x80) != 0) return false;
      bits = static_cast<uint8_t>(bits << 1);
    }
  }
  return true;
}

}  // namespace

std::string GestureStrip::label(const std::string& gesture) {
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

void GestureStrip::show(const char* gesture) {
  label_ = gesture != nullptr && gesture[0] != '\0' ? label(gesture) : "";
  frames_ = label_.empty() ? 0 : Frames;
}

CaptureDisplay::Component GestureStrip::component(gfx::Rect box) {
  return CaptureDisplay::Component{
      box, gfx::callable([this](gfx::Canvas& canvas) { draw(canvas); })};
}

void GestureStrip::draw(gfx::Canvas& canvas) {
  if (frames_ <= 0) return;

  int16_t x = static_cast<int16_t>(
      (canvas.width() - CellWidth * static_cast<int16_t>(label_.size())) / 2);
  const int16_t y = static_cast<int16_t>((canvas.height() - CellHeight) / 2);
  for (char c : label_) {
    uint8_t code = static_cast<uint8_t>(c);
    if (code >= 176 && code < 255) code++;
    if (glyphBlank(code)) code = 'x';
    gfx::shapes::drawGlyph(canvas, x, y, code, 1, gfx::Ink);
    x = static_cast<int16_t>(x + CellWidth);
  }
  frames_--;
}

}  // namespace gardener
