#include "app/AppDisplay.h"

#include <type_traits>

namespace eggplan {
namespace {

constexpr int16_t kBatteryIconWidth = 12;
constexpr int16_t kBatteryBodyWidth = 10;
constexpr int16_t kBatteryBodyHeight = 7;
constexpr int16_t kBatteryLabelY = 0;
constexpr int16_t kBatteryIconY = 1;

// Below the low threshold the fill blinks. With no buttons and no alarm, that blink is the
// entire low battery notification.
constexpr uint32_t kLowBatteryBlinkMs = 500;

// The pass-through is what makes this a Display at all: if a verb is ever added to the
// interface and not forwarded here, this stops the build rather than leaving a display the app
// cannot hand to a page.
static_assert(!std::is_abstract<AppDisplay>::value,
              "AppDisplay must forward every Display verb to stay concrete");

}  // namespace

// The battery goes down last, so it lands on top of whatever the page drew and no page has to
// know it exists.
void AppDisplay::flush() {
  drawBattery();
  inner_.flush();
}

void AppDisplay::drawBattery() {
  if (!snapshot_.batteryPresent) return;  // nothing fitted, so leave the corner empty

  String label = String(snapshot_.batteryPercent) + '%';
  int16_t labelWidth = 0;
  int16_t labelHeight = 0;
  inner_.textBounds(label, 1, labelWidth, labelHeight);

  bool blinkOff = snapshot_.batteryLow && ((snapshot_.uptimeMs / kLowBatteryBlinkMs) % 2u) == 0u;
  int16_t iconX = inner_.width() - labelWidth - 4 - kBatteryIconWidth;
  batteryIcon(iconX, kBatteryIconY, blinkOff ? 0 : snapshot_.batteryPercent);
  inner_.text(inner_.width() - labelWidth, kBatteryLabelY, label, 1, false);
}

void AppDisplay::batteryIcon(int16_t x, int16_t y, uint8_t percent) {
  inner_.rect(x, y, kBatteryBodyWidth, kBatteryBodyHeight);
  inner_.fillRect(x + kBatteryBodyWidth, y + 2, 2, 3);  // nub
  if (percent == 0) return;
  int16_t interior = kBatteryBodyWidth - 4;  // 1 px frame plus 1 px of gap
  int16_t fill = (int16_t)((int32_t)interior * percent / 100);
  if (fill < 1) fill = 1;  // any charge at all shows something
  if (fill > interior) fill = interior;
  inner_.fillRect(x + 2, y + 2, fill, kBatteryBodyHeight - 4);
}

}  // namespace eggplan
