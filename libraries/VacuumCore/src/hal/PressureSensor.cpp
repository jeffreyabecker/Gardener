#include "hal/PressureSensor.h"

#include <math.h>

#include "core/Config.h"
#include "core/Log.h"
#include "core/Math.h"

namespace eggplan {
namespace {
constexpr float kFilterAlpha = 0.3f;
}

bool PressureSensor::begin(TwoWire& wire) {
  ambientKpa_ = cfg::DEFAULT_AMBIENT_KPA;
  present_ = sensor_.begin(config_.address, &wire);
  if (!present_) {
    LOGE("mprls: not found at 0x%02X", config_.address);
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
  float kpa = sensor_.readPressure() * 0.1f;  // the library reports hPa
  if (isnan(kpa) || kpa <= config_.minKpa || kpa > config_.maxKpa) {
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
  if (nowMs - lastPollMs_ < config_.pollMs) return;
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
  float deficit = ambientKpa_ - filteredKpa_;
  return deficit > 0.0f ? deficit : 0.0f;
}

float PressureSensor::vacuumPercent() const {
  if (!valid_ || ambientKpa_ <= 0.0f) return 0.0f;
  return clampf(vacuumKpa() / ambientKpa_ * 100.0f, 0.0f, 100.0f);
}

bool PressureSensor::setAmbientKpa(float kpa) {
  if (kpa < 10.0f || kpa > config_.maxKpa) {
    LOGW("mprls: rejected ambient reference %.2f kPa", kpa);
    return false;
  }
  ambientKpa_ = kpa;
  LOGI("mprls: ambient reference set to %.2f kPa", kpa);
  return true;
}

void PressureSensor::captureAmbient() {
  if (!valid_) return;
  setAmbientKpa(filteredKpa_);
}

}  // namespace eggplan
