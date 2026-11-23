#pragma once

#include <Arduino.h>
#include <utility>

namespace gardener {

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
