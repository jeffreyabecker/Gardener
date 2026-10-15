#include "PressureSensor.h"

#include <math.h>

#include "Log.h"
#include "Util.h"

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
  float hpa = 0.0f;
  if (readNow(hpa)) {
    filteredHpa_ = hpa;
    lastHpa_ = hpa;
    valid_ = true;
  }
  LOGI("mprls: online, ambient reference %.1f hPa", filteredHpa_);
  return true;
}

bool PressureSensor::readNow(float& outHpa) {
  float hpa = sensor_.readPressure();
  if (isnan(hpa) || hpa <= cfg::MPRLS_MIN_HPA || hpa > cfg::MPRLS_MAX_HPA) {
    errorCount_++;
    return false;
  }
  outHpa = hpa;
  lastHpa_ = hpa;
  lastSampleMs_ = millis();
  return true;
}

void PressureSensor::update(uint32_t nowMs) {
  if (!present_) return;
  if (nowMs - lastPollMs_ < cfg::MPRLS_POLL_MS) return;
  lastPollMs_ = nowMs;

  float hpa = 0.0f;
  if (!readNow(hpa)) return;

  window_[windowIndex_] = hpa;
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
    filteredHpa_ = median;
    valid_ = true;
  } else {
    filteredHpa_ += kFilterAlpha * (median - filteredHpa_);
  }
}

float PressureSensor::vacuumHpa() const {
  if (!valid_) return 0.0f;
  float v = ambientHpa_ - filteredHpa_;
  return v > 0.0f ? v : 0.0f;
}

float PressureSensor::vacuumPercent() const {
  if (!valid_ || ambientHpa_ <= 0.0f) return 0.0f;
  return clampf(vacuumHpa() / ambientHpa_ * 100.0f, 0.0f, 100.0f);
}

void PressureSensor::setAmbientHpa(float hpa) {
  if (hpa < 100.0f || hpa > cfg::MPRLS_MAX_HPA) {
    LOGW("mprls: rejected ambient reference %.1f hPa", hpa);
    return;
  }
  ambientHpa_ = hpa;
  LOGI("mprls: ambient reference set to %.1f hPa", hpa);
}

void PressureSensor::captureAmbient() {
  if (!valid_) return;
  setAmbientHpa(filteredHpa_);
}

}  // namespace eggplan
