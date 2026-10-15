#include "ui/Renderer.h"

#include <string.h>

#include "core/Config.h"
#include "hal/Display.h"
#include "ui/pages/Pages.h"

namespace eggplan {
namespace {
constexpr int16_t kGlyphWidth = 6;   // advance of the classic 5x7 font at size 1
constexpr int16_t kBatteryBodyWidth = 10;
constexpr int16_t kBatteryBodyHeight = 7;
constexpr int16_t kBannerHeight = 9;
}  // namespace

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

void Renderer::render(const SystemSnapshot& snapshot) {
  if (!display_.present()) return;
  display_.clear();
  pages::renderStatus(*this, snapshot);
  display_.flush();
}

int16_t Renderer::textWidth(const char* text, uint8_t size) const {
  return (int16_t)(text ? strlen(text) : 0) * kGlyphWidth * (int16_t)size;
}

void Renderer::text(int16_t x, int16_t y, const char* text, uint8_t size, bool inverted) {
  if (inverted) {
    display_.textInverted(x, y, text, size);
  } else {
    display_.text(x, y, text, size);
  }
}

void Renderer::textRight(const char* value, int16_t y, uint8_t size) {
  int16_t x = display_.width() - textWidth(value, size);
  text(x < 0 ? 0 : x, y, value, size);
}

void Renderer::centered(const char* text, int16_t y, uint8_t size, bool inverted) {
  int16_t x = (display_.width() - textWidth(text, size)) / 2;
  this->text(x < 0 ? 0 : x, y, text, size, inverted);
}

void Renderer::battery(int16_t x, int16_t y, uint8_t percent) {
  display_.rect(x, y, kBatteryBodyWidth, kBatteryBodyHeight, false);
  display_.rect(x + kBatteryBodyWidth, y + 2, 2, 3, true);  // nub
  if (percent == 0) return;
  int16_t interior = kBatteryBodyWidth - 4;  // 1 px frame plus 1 px of gap
  int16_t fill = (int16_t)((int32_t)interior * percent / 100);
  if (fill < 1) fill = 1;  // any charge at all shows something
  if (fill > interior) fill = interior;
  display_.rect(x + 2, y + 2, fill, kBatteryBodyHeight - 4, true);
}

void Renderer::banner(const char* text, int16_t y) {
  display_.rect(0, y, display_.width(), kBannerHeight, true);
  display_.textInverted(0, y + 1, text, 1);
}

void Renderer::bar(int16_t x, int16_t y, int16_t w, int16_t h, float fraction) {
  if (fraction < 0.0f) fraction = 0.0f;
  if (fraction > 1.0f) fraction = 1.0f;
  display_.rect(x, y, w, h, false);
  int16_t fill = (int16_t)((float)(w - 2) * fraction);
  if (fill > 0) display_.rect(x + 1, y + 1, fill, h - 2, true);
}

}  // namespace eggplan
