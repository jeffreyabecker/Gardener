#include "GestureStrip.h"

#include <cstdio>
#include <string>

#include "Canvas.h"
#include "Rect.h"
#include "fonts/AdafruitFont.h"
#include "layout/Drawing.h"
#include "shapes/Shapes.h"
#include "shapes/Text.h"

namespace gardener {

namespace {

// The built-in face's cell: five columns of ink and the blank column its advance leaves, eight rows
// tall.
constexpr int16_t CellWidth = 6;
constexpr int16_t CellHeight = 8;

// The box an arrow fills in its button: centred across, and as tall as the glyph the letters beside
// it are drawn at, so the six buttons read as one row.
constexpr int16_t ArrowWidth = 9;
constexpr int16_t ArrowHeight = 8;

// The panel's six buttons in the order the row reads them: the four the arrows fill, then the two
// that settle a page. A key draws either an arrow in the button's own box or the letters beside it.
struct Key {
  const char* name;
  const char* text;
  gfx::Triangle (*arrow)(gfx::Rect, uint8_t, bool);
};

constexpr Key Keys[] = {{"up", nullptr, &gfx::Triangle::up},
                        {"down", nullptr, &gfx::Triangle::down},
                        {"forward", nullptr, &gfx::Triangle::right},
                        {"back", nullptr, &gfx::Triangle::left},
                        {"ok", "OK", nullptr},
                        {"cancel", "X", nullptr}};

int16_t textWidth(const char* text) {
  int16_t width = 0;
  for (const char* c = text; *c != '\0'; c++)
    width = static_cast<int16_t>(width + CellWidth);
  return width;
}

// Whether the built-in face has no ink for a code, so a label can fall back rather than print a
// blank where a glyph is missing. A column of the cell is a byte of the face's bitmap, so a glyph
// with no bit set in any of its columns has nothing to draw.
bool glyphBlank(uint8_t c) {
  const uint8_t* glyph = gfx::AdafruitFont.bitmap;
  const uint16_t start = static_cast<uint16_t>(c * gfx::AdafruitFont.width);
  for (uint8_t across = 0; across < gfx::AdafruitFont.width; across++) {
    if (glyph[start + across] != 0) return false;
  }
  return true;
}

// A label at the built-in face's own cell pitch. The strip is the six buttons as that face draws
// them — the arrows beside the letters are the same height as its line — so it names the face rather
// than following the session's default family, which a walk sets to the panel's design.
void drawText(gfx::Canvas& canvas, int16_t x, int16_t y, const char* text) {
  for (const char* c = text; *c != '\0'; c++) {
    const uint8_t code = static_cast<uint8_t>(*c);
    const char value[] = {static_cast<char>(glyphBlank(code) ? 'x' : code), '\0'};
    gfx::text(gfx::Box(x, y, 0, gfx::lineHeight(gfx::AdafruitFont), true, false), value,
              gfx::findFamily("adafruit"), gfx::Justify::Start, gfx::Justify::Start)
        ->draw(canvas);
    x = static_cast<int16_t>(x + CellWidth);
  }
}

// Whether a form is a count of seconds rather than a name: `cancel:5` is a five-second hold.
bool isCount(const std::string& form) {
  if (form.empty()) return false;
  return form.find_first_not_of("0123456789") == std::string::npos;
}

}  // namespace

void GestureStrip::show(const char* gesture) {
  for (Mark& mark : marks_)
    mark = Mark();
  frames_ = 0;
  if (gesture == nullptr || gesture[0] == '\0') return;

  // A token is one key:form or several joined with '+', which is how the panel names a chord.
  const std::string token(gesture);
  size_t start = 0;
  while (start < token.size()) {
    const size_t plus = token.find('+', start);
    const std::string part =
        token.substr(start, plus == std::string::npos ? std::string::npos : plus - start);
    const size_t colon = part.find(':');
    mark(part.substr(0, colon),
         colon == std::string::npos ? std::string() : part.substr(colon + 1));
    if (plus == std::string::npos) break;
    start = plus + 1;
  }
  frames_ = Frames;
}

void GestureStrip::mark(const std::string& key, const std::string& form) {
  for (int16_t button = 0; button < CapturePicture::Buttons; button++) {
    if (key != Keys[button].name) continue;
    Mark& state = marks_[button];
    if (form == "double") {
      state.outlines = 2;
    } else if (isCount(form)) {
      // Two digits of seconds is as much as the columns beside a two-glyph label hold, and a hold
      // longer than that is not what a walkthrough has to show.
      const int seconds = std::stoi(form);
      state.outlines = 1;
      state.seconds = static_cast<int8_t>(seconds > 99 ? 99 : seconds);
    } else {
      state.outlines = 1;
    }
    return;
  }
}

CaptureDisplay::Component GestureStrip::component(gfx::Rect box) {
  return CaptureDisplay::Component{box,
                                   gfx::callable([this](gfx::Canvas& canvas) { draw(canvas); })};
}

void GestureStrip::draw(gfx::Canvas& canvas) {
  // The buttons are part of the picture whether or not anything is pressed; only the marks a press
  // leaves are what the frames run out.
  const Mark noMark;
  for (int16_t button = 0; button < CapturePicture::Buttons; button++) {
    const Mark& mark = frames_ > 0 ? marks_[button] : noMark;
    const int16_t left = static_cast<int16_t>(button * CapturePicture::ButtonWidth);
    const Key& key = Keys[button];
    // A hold's number goes under the glyph, so what the button shows sits a row higher when there
    // is one and is centred in the button when there is not.
    const bool held = mark.seconds >= 0;
    const int16_t top =
        held ? 1 : static_cast<int16_t>((CapturePicture::ButtonHeight - CellHeight) / 2);
    if (key.arrow != nullptr) {
      const int16_t arrowLeft =
          static_cast<int16_t>(left + (CapturePicture::ButtonWidth - ArrowWidth) / 2);
      canvas.draw(key.arrow(gfx::Rect(arrowLeft, top, ArrowWidth, ArrowHeight), gfx::Ink, true));
    } else {
      drawText(canvas,
               static_cast<int16_t>(left + (CapturePicture::ButtonWidth - textWidth(key.text)) / 2),
               top, key.text);
    }

    if (held) {
      char seconds[4];
      snprintf(seconds, sizeof(seconds), "%d", static_cast<int>(mark.seconds));
      drawText(canvas,
               static_cast<int16_t>(left + CapturePicture::ButtonWidth - 1 - textWidth(seconds)), 9,
               seconds);
    }

    if (mark.outlines > 0) {
      canvas.draw(gfx::Rectangle(left, 0, CapturePicture::ButtonWidth, CapturePicture::ButtonHeight,
                                 gfx::Ink));
    }
    if (mark.outlines > 1) {
      canvas.draw(gfx::Rectangle(static_cast<int16_t>(left + 2), 2,
                                 static_cast<int16_t>(CapturePicture::ButtonWidth - 4),
                                 static_cast<int16_t>(CapturePicture::ButtonHeight - 4), gfx::Ink));
    }
  }

  if (frames_ > 0) frames_--;
}

}  // namespace gardener
