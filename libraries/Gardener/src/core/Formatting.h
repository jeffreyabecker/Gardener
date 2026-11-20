#pragma once

#include <Arduino.h>
#include <utility>

namespace gardener {

// A value written the way the wire reads a token back: bare while it is one, and inside quotes with
// the characters that would end or escape the quote spelled out when it is not. The rules are the
// ones the network's names already follow, so a value the wire prints is one it takes again.
class Quote : public Printable {
 public:
  explicit Quote(const String& text) : text_(text) {}

  size_t printTo(Print& io) const override;

 private:
  String text_;
};

inline size_t Quote::printTo(Print& io) const {
  bool bare = !text_.isEmpty();
  for (unsigned int i = 0; bare && i < text_.length(); i++) {
    const char character = text_[i];
    if (character == ' ' || character == '"' || character == '\\') bare = false;
  }
  if (bare) return io.print(text_);

  size_t written = io.print('"');
  for (unsigned int i = 0; i < text_.length(); i++) {
    const char character = text_[i];
    if (character == '"' || character == '\\') written += io.print('\\');
    written += io.print(character);
  }
  return written + io.print('"');
}

inline Quote quote(const String& text) { return Quote(text); }
inline Quote quote(const char* text) { return Quote(text ? text : ""); }

// One part of a line: anything the Print API takes goes as it is. A pair is a value and the
// decimals it is written to, so a number's places are stated where it is written rather than left
// to Print's default.
template <typename Value> inline void writePart(Print& io, const Value& value) { io.print(value); }

template <typename Value, typename Decimals>
inline void writePart(Print& io, const std::pair<Value, Decimals>& value) {
  io.print(value.first, static_cast<int>(value.second));
}

// A run of parts in the order it is given, with nothing added.
template <typename... Parts> inline void print(Print& io, const Parts&... parts) {
  (writePart(io, parts), ...);
}

// A reply line: every part in the order it is given, then the LF that ends it.
template <typename... Parts> inline void println(Print& io, const Parts&... parts) {
  print(io, parts...);
  io.print('\n');
}

// A Print that collects what is written into a String, for a sink that takes text rather than a
// stream.
class StringPrint : public Print {
 public:
  size_t write(uint8_t byte) override {
    text_.concat((char)byte);
    return 1;
  }

  size_t write(const uint8_t* buffer, size_t size) override {
    text_.concat((const char*)buffer, (unsigned int)size);
    return size;
  }

  const String& text() const { return text_; }

 private:
  String text_;
};

// The parts as one string, written the way print writes them.
template <typename... Parts> String format(const Parts&... parts) {
  StringPrint out;
  print(out, parts...);
  return out.text();
}

}  // namespace gardener
