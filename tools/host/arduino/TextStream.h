#pragma once

#include <Arduino.h>

#include <cstddef>
#include <string>

namespace gardener::host {

// The port a case holds: the bytes the case typed are what a domain reads, and what the domain
// printed is what the case reads back. It is the API's Stream, so a domain sees exactly the surface
// the device's core gives it.
class TextStream : public Stream {
 public:
  // Queues text as if a host had typed it. It appends, so a case may type a line in pieces.
  void type(const char* text);
  void type(const std::string& text);

  // What the domain has printed since the last clearPrinted().
  const std::string& printed() const { return printed_; }
  void clearPrinted() { printed_.clear(); }

  int available() override { return static_cast<int>(typed_.size() - readAt_); }
  int read() override;
  int peek() override;
  void flush() override {}
  size_t write(uint8_t byte) override;
  using Print::write;

 private:
  std::string typed_;
  size_t readAt_ = 0;
  std::string printed_;
};

}  // namespace gardener::host
