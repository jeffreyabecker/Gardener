#include "hal/PressureSensor.h"

#include <math.h>

#include "core/Log.h"
#include "core/Util.h"

namespace eggplan {
namespace {
constexpr float kFilterAlpha = 0.3f;
constexpr uint32_t kStaleSampleMs = 1000;
}

bool PressureSensor::begin(TwoWire& wire) {
  present_ = sensor_.begin(cfg::MPRLS_I2C_ADDR, &wire);
  if (!present_) {
    LOGE("mprls: not found at 0x%02X", cfg::MPRLS_I2C_ADDR);
    return false;
  }
  delay(15);  // first conversion after power up
  float kpa = 0.0f;
  if (readNow(kpa)) {
    filteredKpa_ = kpa;
    lastKpa_ = kpa;
    valid_ = true;
  }
  LOGI("mprls: online, ambient reference %.2f kPa", filteredKpa_);
  return true;
}

bool PressureSensor::readNow(float& outKpa) {
  // The Adafruit library reports hPa; everything above this boundary is kPa.
  float kpa = sensor_.readPressure() * 0.1f;
  if (isnan(kpa) || kpa <= cfg::MPRLS_MIN_KPA || kpa > cfg::MPRLS_MAX_KPA) {
    errorCount_++;
    return false;
  }
  outKpa = kpa;
  lastKpa_ = kpa;
  lastSampleMs_ = millis();
  return true;
}

void PressureSensor::update(uint32_t nowMs) {
  if (!present_) return;
  if (nowMs - lastPollMs_ < cfg::MPRLS_POLL_MS) return;
  lastPollMs_ = nowMs;

  float kpa = 0.0f;
  if (!readNow(kpa)) return;

  window_[windowIndex_] = kpa;
  windowIndex_ = (uint8_t)((windowIndex_ + 1) % 5);
  if (windowCount_ < 5) windowCount_++;

  // Median of the last few samples kills the occasional MPRLS outlier.
  float sorted[5];
  memcpy(sorted, window_, sizeof(sorted));
  for (uint8_t i = 1; i < windowCount_; i++) {
    float key = sorted[i];
    int8_t j = (int8_t)i - 1;
    while (j >= 0 && sorted[j] > key) {
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = key;
  }
  float median = sorted[windowCount_ / 2];

  if (!valid_) {
    filteredKpa_ = median;
    valid_ = true;
  } else {
    filteredKpa_ += kFilterAlpha * (median - filteredKpa_);
  }
}

float PressureSensor::vacuumKpa() const {
  if (!valid_) return 0.0f;
  float v = ambientKpa_ - filteredKpa_;
  return v > 0.0f ? v : 0.0f;
}

float PressureSensor::vacuumPercent() const {
  if (!valid_ || ambientKpa_ <= 0.0f) return 0.0f;
  return clampf(vacuumKpa() / ambientKpa_ * 100.0f, 0.0f, 100.0f);
}

void PressureSensor::setAmbientKpa(float kpa) {
  if (kpa < 10.0f || kpa > cfg::MPRLS_MAX_KPA) {
    LOGW("mprls: rejected ambient reference %.2f kPa", kpa);
    return;
  }
  ambientKpa_ = kpa;
  LOGI("mprls: ambient reference set to %.2f kPa", kpa);
}

void PressureSensor::captureAmbient() {
  if (!valid_) return;
  setAmbientKpa(filteredKpa_);
}

}  // namespace eggplan
