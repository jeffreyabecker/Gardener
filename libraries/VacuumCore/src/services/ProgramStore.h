#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "core/Snapshot.h"

#include <FS.h>

namespace eggplan {

class Storage;

// Script programs live in LittleFS as /programs/<name>.be.
//
// Writes are staged into a temporary file and renamed into place only when the client
// commits a chunked upload, so an interrupted transfer can never leave a half written
// program behind. The on disk layout, the name rules and the checksum are all this
// class's business; nothing outside needs to know any of them.
class ProgramStore {
 public:
  static constexpr size_t kNameMax = 31;
  static constexpr size_t kNameBufferSize = 32;
  static constexpr uint32_t kMaxBytes = 64 * 1024;
  static constexpr uint8_t kMaxCount = 24;

  explicit ProgramStore(Storage& storage) : storage_(storage) {}

  bool begin();
  void refresh();

  size_t count() const { return count_; }
  const ProgramInfo* list() const { return programs_; }
  const ProgramInfo* at(size_t index) const;
  int indexOf(const char* name) const;
  bool exists(const char* name) const;
  bool read(const char* name, String& out) const;
  bool remove(const char* name);

  // Empty string when no program is marked for boot.
  const char* autostartName() const { return autostartName_; }
  bool setAutostart(const char* name);

  bool writeBegin(const char* name);
  bool writeAppend(const uint8_t* data, size_t length);
  bool writeCommit(ProgramChecksum expectedChecksum, uint32_t expectedSize, String& error);
  bool writeAbort();
  bool writing() const { return writing_; }
  const String& writingName() const { return writingName_; }
  uint32_t writeBytes() const { return writeBytes_; }

  uint32_t usedKb() const;
  uint32_t totalKb() const;

  // Names are restricted to [A-Za-z0-9_-] so the on disk layout stays predictable and
  // the dot files used for staging can never be addressed.
  static bool validName(const char* name);

 private:
  static String pathFor(const char* name);
  static uint32_t checksum(const uint8_t* data, size_t length);
  static uint32_t checksumUpdate(uint32_t checksum, const uint8_t* data, size_t length);
  static void writeDefault(const char* name, const char* text, Storage& storage);
  void installDefaults();

  Storage& storage_;
  fs::File file_;
  bool writing_ = false;
  String writingName_;
  uint32_t writeBytes_ = 0;
  uint32_t writeChecksum_ = 0xFFFFFFFFu;
  ProgramInfo programs_[kMaxCount];
  size_t count_ = 0;
  char autostartName_[kNameBufferSize] = {0};
};

}  // namespace eggplan
