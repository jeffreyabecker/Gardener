#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

namespace gardener {

// One domain of the serial surface. The dispatcher reads only the domain token, then hands it the
// stream at that point. `io` is the port: the domain reads the rest of its command from it and
// writes its reply back to it.
class Domain {
 public:
  virtual ~Domain() {}
  virtual const char* name() const = 0;
  virtual void execute(Stream& io) = 0;
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

// --- reply items -----------------------------------------------------------
// A reply value is one token: it reaches the wire without quoting, so no domain may put a space
// in one.
inline void addKey(String& out, const char* key) {
  if (out.length() > 0) out += ' ';
  out += key;
  out += '=';
}

inline void addValue(String& out, const char* value) { out += value ? value : "-"; }

inline void addItem(String& out, const char* key, const char* value) {
  addKey(out, key);
  addValue(out, value);
}

inline void addItemU32(String& out, const char* key, uint32_t value) {
  addKey(out, key);
  out += String(value);
}

inline void addItemFloat(String& out, const char* key, float value, uint8_t decimals) {
  addKey(out, key);
  // The cast picks String(float, decimals) out of the overload set.
  out += String(value, (unsigned int)decimals);
}

inline void addItemFlag(String& out, const char* key, bool value) {
  addItem(out, key, value ? "on" : "off");
}

// --- reply text ------------------------------------------------------------
inline void replyLine(Stream& out, const String& line) {
  out.print(line);
  out.write('\n');
}

inline void replyOk(Stream& out, const String& items) {
  String line = "ok";
  if (!items.isEmpty()) {
    line += ' ';
    line += items;
  }
  replyLine(out, line);
}

inline void replyErr(Stream& out, const char* code, const String& detail) {
  String line = "err ";
  line += code;
  if (!detail.isEmpty()) {
    line += ' ';
    line += detail;
  }
  replyLine(out, line);
}

inline void replyBadArgument(Stream& out, const char* token) {
  replyErr(out, "bad_argument", token ? token : "");
}

inline void replyUnknownVerb(Stream& out, const char* verb) {
  replyErr(out, "unknown_verb", verb);
}

inline void replyOutOfRange(Stream& out, const char* field, const char* value, const float* min,
                            const float* max) {
  String detail;
  addItem(detail, "field", field);
  addItem(detail, "value", value);
  if (min) addItemFloat(detail, "min", *min, 1);
  if (max) addItemFloat(detail, "max", *max, 1);
  replyErr(out, "out_of_range", detail);
}

inline void replyNotReady(Stream& out, const char* reason) {
  replyErr(out, "not_ready", reason);
}

}  // namespace gardener
