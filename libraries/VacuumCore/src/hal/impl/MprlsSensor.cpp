#include "hal/impl/MprlsSensor.h"

#include <math.h>
#include <string.h>
#include <type_traits>

namespace gardener {
namespace {
constexpr float kFilterAlpha = 0.3f;

// The part's own facts, so they move with the part rather than with the board: its address on
// the bus, how often it is worth asking, and the range it can measure. Everything outside the
// range is refused rather than believed, which is what keeps a dead part from reading as vacuum.
constexpr uint8_t kI2cAddr = 0x18;
constexpr uint32_t kPollMs = 20;
constexpr float kMinKpa = 0.0f;
constexpr float kMaxKpa = 172.369f;  // 25 PSI full scale
}  // namespace

// Nothing else spells the interface out, so a verb added to PressureSensor and not implemented here
// would leave this class abstract - silently, until something tried to construct it.
static_assert(!std::is_abstract<MprlsSensor>::value,
              "MprlsSensor must implement every PressureSensor verb");

bool MprlsSensor::begin() {
  present_ = sensor_.begin(kI2cAddr, &wire_);
  if (!present_) return false;
  delay(15);  // first conversion after power up
  float kpa = 0.0f;
  if (readNow(kpa)) {
    ambientKpa_ = kpa;
    filteredKpa_ = kpa;
    valid_ = true;
  }
  return true;
}

bool MprlsSensor::readNow(float& outKpa) {
  float kpa = sensor_.readPressure() * 0.1f;  // the library reports hPa
  if (isnan(kpa) || kpa <= kMinKpa || kpa > kMaxKpa) return false;
  outKpa = kpa;
  return true;
}

void MprlsSensor::update(uint32_t nowMs) {
  if (!present_) return;
  if (nowMs - lastPollMs_ < kPollMs) return;
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
    // begin()'s first read did not land, so the first sample here is the reference.
    if (ambientKpa_ <= 0.0f) ambientKpa_ = median;
    filteredKpa_ = median;
    valid_ = true;
  } else {
    filteredKpa_ += kFilterAlpha * (median - filteredKpa_);
  }
}

float MprlsSensor::kpa() const {
  if (!valid_) return 0.0f;
  const float depth = ambientKpa_ - filteredKpa_;
  return depth > 0.0f ? depth : 0.0f;
}

}  // namespace gardener
