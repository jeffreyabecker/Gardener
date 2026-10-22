#pragma once

#include <Arduino.h>

#include <vector>

namespace gardener {

// The files the interpreter compiles and the library it reads, by name. The implementation knows
// where they are; a routine never does.
class Library {
 public:
  virtual ~Library() = default;

  // The `.be` files in one of the interpreter's folders, "modules" or "routines", in load order.
  virtual void list(const String& folder, std::vector<String>& names) = 0;
  // One file, as "<folder>/<name>".
  virtual bool readFile(const String& name, String& out) = 0;
};

}  // namespace gardener
