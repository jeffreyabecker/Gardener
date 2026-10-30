#include "panel/page/BoolScene.h"

#include "Text.h"
#include "Drawing.h"
#include "Layout.h"

namespace gardener::panel::page {
namespace {

// The keys the document keeps for the two states.
constexpr const char* OnKey = "true";
constexpr const char* OffKey = "false";

// The face a state prints at.
constexpr uint8_t ChipScale = 2;

gfx::Justification centred() {
  gfx::Justification justification;
  justification.along = gfx::Justify::Center;
  justification.across = gfx::Justify::Center;
  return justification;
}

// One state of the switch: its label centred in an equal share of the row, and that label inverted
// when it is the state showing. The inner column is what centres the label's own ink inside the
// share, so the inverted box is the label rather than the whole cell.
gfx::Child chip(const char* title, bool showing, float share) {
  const gfx::shapes::TextBox ink = gfx::shapes::measureText(nullptr, title, ChipScale);
  const gfx::Paint label = [title, showing, ink](gfx::Canvas& own) {
    const gfx::Paint content =
        gfx::text(gfx::Rect(0, 0, ink.width, ink.height), title, nullptr, ChipScale);
    own.draw(showing ? gfx::inverted(content) : content);
  };
  const gfx::Child centredLabel{gfx::Cell(static_cast<float>(ink.width),
                                          static_cast<float>(ink.height)),
                                label};
  return gfx::Child{gfx::Cell(share, 1.0f), gfx::column(1.0f, {centredLabel}, centred())};
}

// The states as one row of equal cells: `--`, OFF and ON when the setting is nullable and OFF and
// ON when it is not, the state the value is on inverted.
gfx::Paint switchRow(bool nullable, bool hasValue, bool on) {
  const float share = nullable ? 1.0f / 3.0f : 0.5f;
  if (nullable) {
    return gfx::row(1.0f, {chip("--", !hasValue, share), chip("OFF", hasValue && !on, share),
                           chip("ON", hasValue && on, share)});
  }
  return gfx::row(1.0f, {chip("OFF", !on, share), chip("ON", on, share)});
}

}  // namespace

BoolScene::BoolScene(GestureQueue& gestures, Ux& ux, String& label, const String& current,
                     bool nullable)
    : ChoiceScene(gestures, ux, label, current,
                  {{OffKey, "OFF"}, {OnKey, "ON"}}, nullable),
      nullable_(nullable) {}

bool BoolScene::on() const { return value_ == OnKey; }

std::any BoolScene::value() const { return hasValue_ ? std::any(on()) : std::any(); }

gfx::Paint BoolScene::pageContents() { return switchRow(nullable_, hasValue_, on()); }

}  // namespace gardener::panel::page
