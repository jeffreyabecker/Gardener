#pragma once

#include <math.h>
#include <stdint.h>

#include <any>
#include <map>
#include <utility>
#include <vector>

#include <Arduino.h>

namespace gardener {
inline namespace storage {

// One entry a choice offers: the key the setting stores and the title it prints.
struct ChoiceItem {
  String key;
  String title;
};

// The values a settings list holds, keyed by setting key: what an editor wrote back and what OK
// hands on.
using SettingValues = std::map<String, std::any>;

// The declaration a row was built from, as a page reads it back off a routine's record: the kind the
// file named, the step and bounds that kind was given and the entries a choice offers. A kind with
// none of them answers NAN and no entries, which a record carries as nil.
struct SettingDeclaration {
  const char* kind = "";
  float step = NAN;
  float min = NAN;
  float max = NAN;
  std::vector<ChoiceItem> entries;
};

// The half of a settings row a document asks for: the key its value is stored under, the default its
// declaration gives, the declaration itself, and the text written and read back. A row answers the
// rest of itself -- the label it prints, the editor that edits it, the check it carries -- and that
// half is the page's, which is what lets the store below the pages be written against this alone.
class SettingKind {
 public:
  virtual ~SettingKind() = default;

  virtual const String& key() const = 0;
  virtual std::any defaultValue() const = 0;
  virtual SettingDeclaration declaration() const = 0;
  virtual String stored(const std::any& value) const = 0;
  virtual std::any parse(const String& stored) const = 0;
};

}  // namespace storage
}  // namespace gardener
