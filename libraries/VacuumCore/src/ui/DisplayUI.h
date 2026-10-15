#pragma once

#include <Adafruit_SH110X.h>
#include <Arduino.h>
#include <Wire.h>

#include "services/ProgramStore.h"
#include "core/VacuumConfig.h"

namespace eggplan {

// Local 128x64 OLED user interface.
//
// Rendering is driven by a plain model struct that the application refreshes
// every loop; the UI never reaches into the drivers itself.
class DisplayUI {
 public:
  enum class Page : uint8_t { Status = 0, Programs, Setpoint, Graph, Info, Count };

  struct Model {
    float pressureKpa = 0.0f;
    float vacuumKpa = 0.0f;
    float vacuumPercent = 0.0f;
    float ambientKpa = 0.0f;
    float duty = 0.0f;
    float targetKpa = 0.0f;
    bool targetActive = false;
    const char* state = "IDLE";
    const char* program = "";
    const char* autostart = "";
    bool bleConnected = false;
    bool sensorOk = false;
    bool scriptRunning = false;
    uint32_t uptimeMs = 0;
    const ProgramInfo* programs = nullptr;
    size_t programCount = 0;
  };

  bool begin();
  bool present() const { return present_; }

  void setModel(const Model& model) { model_ = model; }
  const Model& model() const { return model_; }

  void update(uint32_t nowMs);

  void setPage(Page page);
  void nextPage(int direction);
  Page page() const { return page_; }
  const char* pageName() const;

  size_t cursor() const { return cursor_; }
  void setCursor(size_t value) { cursor_ = value; }
  void moveCursor(int delta, size_t count);

  void pushPressure(float kpa);
  void toast(const char* text, uint32_t durationMs);

  // Overlay used by running scripts.
  void overlayClear();
  bool overlayLine(int line, const char* text);
  void overlayBar(float fraction);
  void overlayRefresh() { dirty_ = true; }
  bool overlayActive() const { return overlayActive_; }
  void setOverlayActive(bool active) { overlayActive_ = active; dirty_ = true; }

 private:
  void render();
  void renderStatus();
  void renderPrograms();
  void renderSetpoint();
  void renderGraph();
  void renderInfo();
  void renderOverlay();
  void renderToast(uint32_t nowMs);
  void drawBar(int16_t x, int16_t y, int16_t w, int16_t h, float fraction);
  void drawHeader(const char* title);
  void drawLine(int16_t line, const char* text);
  void drawCentered(const char* text, int16_t y, uint8_t size);

  Adafruit_SH1106G display_{cfg::OLED_WIDTH, cfg::OLED_HEIGHT, &Wire, -1};
  bool present_ = false;
  bool dirty_ = true;
  Page page_ = Page::Status;
  Model model_;
  size_t cursor_ = 0;
  float history_[cfg::OLED_WIDTH] = {0};
  uint8_t historyCount_ = 0;
  uint8_t historyIndex_ = 0;
  uint32_t lastRenderMs_ = 0;
  char toastText_[26] = {0};
  uint32_t toastUntilMs_ = 0;
  bool overlayActive_ = false;
  char overlayLines_[7][24] = {{0}};
  float overlayBarValue_ = 0.0f;
};

}  // namespace eggplan
