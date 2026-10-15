#pragma once

#include <Arduino.h>

#include "core/Snapshot.h"

namespace eggplan {

class Display;

// The one page. Drawing helpers and layout live in the same class because there is exactly
// one screen: with no local controls the display is a readout, and every control is a
// command on the serial interface.
//
// The page reads only the snapshot, so it can never reach into a module. Row positions
// assume the classic 5x7 font: 8 px of pitch at size 1, 16 px at size 2.
class StatusPage {
 public:
  explicit StatusPage(Display& display) : display_(display) {}

  bool begin();
  void render(const SystemSnapshot& snapshot);

 private:
  static constexpr int16_t kBatteryIconWidth = 12;

  int16_t width() const;
  int16_t textWidth(const char* text, uint8_t size = 1) const;

  void text(int16_t x, int16_t y, const char* text, uint8_t size = 1, bool inverted = false);
  void textRight(const char* text, int16_t y, uint8_t size = 1);
  void centered(const char* text, int16_t y, uint8_t size = 1, bool inverted = false);
  void battery(int16_t x, int16_t y, uint8_t percent);
  void banner(const char* text, int16_t y);
  void bar(int16_t x, int16_t y, int16_t w, int16_t h, float fraction);

  void drawBattery(const SystemSnapshot& snapshot, char* buffer, size_t size);
  void drawPressure(const SystemSnapshot& snapshot, char* buffer, size_t size);
  void drawTargetRow(const SystemSnapshot& snapshot, char* buffer, size_t size);
  void drawCountdown(const SystemSnapshot& snapshot, char* buffer, size_t size);

  Display& display_;
};

}  // namespace eggplan
