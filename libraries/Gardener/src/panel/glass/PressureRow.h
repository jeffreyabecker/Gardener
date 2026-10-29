#pragma once

#include <any>
#include <memory>

#include <Arduino.h>

#include "panel/glass/PressureScene.h"
#include "panel/glass/Units.h"
#include "panel/page/SettingsRows.h"

namespace gardener {

// The row of the target pressure: a number row over the depth in the kPa the machine stores, whose
// editor is the pressure scene — a keypress is one step of the unit the glass shows — and which
// prints the depth with that unit's decimals. The bounds are the machine's, so a keypress cannot
// leave the editor on a target the loop would refuse.
class PressureRow : public gfx::ui::NumberRow {
 public:
  PressureRow(gfx::ui::GestureQueue& gestures, gfx::ui::Ux& ux, const String& key,
              const String& label, float currentKpa, DisplayUnit unit, float wholeSteps,
              bool nullable = false, gfx::ui::Validator validator = nullptr);

  gfx::ui::SettingDeclaration declaration() const override;
  std::unique_ptr<gfx::ui::EditorScene> scene(const std::any& value) override;

 private:
  DisplayUnit unit_;
  float wholeSteps_;
};

}  // namespace gardener
