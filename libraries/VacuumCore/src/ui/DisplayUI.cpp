#include "ui/DisplayUI.h"

#include "core/Log.h"
#include "core/Util.h"

namespace eggplan {
namespace {
constexpr uint32_t kRenderPeriodMs = 100;
constexpr int16_t kLineHeight = 8;
constexpr uint8_t kOverlayLines = 7;
}

bool DisplayUI::begin() {
  present_ = display_.begin(cfg::OLED_I2C_ADDR, false);
  if (!present_) {
    LOGE("oled: not found at 0x%02X", cfg::OLED_I2C_ADDR);
    return false;
  }
  display_.setTextColor(SH110X_WHITE);
  display_.clearDisplay();
  display_.setTextSize(1);
  display_.setCursor(0, 0);
  display_.println("Eggplan Vacuum");
  display_.print("fw ");
  display_.println(cfg::FIRMWARE_VERSION);
  display_.display();
  LOGI("oled: SH1106 online");
  return true;
}

const char* DisplayUI::pageName() const {
  switch (page_) {
    case Page::Status: return "status";
    case Page::Programs: return "programs";
    case Page::Setpoint: return "setpoint";
    case Page::Graph: return "graph";
    case Page::Info: return "info";
    default: return "?";
  }
}

void DisplayUI::setPage(Page page) {
  if (page_ == page) return;
  page_ = page;
  dirty_ = true;
}

void DisplayUI::nextPage(int direction) {
  int count = (int)Page::Count;
  int index = (int)page_ + direction;
  while (index < 0) index += count;
  index %= count;
  setPage((Page)index);
}

void DisplayUI::moveCursor(int delta, size_t count) {
  if (count == 0) {
    cursor_ = 0;
    return;
  }
  int value = (int)cursor_ + delta;
  while (value < 0) value += (int)count;
  cursor_ = (size_t)(value % (int)count);
  dirty_ = true;
}

void DisplayUI::pushPressure(float kpa) {
  history_[historyIndex_] = kpa;
  historyIndex_ = (uint8_t)((historyIndex_ + 1) % cfg::OLED_WIDTH);
  if (historyCount_ < cfg::OLED_WIDTH) historyCount_++;
}

void DisplayUI::toast(const char* text, uint32_t durationMs) {
  if (!text) return;
  strncpy(toastText_, text, sizeof(toastText_) - 1);
  toastText_[sizeof(toastText_) - 1] = '\0';
  toastUntilMs_ = millis() + durationMs;
  dirty_ = true;
}

void DisplayUI::overlayClear() {
  for (uint8_t i = 0; i < kOverlayLines; i++) overlayLines_[i][0] = '\0';
  overlayBarValue_ = 0.0f;
  overlayActive_ = true;
  dirty_ = true;
}

bool DisplayUI::overlayLine(int line, const char* text) {
  if (line < 0 || line >= (int)kOverlayLines) return false;
  strncpy(overlayLines_[line], text ? text : "", sizeof(overlayLines_[line]) - 1);
  overlayLines_[line][sizeof(overlayLines_[line]) - 1] = '\0';
  overlayActive_ = true;
  dirty_ = true;
  return true;
}

void DisplayUI::overlayBar(float fraction) {
  overlayBarValue_ = clampf(fraction, 0.0f, 1.0f);
  overlayActive_ = true;
  dirty_ = true;
}

void DisplayUI::update(uint32_t nowMs) {
  if (!present_) return;
  if (!dirty_ && (nowMs - lastRenderMs_) < kRenderPeriodMs && toastUntilMs_ <= nowMs) return;
  lastRenderMs_ = nowMs;
  dirty_ = false;
  render();
  if (toastUntilMs_ > nowMs) {
    renderToast(nowMs);
  }
  display_.display();
}

void DisplayUI::render() {
  if (overlayActive_) {
    renderOverlay();
    return;
  }
  switch (page_) {
    case Page::Status: renderStatus(); break;
    case Page::Programs: renderPrograms(); break;
    case Page::Setpoint: renderSetpoint(); break;
    case Page::Graph: renderGraph(); break;
    case Page::Info: renderInfo(); break;
    default: renderStatus(); break;
  }
}

void DisplayUI::drawHeader(const char* title) {
  display_.setTextSize(1);
  display_.setCursor(0, 0);
  display_.print(title);
  if (model_.bleConnected) {
    display_.setCursor(112, 0);
    display_.print("BLE");
  }
  display_.drawFastHLine(0, 9, cfg::OLED_WIDTH, SH110X_WHITE);
}

void DisplayUI::drawLine(int16_t line, const char* text) {
  display_.setTextSize(1);
  display_.setCursor(0, 10 + line * kLineHeight);
  display_.print(text);
}

void DisplayUI::drawCentered(const char* text, int16_t y, uint8_t size) {
  display_.setTextSize(size);
  int16_t width = (int16_t)strlen(text) * 6 * size;
  int16_t x = (cfg::OLED_WIDTH - width) / 2;
  if (x < 0) x = 0;
  display_.setCursor(x, y);
  display_.print(text);
}

void DisplayUI::drawBar(int16_t x, int16_t y, int16_t w, int16_t h, float fraction) {
  fraction = clampf(fraction, 0.0f, 1.0f);
  display_.drawRect(x, y, w, h, SH110X_WHITE);
  int16_t fill = (int16_t)((float)(w - 2) * fraction);
  if (fill > 0) display_.fillRect(x + 1, y + 1, fill, h - 2, SH110X_WHITE);
}

void DisplayUI::renderStatus() {
  display_.clearDisplay();
  drawHeader("EGGPLAN VACUUM");

  char buf[24];
  if (model_.sensorOk) {
    snprintf(buf, sizeof(buf), "%.1f", model_.pressureKpa);
    drawCentered(buf, 12, 2);
    display_.setTextSize(1);
    display_.setCursor(104, 18);
    display_.print("kPa");
  } else {
    drawCentered("NO SENSOR", 16, 1);
  }

  snprintf(buf, sizeof(buf), "vac %.1f kPa  %.0f%%", model_.vacuumKpa,
           model_.vacuumPercent);
  drawLine(1, buf);

  drawBar(0, 30, cfg::OLED_WIDTH, 7, model_.duty / 100.0f);
  snprintf(buf, sizeof(buf), "duty %3.0f%%", model_.duty);
  display_.setTextSize(1);
  display_.setCursor(2, 31);
  display_.setTextColor(SH110X_BLACK, SH110X_WHITE);
  display_.print(buf);
  display_.setTextColor(SH110X_WHITE);

  if (model_.targetActive) {
    snprintf(buf, sizeof(buf), "%s -> %.1f kPa", model_.state, model_.targetKpa);
  } else {
    snprintf(buf, sizeof(buf), "%s", model_.state);
  }
  drawLine(3, buf);

  if (model_.scriptRunning) {
    snprintf(buf, sizeof(buf), "run %s", model_.program);
  } else if (model_.autostart && model_.autostart[0]) {
    snprintf(buf, sizeof(buf), "auto %s", model_.autostart);
  } else {
    snprintf(buf, sizeof(buf), "no autostart");
  }
  drawLine(4, buf);
}

void DisplayUI::renderPrograms() {
  display_.clearDisplay();
  char buf[28];
  snprintf(buf, sizeof(buf), "PROGRAMS %u", (unsigned)model_.programCount);
  drawHeader(buf);

  if (model_.programCount == 0) {
    drawLine(0, "no programs stored");
    drawLine(2, "upload over BLE:");
    drawLine(3, "  prog characteristic");
    return;
  }

  const uint8_t rows = 5;
  size_t first = 0;
  if (cursor_ >= rows) first = cursor_ - rows + 1;

  for (uint8_t row = 0; row < rows; row++) {
    size_t index = first + row;
    if (index >= model_.programCount) break;
    const ProgramInfo* info = &model_.programs[index];
    snprintf(buf, sizeof(buf), "%c%-12s%5u", index == cursor_ ? '>' : ' ',
             info->name.c_str(), (unsigned)info->size);
    drawLine(row, buf);
  }

  display_.setTextSize(1);
  display_.setCursor(0, 56);
  display_.print("OK=run  BACK=stop");
}

void DisplayUI::renderSetpoint() {
  display_.clearDisplay();
  drawHeader("TARGET");

  char buf[24];
  if (model_.targetActive) {
    snprintf(buf, sizeof(buf), "%.1f", model_.targetKpa);
  } else {
    snprintf(buf, sizeof(buf), "--");
  }
  drawCentered(buf, 14, 2);
  display_.setTextSize(1);
  display_.setCursor(100, 20);
  display_.print("kPa");

  snprintf(buf, sizeof(buf), "now  %.1f kPa", model_.pressureKpa);
  drawLine(2, buf);
  snprintf(buf, sizeof(buf), "trim %.0f%%", model_.duty);
  drawLine(3, buf);
  drawLine(4, "UP/DOWN +-1  OK=hold");
}

void DisplayUI::renderGraph() {
  display_.clearDisplay();
  char buf[28];
  snprintf(buf, sizeof(buf), "PRESSURE %.1f kPa", model_.pressureKpa);
  drawHeader(buf);

  const int16_t top = 12;
  const int16_t height = 42;
  float minValue = 1e9f;
  float maxValue = -1e9f;
  for (uint8_t i = 0; i < historyCount_; i++) {
    if (history_[i] < minValue) minValue = history_[i];
    if (history_[i] > maxValue) maxValue = history_[i];
  }
  if (historyCount_ < 2 || maxValue - minValue < 1.0f) {
    minValue = model_.pressureKpa - 5.0f;
    maxValue = model_.pressureKpa + 5.0f;
  }

  display_.drawRect(0, top, cfg::OLED_WIDTH, height, SH110X_WHITE);
  if (historyCount_ > 1) {
    int16_t prevY = -1;
    for (uint8_t i = 0; i < historyCount_; i++) {
      uint8_t slot = (uint8_t)((historyIndex_ + cfg::OLED_WIDTH - historyCount_ + i) %
                               cfg::OLED_WIDTH);
      float norm = (history_[slot] - minValue) / (maxValue - minValue);
      int16_t y = (int16_t)(top + height - 2 - norm * (height - 4));
      int16_t x = (int16_t)i;
      if (prevY >= 0) display_.drawLine(x - 1, prevY, x, y, SH110X_WHITE);
      prevY = y;
    }
  }

  snprintf(buf, sizeof(buf), "%.1f..%.1f kPa", minValue, maxValue);
  display_.setTextSize(1);
  display_.setCursor(0, 56);
  display_.print(buf);
}

void DisplayUI::renderInfo() {
  display_.clearDisplay();
  drawHeader("INFO");

  char buf[28];
  snprintf(buf, sizeof(buf), "fw %s", cfg::FIRMWARE_VERSION);
  drawLine(0, buf);
  snprintf(buf, sizeof(buf), "chip %s", ESP.getChipModel());
  drawLine(1, buf);
  snprintf(buf, sizeof(buf), "heap %u kB", (unsigned)(ESP.getFreeHeap() / 1024));
  drawLine(2, buf);
  snprintf(buf, sizeof(buf), "up %lu s", (unsigned long)(model_.uptimeMs / 1000));
  drawLine(3, buf);
  snprintf(buf, sizeof(buf), "progs %u", (unsigned)model_.programCount);
  drawLine(4, buf);
  snprintf(buf, sizeof(buf), "i2c 0x%02X 0x%02X", cfg::MPRLS_I2C_ADDR, cfg::OLED_I2C_ADDR);
  display_.setCursor(0, 56);
  display_.print(buf);
}

void DisplayUI::renderOverlay() {
  display_.clearDisplay();
  char buf[28];
  snprintf(buf, sizeof(buf), "SCRIPT %s", model_.program);
  drawHeader(buf);
  for (uint8_t i = 0; i < kOverlayLines; i++) {
    drawLine(i, overlayLines_[i]);
  }
  drawBar(0, 56, cfg::OLED_WIDTH, 7, overlayBarValue_);
}

void DisplayUI::renderToast(uint32_t nowMs) {
  (void)nowMs;
  int16_t width = (int16_t)strlen(toastText_) * 6 + 6;
  if (width > cfg::OLED_WIDTH) width = cfg::OLED_WIDTH;
  int16_t x = (cfg::OLED_WIDTH - width) / 2;
  display_.fillRect(x, 44, width, 12, SH110X_WHITE);
  display_.setTextColor(SH110X_BLACK, SH110X_WHITE);
  display_.setTextSize(1);
  display_.setCursor(x + 3, 46);
  display_.print(toastText_);
  display_.setTextColor(SH110X_WHITE);
}

}  // namespace eggplan
