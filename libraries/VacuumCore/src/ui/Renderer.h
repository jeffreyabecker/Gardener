#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "core/Snapshot.h"

namespace eggplan {

class Display;

enum class Page : uint8_t { Status = 0, Programs, Setpoint, Graph, Info, Count };

struct ViewState {
  Page page = Page::Status;
  size_t cursor = 0;
};

// Drawing helpers and page dispatch. Display is only forward declared, so no Adafruit
// type reaches a UI header and the pages never touch the driver's API directly.
class Renderer {
 public:
  explicit Renderer(Display& display) : display_(display) {}

  bool begin();
  void render(const ViewState& view, const SystemSnapshot& snapshot,
              const UiSettings& settings);

  // Pressure history for the graph page.
  void pushSample(float kpa);
  void sampleView(const float*& samples, uint8_t& count, uint8_t& head) const;

  int16_t width() const;
  int16_t height() const;

  // Helpers for ui/pages/*
  void header(const char* title, bool bleConnected);
  void line(uint8_t index, const char* text);
  void centered(const char* text, int16_t y, uint8_t size);
  void bar(int16_t x, int16_t y, int16_t w, int16_t h, float fraction);
  void frame(int16_t x, int16_t y, int16_t w, int16_t h);
  void plot(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
  void footer(const char* text);
  static constexpr uint8_t historySize() { return kHistorySize; }

 private:
  void renderOverlay(const ScriptOverlay& overlay, const SystemSnapshot& snapshot);
  void renderToast(const UiSettings& settings);

  static constexpr uint8_t kHistorySize = 128;

  Display& display_;
  float history_[kHistorySize] = {0};
  uint8_t historyCount_ = 0;
  uint8_t historyIndex_ = 0;
};

}  // namespace eggplan
