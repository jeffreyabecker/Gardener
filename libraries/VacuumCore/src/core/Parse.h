#pragma once

#include <stdint.h>
#include <strings.h>

namespace gardener {

// Strict scalar parsers. A value that does not parse in full is a bad_argument, never a silent
// zero. Command tokenization belongs to the domains; these only turn one token into a value.

// A flag value: `on` or `off`, either case.
inline bool parseFlag(const char* text, bool& out) {
  if (!text) return false;
  if (strcasecmp(text, "on") == 0) {
    out = true;
    return true;
  }
  if (strcasecmp(text, "off") == 0) {
    out = false;
    return true;
  }
  return false;
}

inline bool parseFloat(const char* text, float& out) {
  if (!text || *text == '\0') return false;
  const char* cursor = text;
  bool negative = false;
  if (*cursor == '+' || *cursor == '-') {
    negative = (*cursor == '-');
    cursor++;
  }

  float value = 0.0f;
  bool any = false;
  while (*cursor >= '0' && *cursor <= '9') {
    value = value * 10.0f + (float)(*cursor - '0');
    cursor++;
    any = true;
  }
  if (*cursor == '.') {
    cursor++;
    float scale = 0.1f;
    while (*cursor >= '0' && *cursor <= '9') {
      value += (float)(*cursor - '0') * scale;
      scale *= 0.1f;
      cursor++;
      any = true;
    }
  }
  if (!any || *cursor != '\0') return false;
  out = negative ? -value : value;
  return true;
}

inline bool parseU32(const char* text, uint32_t& out) {
  if (!text || *text == '\0') return false;
  uint32_t value = 0;
  for (const char* cursor = text; *cursor != '\0'; cursor++) {
    if (*cursor < '0' || *cursor > '9') return false;
    const uint32_t digit = (uint32_t)(*cursor - '0');
    if (value > (UINT32_MAX - digit) / 10) return false;
    value = value * 10 + digit;
  }
  out = value;
  return true;
}

}  // namespace gardener
