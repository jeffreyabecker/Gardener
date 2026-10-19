#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

namespace gardener {

class AppContext;

// What every domain is handed at construction. The command stream is an argument to execute(),
// not part of this: the domain reads and tokenizes everything after its own name. `out` is the
// reply side of the port, so a domain formats its own reply rather than going through the link.
struct DomainContext {
  AppContext& app;
  Stream& out;
  bool& logEnabled;
};

// One domain of the serial surface. The dispatcher reads only the domain token, then hands the
// stream to execute(); the domain owns its verbs, arguments, help and any body.
class Domain {
 public:
  virtual ~Domain() {}
  virtual const char* name() const = 0;
  virtual void execute(Stream& input) = 0;
  // One row on the top-level help page.
  virtual void describe() = 0;
  // The topic list when `topic` is null, otherwise that topic's page.
  virtual void help(const char* topic) = 0;
};

// One help topic, as the domain's own help text describes it.
struct TopicSpec {
  const char* name;
  const char* args;
  const char* about;
};

// The domains in help order. The dispatcher and HelpDomain read it; SerialLink owns it.
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

  uint8_t count() const { return count_; }
  Domain* at(uint8_t index) const { return index < count_ ? items_[index] : nullptr; }

 private:
  Domain* items_[kMax] = {};
  uint8_t count_ = 0;
};

// --- reply items -----------------------------------------------------------
// The one place a value reaches the wire. A value containing a space or quote is quoted and
// escaped; nothing has to replace spaces with another character.
inline void addKey(String& out, const char* key) {
  if (out.length() > 0) out += ' ';
  out += key;
  out += '=';
}

inline void addItem(String& out, const char* key, const char* value) {
  addKey(out, key);
  if (!value) {
    out += '-';
    return;
  }
  if (!value[0]) {
    out += "\"\"";
    return;
  }

  bool quote = false;
  for (const char* cursor = value; *cursor != '\0'; cursor++) {
    if (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' || *cursor == '\r' ||
        *cursor == '"' || *cursor == '\\') {
      quote = true;
      break;
    }
  }
  if (!quote) {
    out += value;
    return;
  }

  out += '"';
  for (const char* cursor = value; *cursor != '\0'; cursor++) {
    if (*cursor == '"' || *cursor == '\\') out += '\\';
    out += *cursor;
  }
  out += '"';
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
// Opt-in helpers for the domains whose replies are lines of items. A domain that needs a
// different shape (the future fs read, with a raw body) writes to `ctx.out` itself.
inline void replyLine(Stream& out, const String& line) {
  out.print(line);
  out.write('\n');
}

inline void replyOk(Stream& out, const char* domain, const char* verb, const String& items) {
  String line = "ok ";
  line += domain;
  line += ' ';
  line += verb;
  if (!items.isEmpty()) {
    line += ' ';
    line += items;
  }
  replyLine(out, line);
}

inline void replyErr(Stream& out, const char* domain, const char* verb, const char* code,
                     const String& items) {
  String line = "err ";
  line += domain;
  line += ' ';
  line += verb;
  line += ' ';
  line += code;
  if (!items.isEmpty()) {
    line += ' ';
    line += items;
  }
  replyLine(out, line);
}

inline void replyRow(Stream& out, const String& items) { replyLine(out, items); }

inline void replyEnd(Stream& out, const char* domain, const char* verb, const String& items) {
  String line = "end ";
  line += domain;
  line += ' ';
  line += verb;
  if (!items.isEmpty()) {
    line += ' ';
    line += items;
  }
  replyLine(out, line);
}

inline void replyBadArgument(Stream& out, const char* domain, const char* verb,
                             const char* token) {
  if (!token) {
    replyErr(out, domain, verb, "bad_argument", "");
    return;
  }
  String detail;
  addItem(detail, "token", token);
  replyErr(out, domain, verb, "bad_argument", detail);
}

inline void replyUnknownVerb(Stream& out, const char* domain, const char* verb) {
  replyErr(out, domain, verb, "unknown_verb", "");
}

inline void replyOutOfRange(Stream& out, const char* domain, const char* verb, const char* field,
                            const char* value, const float* min, const float* max) {
  String detail;
  addItem(detail, "field", field);
  addItem(detail, "value", value);
  if (min) addItemFloat(detail, "min", *min, 1);
  if (max) addItemFloat(detail, "max", *max, 1);
  replyErr(out, domain, verb, "out_of_range", detail);
}

inline void replyNotReady(Stream& out, const char* domain, const char* verb, const char* reason) {
  String detail;
  addItem(detail, "reason", reason);
  replyErr(out, domain, verb, "not_ready", detail);
}

inline void replyUnknownTopic(Stream& out, const char* domain, const char* topic) {
  String detail;
  addItem(detail, "token", topic);
  replyErr(out, "help", domain, "unknown_topic", detail);
}

// --- read-only fields ------------------------------------------------------
// One field of one domain. Writes are domain verbs, so there is no writable flag here.
struct FieldSpec {
  const char* name;
  const char* type;
  const char* unit;
  const char* choices;
  const float* min;
  const float* max;
  void (*read)(const DomainContext&, String&);
};

inline const FieldSpec* findField(const FieldSpec* fields, uint8_t count, const char* name) {
  if (!name) return nullptr;
  for (uint8_t i = 0; i < count; i++) {
    if (strcasecmp(fields[i].name, name) == 0) return &fields[i];
  }
  return nullptr;
}

inline bool readField(const FieldSpec* fields, uint8_t count, const DomainContext& ctx,
                      const char* name, String& out) {
  const FieldSpec* field = findField(fields, count, name);
  if (!field) return false;
  field->read(ctx, out);
  return true;
}

inline void readFields(const FieldSpec* fields, uint8_t count, const DomainContext& ctx,
                       String& out) {
  for (uint8_t i = 0; i < count; i++) fields[i].read(ctx, out);
}

inline void replyFieldHelp(Stream& out, const FieldSpec* fields, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    const FieldSpec& field = fields[i];
    String items;
    addItem(items, "field", field.name);
    addItem(items, "type", field.type);
    addItem(items, "access", "r");
    if (field.unit) addItem(items, "unit", field.unit);
    if (field.choices) addItem(items, "choices", field.choices);
    if (field.min) addItemFloat(items, "min", *field.min, 1);
    if (field.max) addItemFloat(items, "max", *field.max, 1);
    replyRow(out, items);
  }
}

}  // namespace gardener
