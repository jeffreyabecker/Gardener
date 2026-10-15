#pragma once

#include <Arduino.h>

// Small helpers shared by the firmware modules.
namespace eggplan {

inline float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

inline uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++) {
      crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1u)));
    }
  }
  return crc;
}

inline uint32_t crc32(const uint8_t* data, size_t len) {
  return crc32Update(0xFFFFFFFFu, data, len) ^ 0xFFFFFFFFu;
}

inline uint32_t crc32(const String& s) {
  return crc32((const uint8_t*)s.c_str(), s.length());
}

inline String formatFloat(float value, uint8_t decimals = 1) {
  char buf[24];
  dtostrf(value, 0, decimals, buf);
  return String(buf);
}

// Pressure conversions, all inputs in kPa.
inline float kpaToHpa(float kpa) { return kpa * 10.0f; }
inline float kpaToBar(float kpa) { return kpa * 0.01f; }
inline float kpaToPsi(float kpa) { return kpa * 0.14503774f; }
inline float kpaToInHg(float kpa) { return kpa * 0.29529988f; }
inline float kpaToMmHg(float kpa) { return kpa * 7.50061683f; }

inline String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", (uint8_t)c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out;
}

}  // namespace eggplan
