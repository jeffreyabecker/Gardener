#pragma once

#include <Arduino.h>

namespace gardener {

// A reply payload, built one item at a time: `key=value`, space separated, and nothing else.
//
// Every payload in the protocol is a list of these — a read, a write's echo, an error's detail —
// so this is the one place a value reaches the wire, and the one place the no-spaces-in-a-value
// rule is enforced. That rule needs enforcing rather than assuming because some values come from
// a library string: a chip model or a fault code can carry anything, and a space in one would
// turn one item into two.
class Items {
 public:
  bool empty() const { return buf_.length() == 0; }

  // Text. Whitespace inside it is written as `_`, and a missing value is the `-` the protocol
  // uses for "not applicable" — which is also why the caller can pass an empty fault code.
  void add(const char* key, const char* value) {
    start(key);
    if (!value || !value[0]) {
      buf_ += '-';
      return;
    }
    for (const char* cursor = value; *cursor != '\0'; cursor++) {
      char c = *cursor;
      buf_ += (c == ' ' || c == '\t' || c == '\n' || c == '\r') ? '_' : c;
    }
  }

  void addU32(const char* key, uint32_t value) {
    start(key);
    buf_ += String(value);
  }

  void addFloat(const char* key, float value, uint8_t decimals) {
    start(key);
    // The cast picks String(float, decimals) out of the overload set: a uint8_t also matches
    // the integer constructors, whose base parameter is an unsigned char.
    buf_ += String(value, (unsigned int)decimals);
  }

  // `on`/`off`, which is how the protocol spells a flag a host sets and reads back.
  void addFlag(const char* key, bool value) { add(key, value ? "on" : "off"); }

  const String& text() const { return buf_; }

 private:
  void start(const char* key) {
    if (buf_.length() > 0) buf_ += ' ';
    buf_ += key;
    buf_ += '=';
  }

  String buf_;
};

}  // namespace gardener
