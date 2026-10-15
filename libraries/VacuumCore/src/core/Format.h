#pragma once

#include <Arduino.h>

namespace eggplan {

// Number to string, shared by the wire encoding and the OLED.
inline String formatFloat(float value, uint8_t decimals = 1) {
  char buffer[24];
  dtostrf(value, 0, decimals, buffer);
  return String(buffer);
}

}  // namespace eggplan
