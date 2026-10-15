#include "app/StatusPage.h"

#include "core/Config.h"

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

// Below the low threshold the battery fill blinks. With no buttons and no alarm, that blink
// is the entire low battery notification.
constexpr uint32_t kLowBatteryBlinkMs = 500;

// The countdown is the one field with a fixed width, so "9:5" reads as "09:05".
String twoDigits(uint32_t value) { return (value < 10 ? "0" : "") + String(value); }

}  // namespace

void StatusPage::splash() {
  gfx_.fillScreen(kPaper);
  text(0, 0, "Eggplan Vacuum");
  text(0, 12, String("fw ") + cfg::FIRMWARE_VERSION);
}

void StatusPage::render(const SystemSnapshot& snapshot) {
  gfx_.fillScreen(kPaper);

  text(0, kStateY, runStateName(snapshot.state), 1, snapshot.state == RunState::Fault);
  drawBattery(snapshot);
  drawPressure(snapshot);
  drawTargetRow(snapshot);
  drawCountdown(snapshot);
  bar(0, kBarY, gfx_.width(), kBarHeight, snapshot.duty / 100.0f);
}

// --- data rows -------------------------------------------------------------

void StatusPage::drawBattery(const SystemSnapshot& snapshot) {
  if (!snapshot.batteryPresent) return;  // nothing fitted, so leave the corner empty
  String label = String(snapshot.batteryPercent) + '%';
  bool blinkOff = snapshot.batteryLow && ((snapshot.uptimeMs / kLowBatteryBlinkMs) % 2u) == 0u;
  int16_t percentWidth = textWidth(label);
  int16_t iconX = gfx_.width() - percentWidth - 4 - kBatteryIconWidth;
  battery(iconX, 1, blinkOff ? 0 : snapshot.batteryPercent);
  text(gfx_.width() - percentWidth, kStateY, label);
}

void StatusPage::drawPressure(const SystemSnapshot& snapshot) {
  if (!snapshot.sensorOk) {
    centered("--.-", kPressureY, 2);
    return;
  }
  String number(snapshot.kpa, 1);
  constexpr const char* kUnit = "kPa";
  int16_t numberWidth = textWidth(number, 2);
  int16_t x = (gfx_.width() - (numberWidth + 4 + textWidth(kUnit))) / 2;
  if (x < 0) x = 0;
  text(x, kPressureY, number, 2);
  // +8 lands the unit on the baseline of the size 2 digits.
  text(x + numberWidth + 4, kPressureY + 8, kUnit);
}

void StatusPage::drawTargetRow(const SystemSnapshot& snapshot) {
  if (snapshot.state == RunState::Fault) {
    banner(String("FAULT ") + snapshot.faultCode, kTargetY);
    return;
  }
  if (!snapshot.sensorOk) {
    banner("NO SENSOR", kTargetY);
    return;
  }
  String target = "tgt ";
  target += snapshot.targetActive ? String(snapshot.targetKpa, 1) + " kPa" : "--";
  text(0, kTargetY, target);
  textRight(String("duty ") + String(snapshot.duty, 0) + '%', kTargetY);
}

void StatusPage::drawCountdown(const SystemSnapshot& snapshot) {
  // The number is hold time remaining, so it only means something while pulling or holding:
  // after that the run is over and nothing is counting any more.
  bool counting = snapshot.totalSeconds > 0 && (snapshot.state == RunState::Pulling ||
                                                snapshot.state == RunState::Holding);
  if (!counting) {
    centered("--:--", kCountdownY, 2);
    return;
  }
  String clock = twoDigits(snapshot.remainSeconds / 60) + ':' +
                 twoDigits(snapshot.remainSeconds % 60);
  // The last minute inverts: it is the one an operator is actually watching.
  centered(clock, kCountdownY, 2, snapshot.remainSeconds <= 60);
}

// --- drawing ---------------------------------------------------------------

int16_t StatusPage::textWidth(const String& value, uint8_t size) const {
  return (int16_t)value.length() * kGlyphWidth * (int16_t)size;
}

void StatusPage::text(int16_t x, int16_t y, const String& value, uint8_t size, bool invert) {
  gfx_.setTextSize(size);
  // An opaque background is what makes a banner readable; setting ink and background to the
  // same value is how Adafruit_GFX means "transparent".
  if (invert) {
    gfx_.setTextColor(kPaper, kInk);
  } else {
    gfx_.setTextColor(kInk);
  }
  gfx_.setCursor(x, y);
  gfx_.print(value);
}

void StatusPage::textRight(const String& value, int16_t y, uint8_t size) {
  int16_t x = gfx_.width() - textWidth(value, size);
  text(x < 0 ? 0 : x, y, value, size);
}

void StatusPage::centered(const String& value, int16_t y, uint8_t size, bool invert) {
  int16_t x = (gfx_.width() - textWidth(value, size)) / 2;
  text(x < 0 ? 0 : x, y, value, size, invert);
}

void StatusPage::battery(int16_t x, int16_t y, uint8_t percent) {
  gfx_.drawRect(x, y, kBatteryBodyWidth, kBatteryBodyHeight, kInk);
  gfx_.fillRect(x + kBatteryBodyWidth, y + 2, 2, 3, kInk);  // nub
  if (percent == 0) return;
  int16_t interior = kBatteryBodyWidth - 4;  // 1 px frame plus 1 px of gap
  int16_t fill = (int16_t)((int32_t)interior * percent / 100);
  if (fill < 1) fill = 1;  // any charge at all shows something
  if (fill > interior) fill = interior;
  gfx_.fillRect(x + 2, y + 2, fill, kBatteryBodyHeight - 4, kInk);
}

void StatusPage::banner(const String& value, int16_t y) {
  gfx_.fillRect(0, y, gfx_.width(), kBannerHeight, kInk);
  text(0, y + 1, value, 1, true);
}

void StatusPage::bar(int16_t x, int16_t y, int16_t w, int16_t h, float fraction) {
  if (fraction < 0.0f) fraction = 0.0f;
  if (fraction > 1.0f) fraction = 1.0f;
  gfx_.drawRect(x, y, w, h, kInk);
  int16_t fill = (int16_t)((float)(w - 2) * fraction);
  if (fill > 0) gfx_.fillRect(x + 1, y + 1, fill, h - 2, kInk);
}

}  // namespace eggplan
