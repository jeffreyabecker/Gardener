#pragma once

#include <any>
#include <map>

#include <Arduino.h>

namespace gardener {

// The values a settings list holds, keyed by setting key: what an editor wrote back and what OK
// hands on.
using SettingValues = std::map<String, std::any>;

// The half of a settings row a document asks for: the key its value is stored under, the default its
// declaration gives, and the text written and read back. A row answers the rest of itself -- the
// declaration it was built from, the label it prints, the editor that edits it, the check it
// carries -- and that half is the page's, which is what lets the store below the pages be written
// against this alone.
class SettingKind {
 public:
  virtual ~SettingKind() = default;

  virtual const String& key() const = 0;
  virtual std::any defaultValue() const = 0;
  virtual String stored(const std::any& value) const = 0;
  virtual std::any parse(const String& stored) const = 0;
};

}  // namespace gardener