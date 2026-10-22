#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

namespace gardener {

// The longest command line a domain serves: the link refuses a longer one, and so does each domain
// handed one. A command body, when a domain defines one, is not bounded here.
constexpr uint16_t COMMAND_LINE_MAX = 128;

class Link;

// One domain of the serial surface. The dispatcher reads only the domain token, then hands it the
// stream at that point. `io` is the port: the domain reads the rest of its command from it and
// writes its reply back to it. `link_` is the link that serves the domain: a state-changing verb is
// refused unless that link is on top of the ownership stack.
class Domain {
 public:
  explicit Domain(Link& link) : link_(link) {}
  virtual ~Domain() {}
  virtual const char* name() const = 0;
  virtual void execute(Stream& io) = 0;

 protected:
  Link& link_;
};

// The domains in dispatch order. SerialLink owns it.
class DomainList {
 public:
  static constexpr uint8_t kMax = 8;

  void add(Domain* domain) {
    if (count_ < kMax) items_[count_++] = domain;
  }

  Domain* find(const char* name) const {
    if (!name) return nullptr;
    for (uint8_t i = 0; i < count_; i++) {
      if (strcmp(items_[i]->name(), name) == 0) return items_[i];
    }
    return nullptr;
  }

 private:
  Domain* items_[kMax] = {};
  uint8_t count_ = 0;
};

}  // namespace gardener
