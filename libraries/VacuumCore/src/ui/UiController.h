#pragma once

#include <Arduino.h>
#include <stddef.h>

#include "core/Snapshot.h"
#include "hal/Buttons.h"
#include "ui/Renderer.h"

// Turns button events into intents and owns the local presentation state: which page
// is showing, the program cursor, the graph sampling timer. It never calls a module,
// which is what keeps ui/ out of services/.

namespace eggplan {

enum class UiIntent : uint8_t {
  None = 0,
  Stop,
  CyclePage,
  MoveCursor,
  AdjustTarget,
  ToggleHold,
  RunSelected,
};

struct Intent {
  UiIntent kind = UiIntent::None;
  int delta = 0;
};

class UiController {
 public:
  UiController(Renderer& renderer, Buttons& buttons, UiSettings& settings)
      : renderer_(renderer), buttons_(buttons), settings_(settings) {}

  bool begin();
  void update(uint32_t nowMs, const SystemSnapshot& snapshot);
  Intent onButton(ButtonId id, ButtonEvent event, size_t programCount);

  void cyclePage(int delta);
  void moveCursor(int delta, size_t count);
  void toast(const char* text, uint32_t durationMs);

  Page page() const { return view_.page; }
  const char* pageName() const;
  size_t cursor() const { return view_.cursor; }

 private:
  static const char* pageName(Page page);

  Renderer& renderer_;
  Buttons& buttons_;
  UiSettings& settings_;
  ViewState view_;
  uint32_t lastSampleMs_ = 0;
};

}  // namespace eggplan
