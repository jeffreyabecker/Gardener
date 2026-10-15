#include "ProgramStore.h"

#include <LittleFS.h>

#include "Log.h"
#include "Util.h"

namespace eggplan {
namespace {

const char kDefaultHoldProgram[] = R"BERRY(# hold_vacuum.be - pull down to a target and hold it there.
#
# Runs on the Eggplan Vacuum Berry VM. Available module: vacuum
# (see the README for the full API surface).

var target = 40.0     # kPa absolute

vacuum.log("hold_vacuum: target " + vacuum.fmt(target, 1) + " kPa")

while true
    var p = vacuum.pressure()
    vacuum.display_line(0, "hold " + vacuum.fmt(target, 1) + " kPa")
    vacuum.display_line(1, "now  " + vacuum.fmt(p, 1) + " kPa")

    if p > target + 2.0
        vacuum.pump(70)
    elif p < target - 2.0
        vacuum.pump(0)
    end
    vacuum.delay(100)
end
)BERRY";

const char kDefaultPulseProgram[] = R"BERRY(# pulse.be - cycle the pump on and off while logging pressure.

vacuum.pump(0)
for cycle : 1 .. 10
    vacuum.log("cycle " + str(cycle))
    vacuum.pump(80)
    vacuum.delay(3000)
    vacuum.pump(0)
    vacuum.delay(3000)
    vacuum.log("  pressure " + vacuum.fmt(vacuum.pressure(), 1) + " kPa")
end
vacuum.log("pulse: done")
)BERRY";

}  // namespace

bool ProgramStore::validName(const String& name) {
  if (name.length() == 0 || name.length() > cfg::PROGRAM_NAME_MAX) return false;
  for (size_t i = 0; i < name.length(); i++) {
    char c = name[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

String ProgramStore::pathFor(const String& name) {
  return String(cfg::PROGRAM_DIR) + "/" + name + ".be";
}

bool ProgramStore::begin() {
  if (!LittleFS.begin(true)) {
    LOGE("store: LittleFS mount failed");
    return false;
  }
  if (!LittleFS.exists(cfg::PROGRAM_DIR)) {
    LittleFS.mkdir(cfg::PROGRAM_DIR);
  }
  if (LittleFS.exists(cfg::PROGRAM_TMP)) {
    LittleFS.remove(cfg::PROGRAM_TMP);
  }
  refresh();
  LOGI("store: %u programs, %u/%u bytes used", (unsigned)count_, (unsigned)usedBytes(),
       (unsigned)totalBytes());
  if (count_ == 0) installDefaults();
  return true;
}

void ProgramStore::writeDefault(const char* name, const char* text) {
  fs::File f = LittleFS.open(pathFor(String(name)), FILE_WRITE);
  if (!f) {
    LOGE("store: cannot create default program %s", name);
    return;
  }
  f.print(text);
  f.close();
  LOGI("store: installed default program %s", name);
}

void ProgramStore::installDefaults() {
  writeDefault("hold_vacuum", kDefaultHoldProgram);
  writeDefault("pulse", kDefaultPulseProgram);
  refresh();
}

void ProgramStore::refresh() {
  count_ = 0;
  fs::File dir = LittleFS.open(cfg::PROGRAM_DIR);
  if (!dir || !dir.isDirectory()) {
    LOGW("store: %s is not a directory", cfg::PROGRAM_DIR);
    return;
  }
  fs::File entry = dir.openNextFile();
  while (entry && count_ < cfg::PROGRAM_MAX_COUNT) {
    String raw = entry.name();
    uint32_t size = entry.size();
    entry.close();
    String base = raw;
    int slash = base.lastIndexOf('/');
    if (slash >= 0) base = base.substring(slash + 1);
    if (base.startsWith(".")) {
      entry = dir.openNextFile();
      continue;
    }
    if (base.endsWith(".be")) base = base.substring(0, base.length() - 3);

    ProgramInfo info;
    info.name = base;
    info.size = size;
    String contents;
    if (read(base, contents)) {
      info.crc = crc32(contents);
    }
    info.updated = 0;
    programs_[count_++] = info;
    entry = dir.openNextFile();
  }
  dir.close();

  // Deterministic order for the local UI.
  for (size_t i = 1; i < count_; i++) {
    ProgramInfo key = programs_[i];
    int j = (int)i - 1;
    while (j >= 0 && programs_[j].name.compareTo(key.name) > 0) {
      programs_[j + 1] = programs_[j];
      j--;
    }
    programs_[j + 1] = key;
  }
}

const ProgramInfo* ProgramStore::at(size_t index) const {
  if (index >= count_) return nullptr;
  return &programs_[index];
}

int ProgramStore::indexOf(const String& name) const {
  for (size_t i = 0; i < count_; i++) {
    if (programs_[i].name == name) return (int)i;
  }
  return -1;
}

bool ProgramStore::exists(const String& name) const {
  return validName(name) && LittleFS.exists(pathFor(name));
}

bool ProgramStore::read(const String& name, String& out) const {
  if (!validName(name)) return false;
  fs::File f = LittleFS.open(pathFor(name), FILE_READ);
  if (!f) return false;
  out = f.readString();
  f.close();
  return true;
}

bool ProgramStore::remove(const String& name) {
  if (!validName(name)) return false;
  String path = pathFor(name);
  if (!LittleFS.exists(path)) return false;
  bool ok = LittleFS.remove(path);
  if (ok) {
    LOGI("store: removed %s", name.c_str());
    if (autostart() == name) setAutostart("");
    refresh();
  }
  return ok;
}

String ProgramStore::autostart() const {
  if (!LittleFS.exists(cfg::AUTOSTART_FILE)) return String();
  fs::File f = LittleFS.open(cfg::AUTOSTART_FILE, FILE_READ);
  if (!f) return String();
  String name = f.readString();
  f.close();
  name.trim();
  return name;
}

bool ProgramStore::setAutostart(const String& name) {
  if (name.length() == 0) {
    if (LittleFS.exists(cfg::AUTOSTART_FILE)) LittleFS.remove(cfg::AUTOSTART_FILE);
    LOGI("store: autostart cleared");
    return true;
  }
  if (!validName(name) || !exists(name)) return false;
  fs::File f = LittleFS.open(cfg::AUTOSTART_FILE, FILE_WRITE);
  if (!f) return false;
  f.print(name);
  f.close();
  LOGI("store: autostart set to %s", name.c_str());
  return true;
}

bool ProgramStore::writeBegin(const String& name) {
  if (writing_ || !validName(name)) return false;
  writeAbort();
  file_ = LittleFS.open(cfg::PROGRAM_TMP, FILE_WRITE);
  if (!file_) {
    LOGE("store: cannot open upload staging file");
    return false;
  }
  writing_ = true;
  writingName_ = name;
  writeBytes_ = 0;
  writeCrc_ = 0xFFFFFFFFu;
  return true;
}

bool ProgramStore::writeAppend(const uint8_t* data, size_t len) {
  if (!writing_ || !file_) return false;
  if (writeBytes_ + len > cfg::PROGRAM_MAX_BYTES) {
    writeAbort();
    return false;
  }
  size_t written = file_.write(data, len);
  if (written != len) {
    writeAbort();
    return false;
  }
  if (writeBytes_ == 0) {
    // Strip a UTF-8 BOM so uploaded files behave identically to defaults.
    if (len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
      writeBytes_ += len;
      writeCrc_ = crc32Update(writeCrc_, data + 3, len - 3);
      return true;
    }
  }
  writeCrc_ = crc32Update(writeCrc_, data, len);
  writeBytes_ += len;
  return true;
}

bool ProgramStore::writeCommit(uint32_t expectedCrc, uint32_t expectedSize, String& error) {
  if (!writing_ || !file_) {
    error = "no upload in progress";
    return false;
  }
  file_.close();
  uint32_t crc = writeCrc_ ^ 0xFFFFFFFFu;
  uint32_t size = writeBytes_;
  String name = writingName_;
  writing_ = false;
  writingName_ = "";

  if (size == 0) {
    LittleFS.remove(cfg::PROGRAM_TMP);
    error = "empty program";
    return false;
  }
  if (expectedSize != 0 && expectedSize != size) {
    LittleFS.remove(cfg::PROGRAM_TMP);
    error = "size mismatch";
    return false;
  }
  if (expectedCrc != 0 && expectedCrc != crc) {
    LittleFS.remove(cfg::PROGRAM_TMP);
    error = "crc mismatch";
    return false;
  }

  String target = pathFor(name);
  if (LittleFS.exists(target)) LittleFS.remove(target);
  if (!LittleFS.rename(cfg::PROGRAM_TMP, target)) {
    LittleFS.remove(cfg::PROGRAM_TMP);
    error = "rename failed";
    return false;
  }
  refresh();
  LOGI("store: stored %s (%u bytes)", name.c_str(), (unsigned)size);
  return true;
}

bool ProgramStore::writeAbort() {
  if (file_) file_.close();
  bool wasWriting = writing_;
  writing_ = false;
  writingName_ = "";
  writeBytes_ = 0;
  writeCrc_ = 0xFFFFFFFFu;
  if (wasWriting && LittleFS.exists(cfg::PROGRAM_TMP)) {
    LittleFS.remove(cfg::PROGRAM_TMP);
  }
  return wasWriting;
}

size_t ProgramStore::usedBytes() const { return LittleFS.usedBytes(); }
size_t ProgramStore::totalBytes() const { return LittleFS.totalBytes(); }

}  // namespace eggplan
