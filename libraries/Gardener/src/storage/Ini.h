#pragma once

#include <Arduino.h>

#include <utility>
#include <vector>

namespace gardener {

// A small INI document: `[section]` headers, `key=value` lines, blank lines and `;` comments.
//
// A section's keys are held together in the order they were read, and the sections are held in the
// order they first appear. Keys before the first header belong to the empty section, which is
// written without a header. Comments and spacing are not kept: what comes back is the same keys in
// the same order, formatted plainly.
class Ini {
 public:
  using KeyValue = std::pair<String, String>;
  using Section = std::pair<String, std::vector<KeyValue>>;

  // Replaces whatever was held before. Malformed lines are ignored: a settings file is not worth
  // refusing to boot over.
  void parse(const String& text);

  // The value for a section/key, or `fallback` when it is not there.
  String get(const char* section, const char* key, const char* fallback) const;

  // Whether a section/key is there at all, which a get cannot say: an empty value and a missing key
  // both come back as the fallback.
  bool has(const char* section, const char* key) const { return find(section, key) != nullptr; }

  // Sets a key, adding its section and the key when they are new.
  void set(const char* section, const char* key, const String& value);

  // The document as text, ready to write.
  String text() const;

  // The names of the sections that hold at least one key, in the order they appear. The global
  // section, when it holds a key, is the empty name.
  void sectionKeys(std::vector<String>& keys) const;

 protected:
  // The document's sections in the order they appear, for a subclass that reads its own shape.
  const std::vector<Section>& sections() const { return sections_; }

  Section* findSection(const char* section);
  const Section* findSection(const char* section) const;

  KeyValue* find(const char* section, const char* key);
  const KeyValue* find(const char* section, const char* key) const;

  std::vector<Section> sections_;
};

}  // namespace gardener
