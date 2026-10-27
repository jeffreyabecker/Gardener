#pragma once

#include <stdint.h>

#include <Arduino.h>

#include "ui/ChoiceScene.h"

namespace gardener {

class BuzzerPlayer;
class JinglesConfig;

// The jingle setting's editor: the choice scene over the library's own entries, with what is showing
// auditioned as the page opens and on every step, so a jingle is chosen by ear rather than by
// identifier. Cancel sounds nothing on the way out — what was playing is the value being refused —
// and what the page hands back is the key the setting stores.
//
// The entries are the library's, so a jingle added to the document is a choice with no code here,
// and a stored key the library no longer holds shows the first entry, as any choice does.
class JingleScene : public gfx::ui::ChoiceScene {
 public:
  JingleScene(gfx::ui::GestureQueue& gestures, gfx::ui::Ux& ux, BuzzerPlayer& buzzer,
              const JinglesConfig& jingles, String& label, const String& current);

  // The page went on top: sound the entry showing.
  void activate() override;

 protected:
  void nudge(int8_t direction) override;
  void canceled() override;

 private:
  void play();

  BuzzerPlayer& buzzer_;
  const JinglesConfig& jingles_;
};

}  // namespace gardener
