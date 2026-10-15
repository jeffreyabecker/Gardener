#include "app/SerialLink.h"

#include <string.h>

#include "app/VacuumControl.h"
#include "app/VacuumSystem.h"

namespace eggplan {
namespace {

constexpr size_t kMaxArgs = 3;  // verb [key] [value]; the grammar has nothing else

// Splits in place: every token is NUL terminated, so the caller's buffer is the token
// storage and nothing is allocated.
size_t splitLine(char* line, char* argv[], size_t maxArgs) {
  size_t argc = 0;
  char* cursor = line;
  while (*cursor != '\0' && argc < maxArgs) {
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    if (*cursor == '\0') break;
    argv[argc++] = cursor;
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') cursor++;
    if (*cursor != '\0') *cursor++ = '\0';
  }
  return argc;
}

void lower(char* text) {
  for (; *text != '\0'; text++) {
    if (*text >= 'A' && *text <= 'Z') *text = (char)(*text - 'A' + 'a');
  }
}

bool same(const char* a, const char* b) { return strcmp(a, b) == 0; }

// An argument that does not parse in full is a bad_argument, never a silent zero.
bool parseFloat(const char* text, float& out) {
  if (!text || *text == '\0') return false;
  const char* cursor = text;
  bool negative = false;
  if (*cursor == '+' || *cursor == '-') {
    negative = (*cursor == '-');
    cursor++;
  }

  float value = 0.0f;
  bool any = false;
  while (*cursor >= '0' && *cursor <= '9') {
    value = value * 10.0f + (float)(*cursor - '0');
    cursor++;
    any = true;
  }
  if (*cursor == '.') {
    cursor++;
    float scale = 0.1f;
    while (*cursor >= '0' && *cursor <= '9') {
      value += (float)(*cursor - '0') * scale;
      scale *= 0.1f;
      cursor++;
      any = true;
    }
  }
  if (!any || *cursor != '\0') return false;
  out = negative ? -value : value;
  return true;
}

bool parseU32(const char* text, uint32_t& out) {
  if (!text || *text < '0' || *text > '9') return false;
  uint32_t value = 0;
  for (const char* cursor = text; *cursor != '\0'; cursor++) {
    if (*cursor < '0' || *cursor > '9') return false;
    if (value > 429496729u) return false;  // would wrap
    value = value * 10u + (uint32_t)(*cursor - '0');
  }
  out = value;
  return true;
}

// Every value is a space free token, so '-' can mean "not applicable" without any quoting
// or escaping. This is also where the "no spaces in a value" rule is enforced, because a
// library string is the one value the firmware does not control.
void addToken(String& buf, const char* key, const char* value) {
  if (buf.length() > 0) buf += ' ';
  buf += key;
  buf += '=';
  if (!value || !value[0]) {
    buf += '-';
    return;
  }
  for (const char* cursor = value; *cursor != '\0'; cursor++) {
    char c = *cursor;
    buf += (c == ' ' || c == '\t' || c == '\n' || c == '\r') ? '_' : c;
  }
}

void addFloatToken(String& buf, const char* key, float value, uint8_t decimals) {
  if (buf.length() > 0) buf += ' ';
  buf += key;
  buf += '=';
  // The cast picks String(float, decimals) out of the overload set: a uint8_t also matches
  // the integer constructors, whose base parameter is an unsigned char.
  buf += String(value, (unsigned int)decimals);
}

void addU32Token(String& buf, const char* key, uint32_t value) {
  if (buf.length() > 0) buf += ' ';
  buf += key;
  buf += '=';
  buf += String(value);
}

}  // namespace

SerialLink::SerialLink(VacuumControl& control, VacuumSystem& system, Stream& stream,
                       PresenceFn present, const SystemSnapshot& snapshot)
    : control_(control),
      system_(system),
      stream_(stream),
      present_(present),
      snapshot_(snapshot) {}

void SerialLink::begin() {
  attached_ = controllerAttached();
  length_ = 0;
  oversize_ = false;
  info("serial: interface ready");
}

bool SerialLink::controllerAttached() const { return present_ ? present_() : true; }

bool SerialLink::consumeLinkLost() {
  bool lost = linkLost_;
  linkLost_ = false;
  return lost;
}

// --- diagnostics -----------------------------------------------------------

void SerialLink::logLine(char level, const char* text) {
  if (!logEnabled_ || !text) return;
  stream_.write((uint8_t)'[');
  stream_.write((uint8_t)level);
  stream_.write((uint8_t)']');
  stream_.write((uint8_t)' ');
  stream_.write((const uint8_t*)text, strlen(text));
  stream_.write((uint8_t)'\n');
}

void SerialLink::info(const char* text) { logLine('I', text); }

void SerialLink::warn(const char* text) { logLine('W', text); }

void SerialLink::error(const char* text) { logLine('E', text); }

// --- bytes -----------------------------------------------------------------

void SerialLink::writeLine(const char* prefix, const char* text) {
  stream_.write((const uint8_t*)prefix, strlen(prefix));
  stream_.write((const uint8_t*)text, strlen(text));
  stream_.write((uint8_t)'\n');
}

void SerialLink::handleLine() {
  line_[length_] = '\0';
  dispatch(line_);
  length_ = 0;
}

void SerialLink::poll(uint32_t nowMs) {
  (void)nowMs;

  while (stream_.available() > 0) {
    int value = stream_.read();
    if (value < 0) break;
    char c = (char)value;

    // Both terminators are accepted: a terminal that sends CR alone is a serial console
    // classic, and "\r\n" simply yields an empty line, which is skipped.
    if (c == '\n' || c == '\r') {
      bool oversize = oversize_;
      oversize_ = false;
      if (oversize || length_ == 0) {
        length_ = 0;
        continue;
      }
      handleLine();
      continue;
    }

    if (length_ + 1 >= sizeof(line_)) {
      // Answer once and swallow the rest of the line rather than truncating it into
      // something that parses as a different command.
      if (!oversize_) {
        err("bad_argument");
        oversize_ = true;
      }
      length_ = 0;
      continue;
    }
    line_[length_++] = c;
  }

  bool attached = controllerAttached();
  if (attached_ && !attached) {
    linkLost_ = true;
    length_ = 0;
    oversize_ = false;
    warn("serial: controller detached");
  }
  attached_ = attached;
}

// --- replies ---------------------------------------------------------------

void SerialLink::ok(const char* payload) { writeLine("ok ", payload); }

void SerialLink::err(const char* code) { writeLine("err ", code); }

// --- grammar ---------------------------------------------------------------

void SerialLink::emitHelp() {
  // One line, because every command produces exactly one reply and that is what lets a
  // controller match replies to commands by counting. Argument syntax lives in
  // docs/serial-protocol.md rather than on the wire.
  ok("help=info|status|help|start|stop|reset|reboot|ambient|set");
}

void SerialLink::dispatch(char* line) {
  char* argv[kMaxArgs];
  size_t argc = splitLine(line, argv, kMaxArgs);
  if (argc == 0 || argv[0][0] == '#') return;  // blank line or a comment

  lower(argv[0]);
  const char* command = argv[0];

  if (same(command, "help")) {
    emitHelp();
    return;
  }
  if (same(command, "info") || same(command, "status")) {
    String payload = same(command, "info") ? infoBody() : statusBody();
    ok(payload.c_str());
    return;
  }
  if (same(command, "start")) {
    if (!control_.startRun()) {
      err("not_ready");
      return;
    }
    ok("started");
    return;
  }
  if (same(command, "stop")) {
    control_.stopRun();
    ok("stopped");
    return;
  }
  if (same(command, "set")) {
    if (argc < 3) {
      err("bad_argument");
      return;
    }
    lower(argv[1]);
    lower(argv[2]);
    const char* key = argv[1];
    const char* value = argv[2];

    if (same(key, "target")) {
      if (same(value, "off")) {
        control_.clearTarget();
        ok("target=off");
        return;
      }
      float kpa = 0.0f;
      if (!parseFloat(value, kpa)) {
        err("bad_argument");
        return;
      }
      if (kpa <= cfg::CONTROL_MIN_TARGET_KPA) {
        err("out_of_range");
        return;
      }
      control_.setTarget(kpa);
      String payload = "target=" + String(kpa, 1);
      ok(payload.c_str());
      return;
    }
    if (same(key, "hold")) {
      // The countdown the controller is running, in seconds. The device renders it and never
      // counts it, so this changes nothing about the run.
      if (same(value, "off")) {
        control_.setHoldSeconds(0);
        ok("hold=off");
        return;
      }
      uint32_t seconds = 0;
      if (!parseU32(value, seconds)) {
        err("bad_argument");
        return;
      }
      control_.setHoldSeconds(seconds);
      String payload = "hold=" + String(seconds);
      ok(payload.c_str());
      return;
    }
    if (same(key, "ambient")) {
      float kpa = 0.0f;
      if (!parseFloat(value, kpa)) {
        err("bad_argument");
        return;
      }
      if (!system_.setAmbientKpa(kpa)) {
        err("out_of_range");
        return;
      }
      String payload = "ambient=" + String(system_.ambientKpa(), 2);
      ok(payload.c_str());
      return;
    }
    if (same(key, "log")) {
      if (!same(value, "on") && !same(value, "off")) {
        err("bad_argument");
        return;
      }
      logEnabled_ = same(value, "on");
      ok(logEnabled_ ? "log=on" : "log=off");
      return;
    }
    // v2 configured a telemetry period and could drive the pump directly. Both are gone,
    // and a controller that still asks gets told that rather than that it typed it wrong.
    if (same(key, "period") || same(key, "duty")) {
      err("unsupported");
      return;
    }
    err("unknown_command");
    return;
  }
  if (same(command, "ambient")) {
    if (argc > 1) {
      lower(argv[1]);
      float kpa = 0.0f;
      if (!parseFloat(argv[1], kpa)) {
        err("bad_argument");
        return;
      }
      if (!system_.setAmbientKpa(kpa)) {
        err("out_of_range");
        return;
      }
    } else {
      system_.captureAmbient();
    }
    String payload = "ambient=" + String(system_.ambientKpa(), 2);
    ok(payload.c_str());
    return;
  }
  // v2 turned telemetry on and off by command; a controller polls `status` now.
  if (same(command, "telemetry") || same(command, "tel")) {
    err("unsupported");
    return;
  }
  if (same(command, "reset")) {
    control_.clearFault();
    ok("reset");
    return;
  }
  if (same(command, "reboot")) {
    ok("rebooting");
    delay(50);
    ESP.restart();
    return;
  }

  err("unknown_command");
}

// --- payload text ----------------------------------------------------------

String SerialLink::statusBody() const {
  String buf;

  addFloatToken(buf, "kpa", snapshot_.kpa, 2);
  addFloatToken(buf, "vac", snapshot_.vacuumKpa, 2);
  addFloatToken(buf, "pct", snapshot_.vacuumPercent, 1);
  if (snapshot_.targetActive) {
    addFloatToken(buf, "target", snapshot_.targetKpa, 1);
  } else {
    addToken(buf, "target", "-");
  }
  addFloatToken(buf, "floor", snapshot_.floorKpa, 1);
  addFloatToken(buf, "duty", snapshot_.duty, 1);
  addToken(buf, "state", runStateName(snapshot_.state));
  if (snapshot_.batteryPresent) {
    addU32Token(buf, "batt", snapshot_.batteryPercent);
  } else {
    addToken(buf, "batt", "-");
  }
  addToken(buf, "sensor", snapshot_.sensorOk ? "1" : "0");
  addToken(buf, "fault", snapshot_.pumpTripped ? snapshot_.faultCode : "none");
  addU32Token(buf, "up", snapshot_.uptimeMs);

  return buf;
}

String SerialLink::infoBody() const {
  String buf;

  addToken(buf, "fw", cfg::FIRMWARE_VERSION);
  // Read where they are formatted: two facts from the chip, not a module's state.
  addToken(buf, "chip", ESP.getChipModel());
  addU32Token(buf, "heap", (uint32_t)ESP.getFreeHeap());
  if (snapshot_.batteryPresent) {
    addFloatToken(buf, "batt_v", snapshot_.batteryVolts, 2);
    addU32Token(buf, "batt_pct", snapshot_.batteryPercent);
  } else {
    addToken(buf, "batt_v", "-");
    addToken(buf, "batt_pct", "-");
  }
  addToken(buf, "log", logEnabled_ ? "on" : "off");

  return buf;
}

}  // namespace eggplan
