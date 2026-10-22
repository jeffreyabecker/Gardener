#pragma once

// What the panel library expects of Arduino's String, for the host.
//
// The library formats its text with Arduino's String, so a source file that leaves the firmware for
// the check still has to compile against it. This is the same surface over std::string: construction,
// concatenation, the searches and the number parsing the compiled sources use. It is deliberately
// not a second text builder.

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

class String {
 public:
  String() = default;
  String(const char* text) : text_(text != nullptr ? text : "") {}
  String(const std::string& text) : text_(text) {}
  String(char value) : text_(1, value) {}
  String(int value) : text_(std::to_string(value)) {}
  String(unsigned int value) : text_(std::to_string(value)) {}
  String(long value) : text_(std::to_string(value)) {}
  String(unsigned long value) : text_(std::to_string(value)) {}
  String(float value, unsigned int decimals = 2) { format(value, decimals); }
  String(double value, unsigned int decimals = 2) { format(value, decimals); }

  const char* c_str() const { return text_.c_str(); }
  unsigned int length() const { return (unsigned int)text_.size(); }
  bool isEmpty() const { return text_.empty(); }
  char charAt(unsigned int index) const { return index < text_.size() ? text_[index] : '\0'; }
  char operator[](unsigned int index) const { return charAt(index); }

  int indexOf(char value) const {
    const size_t at = text_.find(value);
    return at == std::string::npos ? -1 : (int)at;
  }
  int indexOf(char value, unsigned int from) const {
    const size_t at = text_.find(value, from);
    return at == std::string::npos ? -1 : (int)at;
  }
  int lastIndexOf(char value) const {
    const size_t at = text_.rfind(value);
    return at == std::string::npos ? -1 : (int)at;
  }

  String substring(unsigned int from) const {
    if (from > text_.size()) from = (unsigned int)text_.size();
    return String(text_.substr(from));
  }
  String substring(unsigned int from, unsigned int to) const {
    if (from > text_.size()) from = (unsigned int)text_.size();
    if (to > text_.size()) to = (unsigned int)text_.size();
    if (to < from) to = from;
    return String(text_.substr(from, to - from));
  }

  void trim() {
    size_t begin = 0;
    size_t end = text_.size();
    while (begin < end && isspace((unsigned char)text_[begin])) begin++;
    while (end > begin && isspace((unsigned char)text_[end - 1])) end--;
    text_ = text_.substr(begin, end - begin);
  }
  void toLowerCase() {
    for (char& value : text_) value = (char)tolower((unsigned char)value);
  }
  void toUpperCase() {
    for (char& value : text_) value = (char)toupper((unsigned char)value);
  }

  long toInt() const { return strtol(text_.c_str(), nullptr, 10); }
  float toFloat() const { return (float)atof(text_.c_str()); }
  double toDouble() const { return atof(text_.c_str()); }

  void toCharArray(char* buffer, unsigned int size) const {
    if (buffer == nullptr || size == 0) return;
    const size_t copied = text_.size() < size - 1 ? text_.size() : size - 1;
    memcpy(buffer, text_.c_str(), copied);
    buffer[copied] = '\0';
  }

  bool equalsIgnoreCase(const String& other) const {
    return strcasecmp(text_.c_str(), other.text_.c_str()) == 0;
  }
  bool startsWith(const String& prefix) const {
    return text_.compare(0, prefix.text_.size(), prefix.text_) == 0;
  }
  bool endsWith(const String& suffix) const {
    if (suffix.text_.size() > text_.size()) return false;
    return text_.compare(text_.size() - suffix.text_.size(), suffix.text_.size(),
                         suffix.text_) == 0;
  }

  bool operator==(const String& other) const { return text_ == other.text_; }
  bool operator!=(const String& other) const { return text_ != other.text_; }
  bool operator==(const char* other) const { return other != nullptr && text_ == other; }
  bool operator!=(const char* other) const { return !(*this == other); }

  String& operator=(const char* value) {
    text_ = value != nullptr ? value : "";
    return *this;
  }
  String& operator+=(const String& other) {
    text_ += other.text_;
    return *this;
  }
  String& operator+=(const char* other) {
    if (other != nullptr) text_ += other;
    return *this;
  }
  String& operator+=(char other) {
    text_ += other;
    return *this;
  }

 private:
  void format(double value, unsigned int decimals) {
    char buffer[48];
    snprintf(buffer, sizeof buffer, "%.*f", (int)decimals, value);
    text_ = buffer;
  }

  std::string text_;
};

inline String operator+(const String& left, const String& right) {
  String out(left);
  out += right;
  return out;
}
inline String operator+(const String& left, const char* right) {
  String out(left);
  out += right;
  return out;
}
inline String operator+(const char* left, const String& right) {
  String out(left);
  out += right;
  return out;
}
inline bool operator==(const char* left, const String& right) { return right == left; }
inline bool operator!=(const char* left, const String& right) { return right != left; }
