#pragma once

// The little of Arduino.h the drawing sources include, for the host: the program-space reads the
// font tables go through, which are plain memory here, and the String the settings scenes carry.

#include <stdint.h>
#include <stdio.h>

#include <string>

#ifndef PROGMEM
#define PROGMEM
#endif

#define pgm_read_byte(addr) (*reinterpret_cast<const uint8_t*>(addr))
#define pgm_read_word(addr) (*reinterpret_cast<const uint16_t*>(addr))

// The Arduino string, as much of it as the drawing side names: the text of a label or a value, its
// C string, whether it is empty and whether two are equal. It keeps a std::string, so a case reads
// one back the way a page would.
class String {
 public:
  String() {}
  String(const char* text) : text_(text == nullptr ? "" : text) {}
  String(long value) {
    char printed[24];
    snprintf(printed, sizeof(printed), "%ld", value);
    text_ = printed;
  }

  const char* c_str() const { return text_.c_str(); }
  bool isEmpty() const { return text_.empty(); }

  String& operator=(const char* text) {
    text_ = text == nullptr ? "" : text;
    return *this;
  }

  bool operator==(const String& other) const { return text_ == other.text_; }
  bool operator!=(const String& other) const { return text_ != other.text_; }
  bool operator<(const String& other) const { return text_ < other.text_; }

  String operator+(const String& other) const { return String((text_ + other.text_).c_str()); }
  String operator+(const char* other) const {
    return String((text_ + (other == nullptr ? "" : other)).c_str());
  }
  friend String operator+(const char* left, const String& right) { return String(left) + right; }

 private:
  std::string text_;
};

