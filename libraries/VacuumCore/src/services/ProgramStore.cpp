#include "services/ProgramStore.h"

#include <FS.h>

#include "core/Log.h"
#include "hal/Storage.h"

namespace eggplan {
namespace {

// Program store internals. Keeping the layout, the staging file and the autostart
// marker private is what lets the store change its mind without touching a caller.
constexpr const char* kProgramDir = "/programs";
constexpr const char* kStagingPath = "/programs/.upload.tmp";
constexpr const char* kAutostartPath = "/programs/.autostart";
constexpr const char* kProgramSuffix = ".be";

const char kDefaultHoldProgram[] = R"BERRY(# hold_vacuum.be - pull down to a target and hold it there.
#
# Runs on the Eggplan Vacuum Berry VM. Available module: vacuum
# (see the README for the full API surface).
#
# At sea level the pump reaches about 101.3 - 60 = 41.3 kPa absolute, so targets
# below that just run it at the duty cap.

var target = 50.0     # kPa absolute

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

// Standard CRC-32, private to this file: callers pass a ProgramChecksum around
// without ever needing the polynomial.
uint32_t ProgramStore::checksumUpdate(uint32_t crc, const uint8_t* data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++) {
      crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1u)));
    }
  }
  return crc;
}

uint32_t ProgramStore::checksum(const uint8_t* data, size_t length) {
  return checksumUpdate(0xFFFFFFFFu, data, length) ^ 0xFFFFFFFFu;
}

bool ProgramStore::validName(const char* name) {
  if (!name) return false;
  size_t length = strlen(name);
  if (length == 0 || length > kNameMax) return false;
  for (size_t i = 0; i < length; i++) {
    char c = name[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

String ProgramStore::pathFor(const char* name) {
  return String(kProgramDir) + "/" + name + kProgramSuffix;
}

bool ProgramStore::begin() {
  if (!storage_.exists(kProgramDir)) storage_.mkdir(kProgramDir);
  if (storage_.exists(kStagingPath)) storage_.remove(kStagingPath);
  refresh();
  LOGI("store: %u programs, %u/%u bytes used", (unsigned)count_,
       (unsigned)storage_.usedBytes(), (unsigned)storage_.totalBytes());
  if (count_ == 0) installDefaults();
  return true;
}

void ProgramStore::writeDefault(const char* name, const char* text, Storage& storage) {
  fs::File file = storage.open(pathFor(name).c_str(), FILE_WRITE);
  if (!file) {
    LOGE("store: cannot create default program %s", name);
    return;
  }
  file.print(text);
  file.close();
  LOGI("store: installed default program %s", name);
}

void ProgramStore::installDefaults() {
  writeDefault("hold_vacuum", kDefaultHoldProgram, storage_);
  writeDefault("pulse", kDefaultPulseProgram, storage_);
  refresh();
}

void ProgramStore::refresh() {
  count_ = 0;
  autostartName_[0] = '\0';

  String names[kMaxCount];
  size_t found = storage_.listDirectory(kProgramDir, names, kMaxCount);

  for (size_t i = 0; i < found && count_ < kMaxCount; i++) {
    String base = names[i];
    int slash = base.lastIndexOf('/');
    if (slash >= 0) base = base.substring(slash + 1);
    if (base.startsWith(".")) continue;
    if (base.endsWith(kProgramSuffix)) base = base.substring(0, base.length() - 3);
    if (!validName(base.c_str())) continue;

    ProgramInfo info;
    strncpy(info.name, base.c_str(), kNameMax);
    info.name[kNameMax] = '\0';
    uint32_t size = 0;
    if (storage_.fileSize(pathFor(base.c_str()).c_str(), size)) info.size = size;

    String contents;
    if (read(base.c_str(), contents)) {
      info.checksum = checksum((const uint8_t*)contents.c_str(), contents.length());
    }
    programs_[count_++] = info;
  }

  // Deterministic order for the local UI.
  for (size_t i = 1; i < count_; i++) {
    ProgramInfo key = programs_[i];
    int j = (int)i - 1;
    while (j >= 0 && strcmp(programs_[j].name, key.name) > 0) {
      programs_[j + 1] = programs_[j];
      j--;
    }
    programs_[j + 1] = key;
  }

  // Autostart marker.
  if (storage_.exists(kAutostartPath)) {
    fs::File file = storage_.open(kAutostartPath, FILE_READ);
    if (file) {
      String name = file.readString();
      file.close();
      name.trim();
      if (validName(name.c_str())) {
        strncpy(autostartName_, name.c_str(), kNameMax);
        autostartName_[kNameMax] = '\0';
        for (size_t i = 0; i < count_; i++) {
          programs_[i].autostart = strcmp(programs_[i].name, autostartName_) == 0;
        }
      }
    }
  }
}

const ProgramInfo* ProgramStore::at(size_t index) const {
  return index < count_ ? &programs_[index] : nullptr;
}

int ProgramStore::indexOf(const char* name) const {
  if (!name) return -1;
  for (size_t i = 0; i < count_; i++) {
    if (strcmp(programs_[i].name, name) == 0) return (int)i;
  }
  return -1;
}

bool ProgramStore::exists(const char* name) const {
  return validName(name) && storage_.exists(pathFor(name).c_str());
}

bool ProgramStore::read(const char* name, String& out) const {
  if (!validName(name)) return false;
  fs::File file = storage_.open(pathFor(name).c_str(), FILE_READ);
  if (!file) return false;
  out = file.readString();
  file.close();
  return true;
}

bool ProgramStore::remove(const char* name) {
  if (!validName(name)) return false;
  String path = pathFor(name);
  if (!storage_.exists(path.c_str())) return false;
  if (!storage_.remove(path.c_str())) return false;
  LOGI("store: removed %s", name);
  bool wasAutostart = strcmp(autostartName_, name) == 0;
  refresh();
  if (wasAutostart) setAutostart("");
  return true;
}

bool ProgramStore::setAutostart(const char* name) {
  if (!name || name[0] == '\0') {
    if (storage_.exists(kAutostartPath)) storage_.remove(kAutostartPath);
    autostartName_[0] = '\0';
    for (size_t i = 0; i < count_; i++) programs_[i].autostart = false;
    LOGI("store: autostart cleared");
    return true;
  }
  if (!validName(name) || !exists(name)) return false;
  fs::File file = storage_.open(kAutostartPath, FILE_WRITE);
  if (!file) return false;
  file.print(name);
  file.close();
  LOGI("store: autostart set to %s", name);
  refresh();
  return true;
}

bool ProgramStore::writeBegin(const char* name) {
  if (writing_ || !validName(name)) return false;
  writeAbort();
  file_ = storage_.open(kStagingPath, FILE_WRITE);
  if (!file_) {
    LOGE("store: cannot open upload staging file");
    return false;
  }
  writing_ = true;
  writingName_ = name;
  writeBytes_ = 0;
  writeChecksum_ = 0xFFFFFFFFu;
  return true;
}

bool ProgramStore::writeAppend(const uint8_t* data, size_t length) {
  if (!writing_ || !file_) return false;
  if (writeBytes_ + length > kMaxBytes) return false;
  if (file_.write(data, length) != length) return false;

  // A leading UTF-8 BOM is stripped so uploaded files behave like the defaults.
  if (writeBytes_ == 0 && length >= 3 && data[0] == 0xEF && data[1] == 0xBB &&
      data[2] == 0xBF) {
    writeChecksum_ = checksumUpdate(writeChecksum_, data + 3, length - 3);
  } else {
    writeChecksum_ = checksumUpdate(writeChecksum_, data, length);
  }
  writeBytes_ += length;
  return true;
}

bool ProgramStore::writeCommit(ProgramChecksum expectedChecksum, uint32_t expectedSize,
                               String& error) {
  if (!writing_ || !file_) {
    error = "no upload in progress";
    return false;
  }
  file_.close();

  uint32_t computed = writeChecksum_ ^ 0xFFFFFFFFu;
  uint32_t size = writeBytes_;
  String name = writingName_;
  writing_ = false;
  writingName_ = "";

  if (size == 0) {
    if (storage_.exists(kStagingPath)) storage_.remove(kStagingPath);
    error = "empty program";
    return false;
  }
  if (expectedSize != 0 && expectedSize != size) {
    if (storage_.exists(kStagingPath)) storage_.remove(kStagingPath);
    error = "size mismatch";
    return false;
  }
  if (expectedChecksum != 0 && expectedChecksum != computed) {
    if (storage_.exists(kStagingPath)) storage_.remove(kStagingPath);
    error = "checksum mismatch";
    return false;
  }

  String target = pathFor(name.c_str());
  if (storage_.exists(target.c_str())) storage_.remove(target.c_str());
  if (!storage_.rename(kStagingPath, target.c_str())) {
    if (storage_.exists(kStagingPath)) storage_.remove(kStagingPath);
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
  writeChecksum_ = 0xFFFFFFFFu;
  if (wasWriting && storage_.exists(kStagingPath)) storage_.remove(kStagingPath);
  return wasWriting;
}

uint32_t ProgramStore::usedKb() const { return storage_.usedBytes() / 1024; }

uint32_t ProgramStore::totalKb() const { return storage_.totalBytes() / 1024; }

}  // namespace eggplan
