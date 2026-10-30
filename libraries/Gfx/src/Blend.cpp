#include "Blend.h"

namespace gardener::gfx {

Blend Blend::then(const Blend& outer) const {
  Blend next = *this;
  for (uint8_t i = 0; i < outer.count_; i++) {
    // Plain changes nothing and would only fill a slot.
    if (outer.rules_[i] == Rule::Plain) continue;
    if (next.count_ == MaxStages) {
      // A chain longer than the stages it holds loses its oldest, which is the one furthest from
      // what the page is drawing now.
      for (uint8_t j = 1; j < MaxStages; j++) {
        next.rules_[j - 1] = next.rules_[j];
        next.levels_[j - 1] = next.levels_[j];
      }
      next.count_ = MaxStages - 1;
    }
    next.rules_[next.count_] = outer.rules_[i];
    next.levels_[next.count_] = outer.levels_[i];
    next.count_++;
  }
  return next;
}

uint8_t Blend::apply(uint8_t value) const {
  for (uint8_t i = 0; i < count_; i++) {
    switch (rules_[i]) {
      case Rule::Invert:
        value = static_cast<uint8_t>(255 - value);
        break;
      case Rule::Dim:
        // Rounded, so a level of 128 on 255 comes back as 128 rather than 127.
        value = static_cast<uint8_t>((value * levels_[i] + 127) / 255);
        break;
      default:
        break;
    }
  }
  return value;
}

}  // namespace gardener::gfx
