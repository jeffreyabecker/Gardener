#pragma once

#include <Arduino.h>
#include <FS.h>

#include "VacuumConfig.h"

namespace eggplan {

struct ProgramInfo {
  String name;
  uint32_t size = 0;
  uint32_t crc = 0;
  uint32_t updated = 0;
};

// Script programs live in LittleFS as /programs/<name>.be.
//
// Writes are staged into a temporary file and only renamed into place once the
// client commits a chunked upload, so an interrupted transfer can never leave a
// half written program behind.
class ProgramStore {
 public:
  bool begin();
  void refresh();

  size_t count() const { return count_; }
  const ProgramInfo* at(size_t index) const;
  const ProgramInfo* list() const { return programs_; }
  int indexOf(const String& name) const;
  bool exists(const String& name) const;
  bool read(const String& name, String& out) const;
  bool remove(const String& name);

  String autostart() const;
  bool setAutostart(const String& name);

  bool writeBegin(const String& name);
  bool writeAppend(const uint8_t* data, size_t len);
  bool writeCommit(uint32_t expectedCrc, uint32_t expectedSize, String& error);
  bool writeAbort();
  bool writing() const { return writing_; }
  const String& writingName() const { return writingName_; }
  uint32_t writeBytes() const { return writeBytes_; }
  uint32_t writeCrc() const { return writeCrc_ ^ 0xFFFFFFFFu; }

  size_t usedBytes() const;
  size_t totalBytes() const;

  // Names are restricted to [A-Za-z0-9_-] so that the on disk layout stays
  // predictable and the dot files used for staging can never be addressed.
  static bool validName(const String& name);
  static String pathFor(const String& name);

 private:
  void installDefaults();
  static void writeDefault(const char* name, const char* text);

  fs::File file_;
  bool writing_ = false;
  String writingName_;
  uint32_t writeBytes_ = 0;
  uint32_t writeCrc_ = 0xFFFFFFFFu;
  ProgramInfo programs_[cfg::PROGRAM_MAX_COUNT];
  size_t count_ = 0;
};

}  // namespace eggplan
