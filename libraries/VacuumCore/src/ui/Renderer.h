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
class Renderer {
 public:
  explicit Renderer(Display& display) : display_(display) {}

  bool begin();
  void render(const SystemSnapshot& snapshot);

  int16_t width() const;
  int16_t height() const;

  // Helpers for ui/pages/*
  void header(const char* title, bool bleConnected);
  void line(uint8_t index, const char* text);
  void centered(const char* text, int16_t y, uint8_t size);
  void bar(int16_t x, int16_t y, int16_t w, int16_t h, float fraction);
  void footer(const char* text);

 private:
  Display& display_;
};

}  // namespace eggplan
