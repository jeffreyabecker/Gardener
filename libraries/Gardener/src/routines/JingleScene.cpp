#include "Build.h"

#if GARDENER_PANEL

#include "routines/JingleScene.h"

#include <any>
#include <vector>

#include "BuzzerPlayer.h"
#include "storage/Jingles.h"

namespace gardener {
namespace {

// The library's entries as the choices they are: the key the setting stores and the short title the
// row prints. The library is the whole of what a page knows about jingles, so a lookup scans it —
// a handful of entries, and the scan needs no index kept in step with the document.
std::vector<gfx::ui::ChoiceItem> choicesOf(const JinglesConfig& jingles) {
  std::vector<gfx::ui::ChoiceItem> items;
  for (const Jingle& jingle : jingles.jingles()) {
    items.push_back(gfx::ui::ChoiceItem{jingle.key, jingle.short_title, nullptr});
  }
  return items;
}

}  // namespace

JingleScene::JingleScene(gfx::ui::GestureQueue& gestures, gfx::ui::Ux& ux, BuzzerPlayer& buzzer,
                         const JinglesConfig& jingles, String& label, const String& current)
    : ChoiceScene(gestures, ux, label, current, choicesOf(jingles), false),
      buzzer_(buzzer),
      jingles_(jingles) {}

void JingleScene::activate() { play(); }

// A step is a new jingle, and sounding it is the step's point.
void JingleScene::nudge(int8_t direction) {
  ChoiceScene::nudge(direction);
  play();
}

// What was auditioning is the value being refused, so it goes quiet; the base then puts back the
// key the page opened on.
void JingleScene::canceled() {
  buzzer_.off();
  ChoiceScene::canceled();
}

void JingleScene::play() {
  if (isNull()) return;
  const String key = std::any_cast<String>(value());
  for (const Jingle& jingle : jingles_.jingles()) {
    if (jingle.key == key) {
      buzzer_.play(jingle.notes.c_str());
      return;
    }
  }
}

}  // namespace gardener
#endif
