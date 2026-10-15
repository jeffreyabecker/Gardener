#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

namespace eggplan {

class ProgramStore;

// Chunked program transfer session. The frame grammar (B begin, D data, E commit,
// X abort) is file private in the .cpp: BleService forwards bytes and never sees an
// opcode, and a header is a contract so the grammar stays out of it.
class ProgramUploader {
 public:
  explicit ProgramUploader(ProgramStore& store) : store_(store) {}

  bool active() const;
  uint32_t bytes() const;

  // One write to the program characteristic, returns the reply line.
  String handle(const uint8_t* data, size_t length);

  void abort(const char* reason);

 private:
  ProgramStore& store_;
  uint32_t chunks_ = 0;
};

}  // namespace eggplan
