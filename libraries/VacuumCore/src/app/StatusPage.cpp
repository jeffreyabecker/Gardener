#include "app/StatusPage.h"

#include <string.h>

#include "core/Config.h"
#include "core/Text.h"
#include "hal/Display.h"

namespace eggplan {
namespace {

constexpr int16_t kGlyphWidth = 6;  // advance of the classic 5x7 font at size 1
constexpr int16_t kBatteryBodyWidth = 10;
constexpr int16_t kBatteryBodyHeight = 7;
constexpr int16_t kBannerHeight = 9;

constexpr int16_t kStateY = 0;
constexpr int16_t kPressureY = 9;
constexpr int16_t kTargetY = 26;
constexpr int16_t kCountdownY = 36;
constexpr int16_t kBarY = 56;
constexpr int16_t kBarHeight = 8;

// Below the low threshold the battery fill blinks. With no buttons and no alarm, that
// blink is the entire low battery notification (Q11).
constexpr uint32_t kLowBatteryBlinkMs = 500;

}  // namespace

bool StatusPage::begin() {
  if (!display_.present()) return false;
  display_.clear();
  display_.text(0, 0, "Eggplan Vacuum");
  display_.text(0, 12, "fw ");
  display_.text(30, 12, cfg::FIRMWARE_VERSION);
  display_.flush();
  return true;
}

void StatusPage::render(const SystemSnapshot& snapshot) {
  if (!display_.present()) return;
  display_.clear();

  char buffer[32];
  text(0, kStateY, runStateName(snapshot.state), 1, snapshot.state == RunState::Fault);
  drawBattery(snapshot, buffer, sizeof(buffer));
  drawPressure(snapshot, buffer, sizeof(buffer));
  drawTargetRow(snapshot, buffer, sizeof(buffer));
  drawCountdown(snapshot, buffer, sizeof(buffer));
  bar(0, kBarY, width(), kBarHeight, snapshot.duty / 100.0f);

  display_.flush();
}

// --- data rows -------------------------------------------------------------

void StatusPage::drawBattery(const SystemSnapshot& snapshot, char* buffer, size_t size) {
  if (!snapshot.batteryPresent) return;  // nothing fitted, so leave the corner empty
  text::Buf label(buffer, size);
  label << (uint32_t)snapshot.batteryPercent << '%';
  bool blinkOff = snapshot.batteryLow && ((snapshot.uptimeMs / kLowBatteryBlinkMs) % 2u) == 0u;
  int16_t percentWidth = textWidth(buffer);
  int16_t iconX = width() - percentWidth - 4 - kBatteryIconWidth;
  battery(iconX, 1, blinkOff ? 0 : snapshot.batteryPercent);
  text(width() - percentWidth, kStateY, buffer);
}

void StatusPage::drawPressure(const SystemSnapshot& snapshot, char* buffer, size_t size) {
  if (!snapshot.sensorOk) {
    centered("--.-", kPressureY, 2);
    return;
  }
  text::Buf number(buffer, size);
  number.add(snapshot.kpa, 1);
  constexpr const char* kUnit = "kPa";
  int16_t numberWidth = textWidth(buffer, 2);
  int16_t x = (width() - (numberWidth + 4 + textWidth(kUnit))) / 2;
  if (x < 0) x = 0;
  text(x, kPressureY, buffer, 2);
  // +8 lands the unit on the baseline of the size 2 digits.
  text(x + numberWidth + 4, kPressureY + 8, kUnit);
}

void StatusPage::drawTargetRow(const SystemSnapshot& snapshot, char* buffer, size_t size) {
  if (snapshot.state == RunState::Fault) {
    text::Buf fault(buffer, size);
    fault << "FAULT " << snapshot.faultCode;
    banner(buffer, kTargetY);
    return;
  }
  if (!snapshot.sensorOk) {
    banner("NO SENSOR", kTargetY);
    return;
  }
  text::Buf target(buffer, size);
  target << "tgt ";
  if (snapshot.targetActive) {
    target.add(snapshot.targetKpa, 1);
    target << " kPa";
  } else {
    target << "--";
  }
  text(0, kTargetY, buffer);

  text::Buf duty(buffer, size);
  duty << "duty " << (uint32_t)(snapshot.duty + 0.5f) << '%';
  textRight(buffer, kTargetY);
}

void StatusPage::drawCountdown(const SystemSnapshot& snapshot, char* buffer, size_t size) {
  // The number is hold time remaining, so it only means something while pulling or holding:
  // after that the run is over and nothing is counting any more.
  bool counting = snapshot.totalSeconds > 0 && (snapshot.state == RunState::Pulling ||
                                                snapshot.state == RunState::Holding);
  if (!counting) {
    centered("--:--", kCountdownY, 2);
    return;
  }
  char minutes[8];
  char seconds[4];
  text::unum(minutes, sizeof(minutes), snapshot.remainSeconds / 60, 2);
  text::unum(seconds, sizeof(seconds), snapshot.remainSeconds % 60, 2);
  text::Buf clock(buffer, size);
  clock << minutes << ':' << seconds;
  // The last minute inverts: it is the one an operator is actually watching.
  centered(buffer, kCountdownY, 2, snapshot.remainSeconds <= 60);
}

// --- drawing ---------------------------------------------------------------

int16_t StatusPage::width() const { return display_.width(); }

int16_t StatusPage::textWidth(const char* text, uint8_t size) const {
  return (int16_t)(text ? strlen(text) : 0) * kGlyphWidth * (int16_t)size;
}

void StatusPage::text(int16_t x, int16_t y, const char* text, uint8_t size, bool inverted) {
  if (inverted) {
    display_.textInverted(x, y, text, size);
  } else {
    display_.text(x, y, text, size);
  }
}

void StatusPage::textRight(const char* value, int16_t y, uint8_t size) {
  int16_t x = width() - textWidth(value, size);
  text(x < 0 ? 0 : x, y, value, size);
}

void StatusPage::centered(const char* value, int16_t y, uint8_t size, bool inverted) {
  int16_t x = (width() - textWidth(value, size)) / 2;
  text(x < 0 ? 0 : x, y, value, size, inverted);
}

void StatusPage::battery(int16_t x, int16_t y, uint8_t percent) {
  display_.rect(x, y, kBatteryBodyWidth, kBatteryBodyHeight, false);
  display_.rect(x + kBatteryBodyWidth, y + 2, 2, 3, true);  // nub
  if (percent == 0) return;
  int16_t interior = kBatteryBodyWidth - 4;  // 1 px frame plus 1 px of gap
  int16_t fill = (int16_t)((int32_t)interior * percent / 100);
  if (fill < 1) fill = 1;  // any charge at all shows something
  if (fill > interior) fill = interior;
  display_.rect(x + 2, y + 2, fill, kBatteryBodyHeight - 4, true);
}

void StatusPage::banner(const char* value, int16_t y) {
  display_.rect(0, y, width(), kBannerHeight, true);
  display_.textInverted(0, y + 1, value, 1);
}

void StatusPage::bar(int16_t x, int16_t y, int16_t w, int16_t h, float fraction) {
  if (fraction < 0.0f) fraction = 0.0f;
  if (fraction > 1.0f) fraction = 1.0f;
  display_.rect(x, y, w, h, false);
  int16_t fill = (int16_t)((float)(w - 2) * fraction);
  if (fill > 0) display_.rect(x + 1, y + 1, fill, h - 2, true);
}

}  // namespace eggplan
