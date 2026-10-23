#pragma once

#include <FS.h>

#include "storage/SettingsSource.h"

namespace gardener {

// A SettingsSource on the filesystem the composition root mounted: it opens the path it is handed
// and carries no layout or validation of its own. The callers build the paths — Settings writes the
// device's document at kDevicePath, Routines builds each routine's document — so safety is theirs.
class FileSettings : public SettingsSource {
 public:
  explicit FileSettings(FS& fs) : fs_(fs) {}

  bool read(const String& path, String& out) override;
  bool write(const String& path, const String& text) override;

 private:
  FS& fs_;
};

}  // namespace gardener
