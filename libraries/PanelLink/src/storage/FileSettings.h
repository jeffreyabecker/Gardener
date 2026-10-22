#pragma once

#include <FS.h>

#include "storage/SettingsSource.h"

namespace gardener {

// The device's settings documents, on the filesystem the composition root mounted: the device's
// own in one file, a routine's in the file named after it.
class FileSettings : public SettingsSource {
 public:
  explicit FileSettings(FS& fs) : fs_(fs) {}

  bool read(const String& name, String& out) override;
  bool write(const String& name, const String& text) override;

 private:
  bool valid(const String& name) const;

  FS& fs_;
};

}  // namespace gardener
