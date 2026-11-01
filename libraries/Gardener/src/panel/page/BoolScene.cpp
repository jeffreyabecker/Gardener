#include "panel/page/BoolScene.h"

#include "layout/Drawing.h"
#include "layout/Layout.h"

namespace gardener {
namespace {

// The keys the document keeps for the two states.
constexpr const char* OnKey = "true";
constexpr const char* OffKey = "false";

// One state of the switch: its label centred in an equal share of the row, and the whole share
// inverted when it is the state showing.
gfx::Child chip(const char* title, bool showing, float share) {
  gfx::PaintNodeRef content = gfx::text(gfx::Box::whole(), title, nullptr);
  if (showing) content = gfx::inverted(std::move(content));
  return gfx::column(share, std::move(content));
}

// The states as one row of equal cells: `--`, OFF and ON when the setting is nullable and OFF and
// ON when it is not, the state the value is on inverted.
gfx::PaintNodeRef switchRow(bool nullable, bool hasValue, bool on) {
  const float share = nullable ? 1.0f / 3.0f : 0.5f;
  if (nullable) {
    return gfx::row(1.0f, chip("--", !hasValue, share), chip("OFF", hasValue && !on, share),
                    chip("ON", hasValue && on, share));
  }
  return gfx::row(1.0f, chip("OFF", !on, share), chip("ON", on, share));
}

}  // namespace

BoolScene::BoolScene(GestureQueue& gestures, Ux& ux, String& label, const String& current,
                     bool nullable)
    : ChoiceScene(gestures, ux, label, current, {{OffKey, "OFF"}, {OnKey, "ON"}}, nullable),
      nullable_(nullable) {}

bool BoolScene::on() const { return value_ == OnKey; }

std::any BoolScene::value() const { return hasValue_ ? std::any(on()) : std::any(); }

gfx::PaintNodeRef BoolScene::pageContents(gfx::Size) {
  return switchRow(nullable_, hasValue_, on());
}

}  // namespace gardener