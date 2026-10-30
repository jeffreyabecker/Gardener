#include "panel/page/BoolScene.h"

#include "Text.h"
#include "layout/Drawing.h"
#include "layout/Layout.h"

namespace gardener::panel::page {
namespace {

// The keys the document keeps for the two states.
constexpr const char* OnKey = "true";
constexpr const char* OffKey = "false";

// The face a state prints at.
constexpr uint8_t ChipScale = 2;

// One state of the switch: its label centred in an equal share of the row, and that label inverted
// when it is the state showing. The alignment places the label's own ink inside the share, so the
// inverted box is the label rather than the whole cell.
gfx::Child chip(const char* title, bool showing, float share) {
  const gfx::Size ink = gfx::shapes::measureText(nullptr, title, ChipScale);
  gfx::PaintNodeRef content =
      gfx::borrowedText(gfx::Box::whole(), title, nullptr, ChipScale);
  if (showing) content = gfx::inverted(std::move(content));
  return gfx::Child{gfx::Cell(share, 1.0f), gfx::aligned(gfx::sized(std::move(content), ink))};
}

// The states as one row of equal cells: `--`, OFF and ON when the setting is nullable and OFF and
// ON when it is not, the state the value is on inverted.
gfx::PaintNodeRef switchRow(bool nullable, bool hasValue, bool on) {
  const float share = nullable ? 1.0f / 3.0f : 0.5f;
  if (nullable) {
    return gfx::row(1.0f, gfx::children(chip("--", !hasValue, share),
                                        chip("OFF", hasValue && !on, share),
                                        chip("ON", hasValue && on, share)));
  }
  return gfx::row(1.0f, gfx::children(chip("OFF", !on, share), chip("ON", on, share)));
}

}  // namespace

BoolScene::BoolScene(GestureQueue& gestures, Ux& ux, String& label, const String& current,
                     bool nullable)
    : ChoiceScene(gestures, ux, label, current,
                  {{OffKey, "OFF"}, {OnKey, "ON"}}, nullable),
      nullable_(nullable) {}

bool BoolScene::on() const { return value_ == OnKey; }

std::any BoolScene::value() const { return hasValue_ ? std::any(on()) : std::any(); }

gfx::PaintNodeRef BoolScene::pageContents() { return switchRow(nullable_, hasValue_, on()); }

}  // namespace gardener::panel::page
