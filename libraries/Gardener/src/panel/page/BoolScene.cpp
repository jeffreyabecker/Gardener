#include "panel/page/BoolScene.h"

#include <utility>

#include "layout/Drawing.h"
#include "layout/Layout.h"

namespace gardener {
namespace {

// The key the document keeps for the state that is on. A setting that holds nothing and one that
// holds "false" are the same OFF, so the key is only what the switch opens on.
constexpr const char* OnKey = "true";

// One state of the switch: its label centred in an equal share of the row, and the whole share
// inverted when it is the state showing.
gfx::Child chip(const char* title, bool showing, float share) {
  gfx::PaintNodeRef content = gfx::text(title);
  if (showing) content = gfx::inverted(std::move(content));
  return gfx::column(share, std::move(content));
}

// The states as one row of equal cells: `--`, OFF and ON when the setting is nullable and OFF and
// ON when it is not, the state the value is on inverted.
gfx::PaintNodeRef switchRow(bool nullable, bool hasState, bool on) {
  const float share = nullable ? 1.0f / 3.0f : 0.5f;
  if (nullable) {
    return gfx::row(1.0f, chip("--", !hasState, share), chip("OFF", hasState && !on, share),
                    chip("ON", hasState && on, share));
  }
  return gfx::row(1.0f, chip("OFF", !on, share), chip("ON", on, share));
}

}  // namespace

BoolScene::BoolScene(GestureQueue& gestures, Ux& ux, String& label, const String& current,
                     bool nullable)
    : EditorScene(gestures, ux, label), nullable_(nullable) {
  hasState_ = !nullable_ || !current.isEmpty();
  on_ = current == OnKey;
  openedOn_ = on_;
  openedHasState_ = hasState_;
}

String BoolScene::text() const {
  if (!hasState_) return "--";
  return on_ ? "ON" : "OFF";
}

std::any BoolScene::value() const { return hasState_ ? std::any(on_) : std::any(); }

// Two states in a ring, with a nullable setting's nothing below OFF: up out of nothing is OFF,
// down out of OFF is nothing again, and any other step turns the switch over.
void BoolScene::nudge(int8_t direction) {
  if (nullable_) {
    if (!hasState_) {
      if (direction > 0) hasState_ = true;
      return;
    }
    if (direction < 0 && !on_) {
      hasState_ = false;
      return;
    }
  }
  on_ = !on_;
}

void BoolScene::canceled() {
  on_ = openedOn_;
  hasState_ = openedHasState_;
}

gfx::PaintNodeRef BoolScene::pageContents(gfx::Size) {
  return switchRow(nullable_, hasState_, on_);
}

}  // namespace gardener
