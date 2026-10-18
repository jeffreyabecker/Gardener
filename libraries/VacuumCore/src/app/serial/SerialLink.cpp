#include "app/serial/SerialLink.h"

#include <stdlib.h>
#include <string.h>

#include "app/AppContext.h"
#include "app/serial/Items.h"
#include "app/VacuumSystem.h"

namespace gardener {
namespace {

// `link term` takes a case sensitive marker, with \n, \r, \t and \\ standing for the characters a
// command line cannot carry directly. False for a trailing or unknown escape, never a silent
// backslash.
bool decodeTerminator(const char* token, String& out) {
  for (const char* cursor = token; *cursor != '\0'; cursor++) {
    if (*cursor != '\\') {
      out += *cursor;
      continue;
    }
    switch (*++cursor) {
      case 'n': out += '\n'; break;
      case 'r': out += '\r'; break;
      case 't': out += '\t'; break;
      case '\\': out += '\\'; break;
      default: return false;
    }
  }
  return true;
}

// One item, for an error detail: the common case is a single token or field to name.
String named(const char* key, const char* value) {
  Items detail;
  detail.add(key, value);
  return detail.text();
}

}  // namespace

SerialLink::SerialLink(AppContext* context, Stream& stream, PresenceFn present,
                       bool& logEnabled)
    : context_(context), stream_(stream), present_(present), logEnabled_(logEnabled) {}

void SerialLink::begin() {
  attached_ = controllerAttached();
  buffer_ = "";
  swallowing_ = false;
  terminator_ = cfg::LINE_TERMINATOR;
  customTerminator_ = false;
  // SerialLink's own records go straight to the stream it owns, regardless of `device log`.
  writeLine("[I] ", "serial: interface ready");
}

bool SerialLink::controllerAttached() const { return present_ ? present_() : true; }

bool SerialLink::consumeLinkLost() {
  bool lost = linkLost_;
  linkLost_ = false;
  return lost;
}

// --- bytes -----------------------------------------------------------------

void SerialLink::writeLine(const char* prefix, const char* text) {
  stream_.write((const uint8_t*)prefix, strlen(prefix));
  stream_.write((const uint8_t*)text, strlen(text));
  stream_.write((uint8_t)'\n');
}

void SerialLink::dispatchLine(const String& line) {
  // The grammar is parsed in place, so a line gets a writable copy. It is sized by the line rather
  // than by a fixed maximum, because a custom terminator lets one line be a whole program's source.
  size_t length = line.length();
  char* text = (char*)malloc(length + 1);
  if (!text) {
    err("bad_argument");
    return;
  }
  memcpy(text, line.c_str(), length);
  text[length] = '\0';
  Tokens tokens(text);
  command(tokens);
  free(text);
}

// Splits the buffer on the terminator and dispatches every complete line, in arrival order.
// A '\r' immediately before the terminator is dropped, so a host that ends its lines with "\r\n"
// still works against the default terminator. Whatever follows the last terminator is an
// unterminated line and stays in the buffer for a later poll.
void SerialLink::drainLines() {
  if (terminator_.length() == 0) return;
  const size_t limit = customTerminator_ ? cfg::COMMAND_PAYLOAD_MAX : cfg::COMMAND_LINE_MAX;

  // The rest of an overlong line is thrown away up to its terminator, which may only arrive in a
  // later poll.
  if (swallowing_) {
    int end = buffer_.indexOf(terminator_);
    if (end < 0) {
      buffer_ = "";
      return;
    }
    buffer_.remove(0, (unsigned int)end + terminator_.length());
    swallowing_ = false;
  }

  for (;;) {
    int end = buffer_.indexOf(terminator_);
    if (end < 0) break;
    size_t length = (size_t)end;
    if (length > 0 && buffer_.charAt((unsigned int)(length - 1)) == '\r') length--;
    if (length > limit) {
      // Answer once and drop it rather than truncating into something that parses as a different
      // command.
      err("bad_argument");
    } else if (length > 0) {
      dispatchLine(buffer_.substring(0, (unsigned int)length));
    }
    buffer_.remove(0, (unsigned int)end + terminator_.length());
  }

  // A tail that already fills the whole allowance can never become a valid line: the ceiling is
  // inclusive, so one more byte than it is the first that cannot turn back into a command. Refuse
  // it once and swallow the rest until its terminator arrives.
  if (!swallowing_ && buffer_.length() > limit) {
    err("bad_argument");
    buffer_ = "";
    swallowing_ = true;
  }
}

void SerialLink::poll(uint32_t nowMs) {
  (void)nowMs;

  // Take everything the host has sent this pass. There is no per pass line budget: a burst of
  // commands is read whole and the drain below services every one of them.
  while (stream_.available() > 0) {
    int value = stream_.read();
    if (value < 0) break;
    buffer_ += (char)value;
  }

  drainLines();

  bool attached = controllerAttached();
  if (attached_ && !attached) {
    linkLost_ = true;
    buffer_ = "";
    swallowing_ = false;
    // A new controller starts from newline framing rather than inheriting a marker it may not know
    // about.
    terminator_ = cfg::LINE_TERMINATOR;
    customTerminator_ = false;
    writeLine("[W] ", "serial: controller detached");
  }
  attached_ = attached;
}

// --- replies ---------------------------------------------------------------

void SerialLink::ok(const String& items) { writeLine("ok ", items.c_str()); }

void SerialLink::err(const char* code) { writeLine("err ", code); }

void SerialLink::err(const char* code, const String& items) {
  String payload = code;
  payload += ' ';
  payload += items;
  writeLine("err ", payload.c_str());
}

// An action's reply always names the action first, so a controller reading a log can tell what it
// asked for from what it got.
void SerialLink::okResult(const char* action) {
  Items out;
  out.add("result", action);
  ok(out.text());
}

void SerialLink::errUnknown(const char* token) { err("unknown_command", named("token", token)); }

void SerialLink::errBadArgument(const char* token) {
  // A missing argument has nothing to name, and inventing a name for it would be worse than saying
  // only what went wrong.
  if (!token) {
    err("bad_argument");
    return;
  }
  err("bad_argument", named("token", token));
}

void SerialLink::errUnsupported(const char* field) { err("unsupported", named("field", field)); }

void SerialLink::errOutOfRange(const char* field, const char* value, const float* min,
                               const float* max) {
  Items detail;
  detail.add("field", field);
  detail.add("value", value);
  // Printed from the same constants the check used, so a bound is never in two places.
  if (min) detail.addFloat("min", *min, 1);
  if (max) detail.addFloat("max", *max, 1);
  err("out_of_range", detail.text());
}

// --- the command surface ---------------------------------------------------

void SerialLink::command(Tokens& tokens) {
  char* resource = tokens.next();
  if (!resource) return;  // a blank line is not a command and gets no reply
  if (resource[0] == '#') return;  // a comment, likewise

  lower(resource);
  if (same(resource, "help")) {
    help(tokens);
    return;
  }

  uint8_t count = 0;
  const ResourceSpec* resources = SerialLink::resources(count);
  for (uint8_t i = 0; i < count; i++) {
    if (!same(resource, resources[i].name)) continue;
    resourceCommand(resources[i], tokens);
    return;
  }
  errUnknown(resource);
}

void SerialLink::resourceCommand(const ResourceSpec& resource, Tokens& tokens) {
  char* op = tokens.next();
  if (!op) {
    errBadArgument(nullptr);
    return;
  }

  // The field name is folded; the value is not. `link term` is why: it carries a marker, and a
  // marker that came back lowercased would not be the marker that was set.
  char* value = splitValue(op);
  lower(op);

  if (!value) {
    if (same(op, "get")) {
      if (tokens.rest() != nullptr) {
        errBadArgument(tokens.rest());
        return;
      }
      Items out;
      renderFields(resource, out);
      ok(out.text());
      return;
    }

    for (uint8_t i = 0; i < resource.actionCount; i++) {
      const ActionSpec& action = resource.actions[i];
      if (!same(op, action.name)) continue;
      // An action takes no argument, so anything after it is refused rather than ignored.
      char* rest = tokens.rest();
      if (rest != nullptr) {
        errBadArgument(rest);
        return;
      }
      (this->*action.run)();
      return;
    }

    const FieldSpec* field = findField(resource, op);
    if (!field) {
      errUnknown(op);
      return;
    }
    if (tokens.rest() != nullptr) {
      errBadArgument(tokens.rest());
      return;
    }
    Items out;
    WireView view = wireView();
    field->read(view, out);
    ok(out.text());
    return;
  }

  // A write: one or more `field=value` pairs. Every one of them is parsed and validated before any
  // of them is applied, so a command that is going to be refused leaves the machine as it was.
  FieldWrite writes[kMaxFieldWrites];
  uint8_t count = 0;
  char* name = op;

  for (;;) {
    if (!value[0]) {
      errBadArgument(name);
      return;
    }
    const FieldSpec* field = findField(resource, name);
    if (!field) {
      errUnknown(name);
      return;
    }
    if (!field->writable) {
      errUnsupported(name);
      return;
    }
    if (count == kMaxFieldWrites) {
      errBadArgument(name);
      return;
    }
    for (uint8_t i = 0; i < count; i++) {
      // Writing the same field twice is one command with two answers to one question.
      if (writes[i].field == field) {
        errBadArgument(name);
        return;
      }
    }
    writes[count].field = field;
    writes[count].value = value;
    count++;

    char* next = tokens.next();
    if (!next) break;
    name = next;
    value = splitValue(name);
    if (!value) {
      // A word with no value among the fields of a write is a stray argument.
      errBadArgument(name);
      return;
    }
    lower(name);
  }

  if (!resource.write) {
    // Unreachable: a resource whose fields are all read-only is refused field by field above.
    errUnsupported(name);
    return;
  }
  if (!(this->*resource.write)(writes, count)) return;

  // The echo is the fields read back, which is what makes a write and a read of the same field
  // agree by construction.
  Items out;
  WireView view = wireView();
  for (uint8_t i = 0; i < count; i++) writes[i].field->read(view, out);
  ok(out.text());
}

WireView SerialLink::wireView() const {
  // The machine is read live: a reply has to describe it as the command left it rather than as the
  // previous loop iteration left it.
  WireView view = {*context_, logEnabled_, terminator_, customTerminator_,
                   present_ != nullptr, attached_};
  return view;
}

void SerialLink::renderField(const char* resource, const char* field, Items& out) const {
  uint8_t count = 0;
  const ResourceSpec* resources = SerialLink::resources(count);
  for (uint8_t i = 0; i < count; i++) {
    if (!same(resources[i].name, resource)) continue;
    const FieldSpec* spec = findField(resources[i], field);
    if (spec) spec->read(wireView(), out);
    return;
  }
}

// --- writers ---------------------------------------------------------------

bool SerialLink::writeDevice(const FieldWrite* writes, uint8_t count) {
  (void)count;  // one writable field, so there is nothing to stage
  bool on = false;
  if (!parseFlag(writes[0].value, on)) {
    errBadArgument(writes[0].value);
    return false;
  }
  logEnabled_ = on;
  return true;
}

bool SerialLink::writeVacuum(const FieldWrite* writes, uint8_t count) {
  (void)count;  // one writable field
  const FieldWrite& write = writes[0];

  // `-` is the protocol's word for unset, and an unset target releases the chamber: a run without a
  // target is not a run.
  if (same(write.value, "-")) {
    context_->vacuum().clearTarget();
    return true;
  }

  float kpa = 0.0f;
  if (!parseFloat(write.value, kpa)) {
    errBadArgument(write.value);
    return false;
  }
  // The bound comes from the table the help page prints, so the reply and the page cannot disagree.
  // It is inclusive: the help page prints `min=5.0` and a host may send 5.0.
  if (kpa < *write.field->min) {
    errOutOfRange(write.field->name, write.value, write.field->min, write.field->max);
    return false;
  }
  context_->vacuum().setTarget(kpa);
  return true;
}

bool SerialLink::writeLink(const FieldWrite* writes, uint8_t count) {
  (void)count;  // one writable field
  return applyTerminator(writes[0].value);
}

// --- framing ---------------------------------------------------------------

bool SerialLink::applyTerminator(const char* token) {
  // `off` is reserved in any case, because it is how newline framing is spelled: a marker cannot be
  // that word.
  if (sameFold(token, "off")) {
    terminator_ = cfg::LINE_TERMINATOR;
    customTerminator_ = false;
    return true;
  }

  String decoded;
  if (!decodeTerminator(token, decoded)) {
    errBadArgument(token);
    return false;
  }
  if (decoded.length() == 0 || decoded.length() > cfg::TERMINATOR_MAX) {
    Items detail;
    detail.add("field", "term");
    detail.add("value", token);
    detail.addU32("max", cfg::TERMINATOR_MAX);
    err("out_of_range", detail.text());
    return false;
  }

  terminator_ = decoded;
  customTerminator_ = true;
  return true;
}

// --- actions ---------------------------------------------------------------

void SerialLink::deviceReboot() {
  okResult("reboot");
  // The reply is out before the restart, because there is no after: a host that received it knows
  // the device is going away, and one that did not can tell from the port disappearing.
  delay(50);
  ESP.restart();
}

void SerialLink::vacuumStart() {
  if (!context_->vacuum().start()) {
    // Three ways to be refused, three reasons: the vacuum system answers with the one that applied,
    // so the reply and the refusal come from the same conditions.
    Items detail;
    detail.add("reason", context_->vacuum().startRefusal());
    err("not_ready", detail.text());
    return;
  }
  Items out;
  out.add("result", "start");
  renderField("vacuum", "state", out);
  ok(out.text());
}

void SerialLink::vacuumStop() {
  context_->vacuum().stop();
  Items out;
  out.add("result", "stop");
  renderField("vacuum", "state", out);
  ok(out.text());
}

void SerialLink::vacuumReset() {
  context_->vacuum().resetFault();
  Items out;
  out.add("result", "reset");
  renderField("vacuum", "state", out);
  ok(out.text());
}

}  // namespace gardener