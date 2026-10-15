#pragma once

#include <Arduino.h>

#include "core/AppContext.h"
#include "services/ProgramStore.h"

namespace eggplan {

// Opcodes for the program transfer characteristic: exactly one opcode per
// characteristic write.
//   'B' <name>              begin, name is [A-Za-z0-9_-]{1,31}
//   'D' <raw bytes>         append chunk
//   'E' <crc32> <size>      commit, both little endian uint32 (0 = "any")
//   'X'                     abort
namespace progproto {
constexpr uint8_t kBegin = 'B';
constexpr uint8_t kData = 'D';
constexpr uint8_t kEnd = 'E';
constexpr uint8_t kAbort = 'X';
constexpr size_t kMaxFrame = 512;
}  // namespace progproto

// Where command replies go. The BLE service implements this and is free to
// split a single emit() across several notifications.
class ResponseSink {
 public:
  virtual ~ResponseSink() {}
  virtual void emit(const String& text) = 0;
};

class ProgramUploader {
 public:
  explicit ProgramUploader(ProgramStore* store = nullptr) : store_(store) {}
  void setStore(ProgramStore* store) { store_ = store; }
  bool active() const { return store_ && store_->writing(); }
  String handle(const uint8_t* data, size_t len);
  void abort(const char* reason);
  uint32_t chunks() const { return chunks_; }
  uint32_t bytes() const { return store_ ? store_->writeBytes() : 0; }

 private:
  ProgramStore* store_;
  uint32_t chunks_ = 0;
};

// Line oriented command interface, shared by BLE, USB serial and the scripts.
class CommandRouter {
 public:
  explicit CommandRouter(AppContext& ctx) : ctx_(ctx) {}
  void handleLine(const String& line, ResponseSink& out);
  String statusJson() const;
  String infoJson() const;

 private:
  void emitHelp(ResponseSink& out);
  void emitList(ResponseSink& out);
  void emitCat(const String& name, uint32_t offset, uint32_t length, ResponseSink& out);
  void emitLog(int lines, ResponseSink& out);
  void cmdRun(const String& name, const String& args, ResponseSink& out);

  AppContext& ctx_;
};

}  // namespace eggplan
