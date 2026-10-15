#include "ui/Renderer.h"

#include <string.h>

#include "core/Config.h"
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
  display_.text(0, 12, "fw ");
  display_.text(30, 12, cfg::FIRMWARE_VERSION);
  display_.flush();
  return true;
}

int16_t Renderer::width() const { return display_.width(); }

int16_t Renderer::height() const { return display_.height(); }

void Renderer::render(const SystemSnapshot& snapshot) {
  if (!display_.present()) return;
  display_.clear();
  pages::renderStatus(*this, snapshot);
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

void Renderer::footer(const char* text) { display_.text(0, 56, text); }

}  // namespace eggplan
