#include "storage/Config.h"

namespace gardener {

Config::Config(Library* store, const char* path) : store_(store), path_(path) {}

bool Config::save() const {
  if (store_ == nullptr) return false;
  return store_->write(path_, text());
}

// Reading is best effort: a document that is not there or does not parse leaves the defaults in
// place. Nothing about a boot depends on the answer.
bool Config::begin() {
  bool read = false;
  if (store_ != nullptr) {
    String text;
    if (store_->read(path_, text)) {
      parse(text);
      read = true;
    }
  }

  load();
  return read;
}

}  // namespace gardener
