#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "core/Snapshot.h"

namespace eggplan {

class Display;

// Drawing helpers and page dispatch. One page, because the device has no local controls
// and the display is a readout; Display is only forward declared, so no Adafruit type
// reaches a UI header.
//
// These helpers are the whole vocabulary a page needs, and they measure text themselves,
// so a page never has to know the font metrics.
class Renderer {
 public:
  static constexpr int16_t kBatteryIconWidth = 12;

  explicit Renderer(Display& display) : display_(display) {}

  bool begin();
  void render(const SystemSnapshot& snapshot);

  int16_t width() const;
  int16_t textWidth(const char* text, uint8_t size = 1) const;

  // Helpers for ui/pages/*
  void text(int16_t x, int16_t y, const char* text, uint8_t size = 1, bool inverted = false);
  void textRight(const char* text, int16_t y, uint8_t size = 1);
  void centered(const char* text, int16_t y, uint8_t size = 1, bool inverted = false);
  void battery(int16_t x, int16_t y, uint8_t percent);
  void banner(const char* text, int16_t y);
  void bar(int16_t x, int16_t y, int16_t w, int16_t h, float fraction);

 private:
  Display& display_;
};

}  // namespace eggplan
