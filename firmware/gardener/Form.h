#pragma once

#include <Arduino.h>
#include <ctype.h>

namespace gardener::runtime {

// The encoding a browser posts a form in and the one the API answers in: key=value pairs joined by
// &, with + or %XX standing in for anything a payload cannot carry. It is platform-free, so the
// host suite compiles it; the handlers that use it need the board.
inline int formHexDigit(char digit) {
  if (digit >= '0' && digit <= '9') return digit - '0';
  if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
  if (digit >= 'A' && digit <= 'F') return digit - 'A' + 10;
  return -1;
}

inline String formDecoded(const String& text) {
  String out;
  out.reserve(text.length());
  for (unsigned int i = 0; i < text.length(); i++) {
    const char character = text[i];
    if (character == '+') {
      out += ' ';
      continue;
    }
    if (character == '%' && i + 2 < text.length()) {
      const int high = formHexDigit(text[i + 1]);
      const int low = formHexDigit(text[i + 2]);
      if (high >= 0 && low >= 0) {
        out += (char)(high * 16 + low);
        i += 2;
        continue;
      }
    }
    out += character;
  }
  return out;
}

inline String formEncoded(const String& text) {
  static const char Hex[] = "0123456789ABCDEF";
  String out;
  out.reserve(text.length());
  for (unsigned int i = 0; i < text.length(); i++) {
    const unsigned char character = (unsigned char)text[i];
    if (isalnum(character) || character == '-' || character == '_' || character == '.' ||
        character == '~') {
      out += (char)character;
    } else {
      out += '%';
      out += Hex[character >> 4];
      out += Hex[character & 0x0F];
    }
  }
  return out;
}

// The value of `name` in a form body, decoded; empty when the field is absent or has no value.
inline String formField(const String& body, const char* name) {
  unsigned int at = 0;
  while (at < body.length()) {
    int end = body.indexOf('&', at);
    if (end < 0) end = body.length();
    const int equals = body.indexOf('=', at);
    if (equals >= 0 && equals < end) {
      if (body.substring(at, equals) == name) return formDecoded(body.substring(equals + 1, end));
    }
    at = end + 1;
  }
  return String();
}

}  // namespace gardener::runtime
