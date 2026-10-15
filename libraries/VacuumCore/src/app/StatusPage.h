#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <stdint.h>

#include "core/Snapshot.h"

namespace eggplan {

// The one page: the device has no local controls, so the display is a readout and every
// control is a command on the serial line.
//
// It draws straight into Adafruit_GFX, the library the panel already speaks, so there is no
// display layer of our own to keep in step with it. App owns the panel and is the only thing
// that knows which one it is: it brings the panel up and flushes it, and this class just
// draws the machine through the base class.
//
// The page reads only the snapshot, so it can never reach into a module. Row positions assume
// the classic 5x7 font: 8 px of pitch at size 1, 16 px at size 2.
class StatusPage {
 public:
  explicit StatusPage(Adafruit_GFX& gfx) : gfx_(gfx) {}

  // Boot banner: drawn once, after the panel has answered.
  void splash();
  void render(const SystemSnapshot& snapshot);

 private:
  // Monochrome pixels: 1 lights one, 0 clears one.
  static constexpr uint16_t kInk = 1;
  static constexpr uint16_t kPaper = 0;
  static constexpr int16_t kBatteryIconWidth = 12;

  // Adafruit_GFX draws text at a cursor and measures it (getTextBounds): what it has no
  // opinion about is alignment, so that is all these helpers add. Nothing here reimplements
  // font metrics.
  int16_t widthOf(const String& value, uint8_t size = 1);
  void text(int16_t x, int16_t y, const String& value, uint8_t size = 1, bool invert = false);
  void textRight(const String& value, int16_t y, uint8_t size = 1);
  void centered(const String& value, int16_t y, uint8_t size = 1, bool invert = false);
  void battery(int16_t x, int16_t y, uint8_t percent);
  void banner(const String& value, int16_t y);
  void bar(int16_t x, int16_t y, int16_t w, int16_t h, float fraction);

  void drawBattery(const SystemSnapshot& snapshot);
  void drawPressure(const SystemSnapshot& snapshot);
  void drawTargetRow(const SystemSnapshot& snapshot);
  void drawCountdown(const SystemSnapshot& snapshot);

  Adafruit_GFX& gfx_;
};

}  // namespace eggplan
