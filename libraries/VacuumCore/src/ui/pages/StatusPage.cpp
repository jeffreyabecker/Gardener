#include <string.h>

#include "core/Text.h"
#include "ui/Renderer.h"
#include "ui/pages/Pages.h"

namespace eggplan {
namespace pages {
namespace {

// One page, so the layout lives here and not in the renderer. Row positions assume the
// classic 5x7 font: 8 px of pitch at size 1, 16 px at size 2.
constexpr int16_t kStateY = 0;
constexpr int16_t kPressureY = 9;
constexpr int16_t kTargetY = 26;
constexpr int16_t kCountdownY = 36;
constexpr int16_t kBarY = 56;
constexpr int16_t kBarHeight = 8;

// Below the low threshold the battery fill blinks. With no buttons and no alarm, that
// blink is the entire low battery notification (Q11).
constexpr uint32_t kLowBatteryBlinkMs = 500;

void drawBattery(Renderer& renderer, const SystemSnapshot& snapshot, char* buffer, size_t size) {
  if (!snapshot.batteryPresent) return;  // nothing fitted, so leave the corner empty
  text::Buf label(buffer, size);
  label << (uint32_t)snapshot.batteryPercent << '%';
  bool blinkOff = snapshot.batteryLow && ((snapshot.uptimeMs / kLowBatteryBlinkMs) % 2u) == 0u;
  int16_t percentWidth = renderer.textWidth(buffer);
  int16_t iconX = renderer.width() - percentWidth - 4 - Renderer::kBatteryIconWidth;
  renderer.battery(iconX, 1, blinkOff ? 0 : snapshot.batteryPercent);
  renderer.text(renderer.width() - percentWidth, kStateY, buffer);
}

void drawPressure(Renderer& renderer, const SystemSnapshot& snapshot, char* buffer, size_t size) {
  if (!snapshot.sensorOk) {
    renderer.centered("--.-", kPressureY, 2);
    return;
  }
  text::Buf number(buffer, size);
  number.add(snapshot.kpa, 1);
  constexpr const char* kUnit = "kPa";
  int16_t numberWidth = renderer.textWidth(buffer, 2);
  int16_t x = (renderer.width() - (numberWidth + 4 + renderer.textWidth(kUnit))) / 2;
  if (x < 0) x = 0;
  renderer.text(x, kPressureY, buffer, 2);
  // +8 lands the unit on the baseline of the size 2 digits.
  renderer.text(x + numberWidth + 4, kPressureY + 8, kUnit);
}

void drawTargetRow(Renderer& renderer, const SystemSnapshot& snapshot, char* buffer, size_t size) {
  if (snapshot.state == RunState::Fault) {
    text::Buf banner(buffer, size);
    banner << "FAULT " << snapshot.faultCode;
    renderer.banner(buffer, kTargetY);
    return;
  }
  if (!snapshot.sensorOk) {
    renderer.banner("NO SENSOR", kTargetY);
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
  renderer.text(0, kTargetY, buffer);

  text::Buf duty(buffer, size);
  duty << "duty " << (uint32_t)(snapshot.duty + 0.5f) << '%';
  renderer.textRight(buffer, kTargetY);
}

void drawCountdown(Renderer& renderer, const SystemSnapshot& snapshot, char* buffer, size_t size) {
  // The number is hold time remaining (Q1), so it only means something while pulling or
  // holding: after that the run is over and nothing is counting any more.
  bool counting = snapshot.totalSeconds > 0 && (snapshot.state == RunState::Pulling ||
                                                snapshot.state == RunState::Holding);
  if (!counting) {
    renderer.centered("--:--", kCountdownY, 2);
    return;
  }
  text::Buf clock(buffer, size);
  char minutes[8];
  char seconds[4];
  text::unum(minutes, sizeof(minutes), snapshot.remainSeconds / 60, 2);
  text::unum(seconds, sizeof(seconds), snapshot.remainSeconds % 60, 2);
  clock << minutes << ':' << seconds;
  // The last minute inverts: it is the one an operator is actually watching.
  renderer.centered(buffer, kCountdownY, 2, snapshot.remainSeconds <= 60);
}

}  // namespace

void renderStatus(Renderer& renderer, const SystemSnapshot& snapshot) {
  char buffer[32];

  renderer.text(0, kStateY, runStateName(snapshot.state), 1, snapshot.state == RunState::Fault);
  drawBattery(renderer, snapshot, buffer, sizeof(buffer));
  drawPressure(renderer, snapshot, buffer, sizeof(buffer));
  drawTargetRow(renderer, snapshot, buffer, sizeof(buffer));
  drawCountdown(renderer, snapshot, buffer, sizeof(buffer));
  renderer.bar(0, kBarY, renderer.width(), kBarHeight, snapshot.duty / 100.0f);
}

}  // namespace pages
}  // namespace eggplan
