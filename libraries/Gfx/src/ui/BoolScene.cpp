#include "ui/BoolScene.h"

#include "gfx/Text.h"
#include "ui/Drawing.h"
#include "ui/Layout.h"

namespace gardener::gfx::ui {
namespace {

// The keys the document keeps for the two states.
constexpr const char* OnKey = "true";
constexpr const char* OffKey = "false";

// The face a state prints at.
constexpr uint8_t ChipScale = 2;

Alignment centred() {
  Alignment alignment;
  alignment.horizontal = Horizontal::Center;
  alignment.vertical = Vertical::Center;
  return alignment;
}

// One state of the switch: its label centred in an equal share of the row, and that label inverted
// when it is the state showing. The inner column is what centres the label's own ink inside the
// share, so the inverted box is the label rather than the whole cell.
Child chip(const char* title, bool showing, float share) {
  const shapes::TextBox ink = shapes::measureText(nullptr, title, ChipScale);
  const Paint label = [title, showing, ink](Canvas& own) {
    text(Rect(0, 0, ink.width, ink.height), title, nullptr, ChipScale)(own);
    if (showing) own.invert(Rect(0, 0, ink.width, ink.height));
  };
  const Child centredLabel{Cell(static_cast<float>(ink.width), static_cast<float>(ink.height)),
                           label};
  return Child{Cell(share, 1.0f), column(1.0f, {centredLabel}, centred())};
}

// The states as one row of equal cells: `--`, OFF and ON when the setting is nullable and OFF and
// ON when it is not, the state the value is on inverted.
Paint switchRow(bool nullable, bool hasValue, bool on) {
  const float share = nullable ? 1.0f / 3.0f : 0.5f;
  if (nullable) {
    return row(1.0f, {chip("--", !hasValue, share), chip("OFF", hasValue && !on, share),
                      chip("ON", hasValue && on, share)});
  }
  return row(1.0f, {chip("OFF", !on, share), chip("ON", on, share)});
}

}  // namespace

BoolScene::BoolScene(GestureQueue& gestures, Ux& ux, String& label, const String& current,
                     bool nullable)
    : ChoiceScene(gestures, ux, label, current,
                  {{OffKey, "OFF", nullptr}, {OnKey, "ON", nullptr}}, nullable),
      nullable_(nullable) {}

bool BoolScene::on() const { return value_ == OnKey; }

std::any BoolScene::value() const { return hasValue_ ? std::any(on()) : std::any(); }

Paint BoolScene::pageContents() { return switchRow(nullable_, hasValue_, on()); }

}  // namespace gardener::gfx::ui
