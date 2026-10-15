#pragma once

#include <stddef.h>
#include <stdint.h>

// Numbers and text into caller supplied buffers: the whole replacement for stdio.
//
// The firmware prints a handful of shapes - a pressure with one or two decimals, a
// percentage, a countdown, a hex address - so it formats them here instead of linking
// printf. `Buf` is a bounded builder over a char array; decimals are always explicit
// because the precision of a number is a decision, not a default.

namespace eggplan {
namespace text {

// Decimal, zero padded to at least `width` characters. Returns the length written.
inline size_t unum(char* out, size_t size, uint32_t value, uint8_t width = 1) {
  if (size == 0) return 0;
  char digits[10];
  uint8_t count = 0;
  do {
    digits[count++] = (char)('0' + (value % 10u));
    value /= 10u;
  } while (value > 0 && count < sizeof(digits));

  size_t total = count > width ? count : width;
  if (total > size - 1) total = size - 1;
  for (size_t i = 0; i < total; i++) {
    out[total - 1 - i] = i < count ? digits[i] : '0';
  }
  out[total] = '\0';
  return total;
}

// Two uppercase hex digits, no prefix.
inline size_t hex8(char* out, size_t size, uint8_t value) {
  if (size < 3) {
    if (size > 0) out[0] = '\0';
    return 0;
  }
  const char* digits = "0123456789ABCDEF";
  out[0] = digits[(value >> 4) & 0x0F];
  out[1] = digits[value & 0x0F];
  out[2] = '\0';
  return 2;
}

// Fixed point, rounded half away from zero. A value that rounds to zero prints unsigned,
// so -0.04 at one decimal is "0.0" rather than "-0.0".
inline size_t fixed(char* out, size_t size, float value, uint8_t decimals) {
  if (size == 0) return 0;
  double magnitude = (double)value;
  bool negative = magnitude < 0.0;
  if (negative) magnitude = -magnitude;

  double scale = 1.0;
  for (uint8_t i = 0; i < decimals; i++) scale *= 10.0;

  // Scaling in double matters: in float, a value that sits just below a tie rounds up to
  // the tie, which turns 9.95 into 10.0 and 101.325 into 101.33.
  double rounded = magnitude * scale + 0.5;
  if (rounded > 4294967295.0) rounded = 4294967295.0;  // never print absurd input
  uint32_t scaled = (uint32_t)rounded;
  uint32_t whole = (uint32_t)((double)scaled / scale);
  uint32_t fraction = (uint32_t)((double)scaled - (double)whole * scale);
  if (scaled == 0) negative = false;

  char buffer[16];
  size_t wholeLength = unum(buffer, sizeof(buffer), whole);

  size_t length = 0;
  if (negative && length + 1 < size) out[length++] = '-';
  for (size_t i = 0; i < wholeLength && length + 1 < size; i++) out[length++] = buffer[i];
  if (decimals > 0) {
    if (length + 1 < size) out[length++] = '.';
    char digits[12];
    size_t fractionLength = unum(digits, sizeof(digits), fraction, decimals);
    for (size_t i = 0; i < fractionLength && length + 1 < size; i++) out[length++] = digits[i];
  }
  out[length] = '\0';
  return length;
}

// A bounded builder over a char array. Appending never overflows and never allocates:
// once the buffer is full, further input is dropped and the string stays terminated.
class Buf {
 public:
  Buf(char* data, size_t size) : data_(data), size_(size) {
    if (size_ > 0) data_[0] = '\0';
  }

  void add(const char* value) { append(value, value ? lengthOf(value) : 0); }

  void add(char value) {
    char single[2] = {value, '\0'};
    append(single, 1);
  }

  void add(int32_t value) {
    if (value < 0) add('-');
    char digits[12];
    uint32_t magnitude = value < 0 ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
    size_t count = unum(digits, sizeof(digits), magnitude);
    append(digits, count);
  }

  void add(uint32_t value) {
    char digits[12];
    size_t count = unum(digits, sizeof(digits), value);
    append(digits, count);
  }

  void add(float value, uint8_t decimals) {
    char digits[16];
    size_t count = fixed(digits, sizeof(digits), value, decimals);
    append(digits, count);
  }

  Buf& operator<<(const char* value) {
    add(value);
    return *this;
  }
  Buf& operator<<(char value) {
    add(value);
    return *this;
  }
  Buf& operator<<(int32_t value) {
    add(value);
    return *this;
  }
  Buf& operator<<(uint32_t value) {
    add(value);
    return *this;
  }

  const char* c_str() const { return data_; }
  size_t length() const { return length_; }
  bool full() const { return length_ + 1 >= size_; }

 private:
  static size_t lengthOf(const char* value) {
    size_t length = 0;
    while (value[length] != '\0') length++;
    return length;
  }

  void append(const char* value, size_t count) {
    for (size_t i = 0; i < count; i++) {
      if (length_ + 1 >= size_) break;
      data_[length_++] = value[i];
    }
    // Terminate on every path: stopping early must not leave the tail of a longer string
    // behind from whatever the buffer held before.
    data_[length_] = '\0';
  }

  char* data_;
  size_t size_;
  size_t length_ = 0;
};

}  // namespace text
}  // namespace eggplan
