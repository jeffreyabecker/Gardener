#include "GestureStrip.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Canvas.h"
#include "CellText.h"
#include "Rect.h"
#include "layout/Drawing.h"
#include "shapes/Shapes.h"

namespace gardener {

namespace {

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

// Whether a form is a count of seconds rather than a name: `cancel:5` is a five-second hold.
bool isCount(const std::string& form) {
  if (form.empty()) return false;
  return form.find_first_not_of("0123456789") == std::string::npos;
}

// The button a name stands for, or -1 when the panel has none.
int16_t buttonIndex(const char* name) {
  if (name == nullptr) return -1;
  for (int16_t button = 0; button < CapturePicture::Buttons; button++) {
    if (strcmp(name, Keys[button].name) == 0) return button;
  }
  return -1;
}

// The keys a gesture names: one, or several joined with '+', which is how the panel spells a chord.
// A '+' with nothing after it names an empty key, which no button answers to.
std::vector<std::string> gestureKeys(const std::string& gesture) {
  std::vector<std::string> keys;
  size_t start = 0;
  for (;;) {
    const size_t plus = gesture.find('+', start);
    keys.push_back(
        gesture.substr(start, plus == std::string::npos ? std::string::npos : plus - start));
    if (plus == std::string::npos) break;
    start = plus + 1;
  }
  return keys;
}

}  // namespace

void GestureStrip::show(const char* gesture) {
  if (gesture == nullptr) return;
  for (const std::string& key : gestureKeys(gesture)) {
    const size_t colon = key.find(':');
    const int16_t button = buttonIndex(key.substr(0, colon).c_str());
    if (button < 0) continue;

    const std::string form = colon == std::string::npos ? std::string() : key.substr(colon + 1);
    Mark& mark = marks_[button];
    mark = Mark();
    if (form == "double") {
      mark.outlines = 2;
    } else if (isCount(form)) {
      // Two digits of seconds is as much as the columns beside a two-glyph label hold, and a hold
      // longer than that is not what a walkthrough has to show.
      const int seconds = std::stoi(form);
      mark.outlines = 1;
      mark.seconds = static_cast<int8_t>(seconds > 99 ? 99 : seconds);
    } else {
      mark.outlines = 1;
    }
  }
}

void GestureStrip::clear(const char* button) {
  const int16_t index = buttonIndex(button);
  if (index >= 0) marks_[index] = Mark();
}

void GestureStrip::clearAll() {
  for (Mark& mark : marks_)
    mark = Mark();
}

CaptureDisplay::Component GestureStrip::component(gfx::Rect box) {
  return CaptureDisplay::Component{box,
                                   gfx::callable([this](gfx::Canvas& canvas) { draw(canvas); })};
}

void GestureStrip::draw(gfx::Canvas& canvas) {
  // The buttons are part of the picture whether or not they are marked; a mark stands until a clear
  // takes it away, so nothing here runs down.
  for (int16_t button = 0; button < CapturePicture::Buttons; button++) {
    const Mark& mark = marks_[button];
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
      drawCellText(
          canvas,
          static_cast<int16_t>(left + (CapturePicture::ButtonWidth - cellTextWidth(key.text)) / 2),
          top, key.text);
    }

    if (held) {
      char seconds[4];
      snprintf(seconds, sizeof(seconds), "%d", static_cast<int>(mark.seconds));
      drawCellText(
          canvas,
          static_cast<int16_t>(left + CapturePicture::ButtonWidth - 1 - cellTextWidth(seconds)), 9,
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
}

bool isButton(const char* name) { return buttonIndex(name) >= 0; }

bool isGesture(const char* gesture) {
  if (gesture == nullptr || gesture[0] == '\0') return false;
  for (const std::string& key : gestureKeys(gesture)) {
    if (buttonIndex(key.substr(0, key.find(':')).c_str()) < 0) return false;
  }
  return true;
}

}  // namespace gardener
