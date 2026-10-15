#include "ui/Renderer.h"

#include <stdio.h>
#include <string.h>

#include "core/Log.h"
#include "hal/Display.h"
#include "ui/pages/Pages.h"

namespace eggplan {
namespace {
constexpr int16_t kLineHeight = 8;
}

bool Renderer::begin() {
  if (!display_.present()) return false;
  display_.clear();
  display_.text(0, 0, "Eggplan Vacuum");
  display_.text(0, 10, "fw " );
  display_.text(30, 10, cfg::FIRMWARE_VERSION);
  display_.flush();
  return true;
}

int16_t Renderer::width() const { return display_.width(); }

int16_t Renderer::height() const { return display_.height(); }

void Renderer::pushSample(float kpa) {
  history_[historyIndex_] = kpa;
  historyIndex_ = (uint8_t)((historyIndex_ + 1) % kHistorySize);
  if (historyCount_ < kHistorySize) historyCount_++;
}

void Renderer::sampleView(const float*& samples, uint8_t& count, uint8_t& head) const {
  samples = history_;
  count = historyCount_;
  head = historyIndex_;
}

void Renderer::render(const ViewState& view, const SystemSnapshot& snapshot,
                      const UiSettings& settings) {
  if (!display_.present()) return;
  display_.clear();

  const ScriptOverlay* overlay = snapshot.overlay;
  if (overlay && overlay->active) {
    renderOverlay(*overlay, snapshot);
  } else {
    switch (view.page) {
      case Page::Status: pages::renderStatus(*this, snapshot); break;
      case Page::Programs: pages::renderPrograms(*this, snapshot, view.cursor); break;
      case Page::Setpoint: pages::renderSetpoint(*this, snapshot); break;
      case Page::Graph: pages::renderGraph(*this, snapshot); break;
      case Page::Info: pages::renderInfo(*this, snapshot); break;
      default: pages::renderStatus(*this, snapshot); break;
    }
  }

  renderToast(settings);
  display_.flush();
}

void Renderer::header(const char* title, bool bleConnected) {
  display_.text(0, 0, title);
  if (bleConnected) display_.text(112, 0, "BLE");
  display_.hline(0, 9, display_.width());
}

void Renderer::line(uint8_t index, const char* text) {
  display_.text(0, 10 + index * kLineHeight, text);
}

void Renderer::centered(const char* text, int16_t y, uint8_t size) {
  int16_t textWidth = (int16_t)strlen(text) * 6 * size;
  int16_t x = (display_.width() - textWidth) / 2;
  display_.text(x < 0 ? 0 : x, y, text, size);
}

void Renderer::bar(int16_t x, int16_t y, int16_t w, int16_t h, float fraction) {
  if (fraction < 0.0f) fraction = 0.0f;
  if (fraction > 1.0f) fraction = 1.0f;
  display_.rect(x, y, w, h, false);
  int16_t fill = (int16_t)((float)(w - 2) * fraction);
  if (fill > 0) display_.rect(x + 1, y + 1, fill, h - 2, true);
}

void Renderer::frame(int16_t x, int16_t y, int16_t w, int16_t h) {
  display_.rect(x, y, w, h, false);
}

void Renderer::plot(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  display_.line(x0, y0, x1, y1);
}

void Renderer::footer(const char* text) { display_.text(0, 56, text); }

void Renderer::renderOverlay(const ScriptOverlay& overlay, const SystemSnapshot& snapshot) {
  char title[28];
  snprintf(title, sizeof(title), "SCRIPT %s", snapshot.program);
  header(title, snapshot.bleConnected);
  for (uint8_t i = 0; i < ScriptOverlay::kLines; i++) {
    line(i, overlay.lines[i]);
  }
  bar(0, 56, display_.width(), 7, overlay.bar);
}

void Renderer::renderToast(const UiSettings& settings) {
  if (settings.toast[0] == '\0' || settings.toastUntilMs <= millis()) return;
  int16_t boxWidth = (int16_t)strlen(settings.toast) * 6 + 6;
  if (boxWidth > display_.width()) boxWidth = display_.width();
  int16_t x = (display_.width() - boxWidth) / 2;
  display_.rect(x, 44, boxWidth, 12, true);
  display_.textInverted(x + 3, 46, settings.toast);
}

}  // namespace eggplan
