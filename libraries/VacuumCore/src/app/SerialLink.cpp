#include "app/SerialLink.h"

#include <string.h>

#include "app/VacuumSystem.h"

namespace gardener {
namespace {

constexpr size_t kMaxArgs = 3;  // verb [key] [value]; the grammar has nothing else
// Drawing is the exception: a host that draws faster than the loop runs would otherwise put the
// sensor and the PID behind a burst of lines, so a poll services this many and leaves the rest
// in the stream for the next iteration.
constexpr size_t kLinesPerPoll = 8;
constexpr const char* kDrawOps =
    "clear|pixel|line|hline|vline|rect|fillrect|roundrect|fillroundrect|circle|fillcircle|"
    "ellipse|fillellipse|triangle|filltriangle|rotatedrect|fillrotatedrect|text";

// True when the line begins with `word` followed by a separator or the end, so `draw` is not
// confused with a verb that merely starts with those letters. Case insensitive, like the
// dispatcher.
bool startsWith(const char* line, const char* word) {
  for (; *word != '\0'; word++, line++) {
    char c = *line;
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (c != *word) return false;
  }
  return *line == '\0' || *line == ' ' || *line == '\t';
}

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

// A coordinate. Out of range is refused rather than wrapped: the library clips anything off the
// panel, but a coordinate that does not fit in an int16_t is a client bug worth telling about.
bool parseCoord(const char* text, int16_t& out) {
  float value = 0.0f;
  if (!parseFloat(text, value)) return false;
  if (value < -32768.0f || value > 32767.0f) return false;
  out = (int16_t)value;
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

// Walks a command line one token at a time, NUL terminating as it goes, so a command can take
// exactly as many tokens as it needs and then treat the rest of the line as text. `splitLine`
// cannot do that: it takes a fixed number of tokens up front, which would cut a drawn string at
// its first space. Declared in the header because the draw path takes one, defined here because
// nothing else needs to know how it works.
class Tokens {
 public:
  explicit Tokens(char* line) : cursor_(line) {}

  char* next() {
    while (*cursor_ == ' ' || *cursor_ == '\t') cursor_++;
    if (*cursor_ == '\0') return nullptr;
    char* token = cursor_;
    while (*cursor_ != '\0' && *cursor_ != ' ' && *cursor_ != '\t') cursor_++;
    if (*cursor_ != '\0') *cursor_++ = '\0';
    return token;
  }

  // Everything left, leading spaces skipped: the string a `draw text` carries, or null when the
  // line is spent.
  char* rest() {
    while (*cursor_ == ' ' || *cursor_ == '\t') cursor_++;
    return *cursor_ == '\0' ? nullptr : cursor_;
  }

 private:
  char* cursor_;
};

SerialLink::SerialLink(VacuumSystem& system, Display& display, Stream& stream, PresenceFn present,
                       const SystemSnapshot& snapshot)
    : system_(system),
      display_(display),
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

  // A host can draw faster than the loop runs, and this is where that gets refused: servicing
  // every buffered line in one pass would put the sensor and the PID behind a burst of drawing,
  // so a poll takes this many lines and leaves the rest in the stream.
  size_t serviced = 0;
  while (serviced < kLinesPerPoll && stream_.available() > 0) {
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
      serviced++;
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
  // docs/serial-protocol.md rather than on the wire, and the drawing ops are their own list
  // because there are eighteen of them.
  ok("help=info|status|help|start|stop|reset|reboot|ambient|set|draw|flush");
}

// --- drawing ---------------------------------------------------------------

// Takes the numbers an op asks for and refuses anything left over, so `draw pixel 1 2 3` is an
// error rather than a pixel. The reply lives in apply() so the sixteen ops that take coordinates
// cannot each get it slightly wrong.
bool SerialLink::coords(Tokens& tokens, size_t count, int16_t* out) {
  for (size_t i = 0; i < count; i++) {
    char* token = tokens.next();
    if (!token || !parseCoord(token, out[i])) return false;
  }
  return true;
}

void SerialLink::draw(Tokens& tokens) {
  tokens.next();  // the verb, already recognised

  char* opToken = tokens.next();
  if (!opToken) {
    err("bad_argument");
    return;
  }
  lower(opToken);
  const char* op = opToken;

  if (same(op, "help")) {
    String payload = "ops=";
    payload += kDrawOps;
    ok(payload.c_str());
    return;
  }
  if (same(op, "clear")) {
    if (tokens.rest() != nullptr) {
      err("bad_argument");
      return;
    }
    display_.clear();
    ok("drawn");
    return;
  }
  if (same(op, "pixel")) {
    apply(tokens, 2, [](Display& d, const int16_t* a) { d.pixel(a[0], a[1]); });
    return;
  }
  if (same(op, "line")) {
    apply(tokens, 4, [](Display& d, const int16_t* a) { d.line(a[0], a[1], a[2], a[3]); });
    return;
  }
  if (same(op, "hline")) {
    apply(tokens, 3, [](Display& d, const int16_t* a) { d.hLine(a[0], a[1], a[2]); });
    return;
  }
  if (same(op, "vline")) {
    apply(tokens, 3, [](Display& d, const int16_t* a) { d.vLine(a[0], a[1], a[2]); });
    return;
  }
  if (same(op, "rect")) {
    apply(tokens, 4, [](Display& d, const int16_t* a) { d.rect(a[0], a[1], a[2], a[3]); });
    return;
  }
  if (same(op, "fillrect")) {
    apply(tokens, 4, [](Display& d, const int16_t* a) { d.fillRect(a[0], a[1], a[2], a[3]); });
    return;
  }
  if (same(op, "roundrect")) {
    apply(tokens, 5,
          [](Display& d, const int16_t* a) { d.roundRect(a[0], a[1], a[2], a[3], a[4]); });
    return;
  }
  if (same(op, "fillroundrect")) {
    apply(tokens, 5,
          [](Display& d, const int16_t* a) { d.fillRoundRect(a[0], a[1], a[2], a[3], a[4]); });
    return;
  }
  if (same(op, "circle")) {
    apply(tokens, 3, [](Display& d, const int16_t* a) { d.circle(a[0], a[1], a[2]); });
    return;
  }
  if (same(op, "fillcircle")) {
    apply(tokens, 3, [](Display& d, const int16_t* a) { d.fillCircle(a[0], a[1], a[2]); });
    return;
  }
  if (same(op, "ellipse")) {
    apply(tokens, 4, [](Display& d, const int16_t* a) { d.ellipse(a[0], a[1], a[2], a[3]); });
    return;
  }
  if (same(op, "fillellipse")) {
    apply(tokens, 4, [](Display& d, const int16_t* a) { d.fillEllipse(a[0], a[1], a[2], a[3]); });
    return;
  }
  if (same(op, "triangle")) {
    apply(tokens, 6, [](Display& d, const int16_t* a) {
      d.triangle(a[0], a[1], a[2], a[3], a[4], a[5]);
    });
    return;
  }
  if (same(op, "filltriangle")) {
    apply(tokens, 6, [](Display& d, const int16_t* a) {
      d.fillTriangle(a[0], a[1], a[2], a[3], a[4], a[5]);
    });
    return;
  }
  if (same(op, "rotatedrect")) {
    apply(tokens, 5,
          [](Display& d, const int16_t* a) { d.rotatedRect(a[0], a[1], a[2], a[3], a[4]); });
    return;
  }
  if (same(op, "fillrotatedrect")) {
    apply(tokens, 5,
          [](Display& d, const int16_t* a) { d.fillRotatedRect(a[0], a[1], a[2], a[3], a[4]); });
    return;
  }
  if (same(op, "text")) {
    int16_t args[4];
    if (!coords(tokens, 4, args)) {
      err("bad_argument");
      return;
    }
    if (args[2] < 1 || args[2] > 8) {
      err("out_of_range");
      return;
    }
    const char* value = tokens.rest();
    if (!value) {
      err("bad_argument");
      return;
    }
    display_.text(args[0], args[1], value, (uint8_t)args[2], args[3] != 0);
    ok("drawn");
    return;
  }

  err("unknown_command");
}

void SerialLink::dispatch(char* line) {
  // Drawing is recognised before anything is split, because `draw text` carries a string that
  // may contain spaces: splitting a fixed number of tokens up front would cut it at the first
  // one, and re-splitting a buffer the splitter has already terminated is worse.
  if (startsWith(line, "draw")) {
    Tokens tokens(line);
    draw(tokens);
    return;
  }

  char* argv[kMaxArgs];
  size_t argc = splitLine(line, argv, kMaxArgs);
  if (argc == 0 || argv[0][0] == '#') return;  // blank line or a comment

  lower(argv[0]);
  const char* command = argv[0];

  if (same(command, "help")) {
    emitHelp();
    return;
  }
  if (same(command, "flush")) {
    // The frame boundary: everything drawn since the last one goes to the glass, with the
    // battery composited on the way out.
    display_.flush();
    ok("flushed");
    return;
  }
  if (same(command, "info") || same(command, "status")) {
    String payload = same(command, "info") ? infoBody() : statusBody();
    ok(payload.c_str());
    return;
  }
  if (same(command, "start")) {
    if (!system_.start()) {
      err("not_ready");
      return;
    }
    ok("started");
    return;
  }
  if (same(command, "stop")) {
    system_.stop();
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
        system_.clearTarget();
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
      system_.setTarget(kpa);
      String payload = "target=" + String(kpa, 1);
      ok(payload.c_str());
      return;
    }
    if (same(key, "ambient")) {
      float kpa = 0.0f;
      if (!parseFloat(value, kpa)) {
        err("bad_argument");
        return;
      }
      if (!system_.setAmbient(kpa)) {
        err("out_of_range");
        return;
      }
      String payload = "ambient=" + String(system_.state().ambientKpa, 2);
      ok(payload.c_str());
      return;
    }
    if (same(key, "invert")) {
      if (!same(value, "on") && !same(value, "off")) {
        err("bad_argument");
        return;
      }
      display_.invert(same(value, "on"));
      ok(same(value, "on") ? "invert=on" : "invert=off");
      return;
    }
    if (same(key, "rotation")) {
      int16_t rotation = 0;
      if (!parseCoord(value, rotation)) {
        err("bad_argument");
        return;
      }
      if (rotation < 0 || rotation > 3) {
        err("out_of_range");
        return;
      }
      display_.setRotation((uint8_t)rotation);
      String payload = "rotation=" + String(rotation);
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
      if (!system_.setAmbient(kpa)) {
        err("out_of_range");
        return;
      }
    } else {
      system_.captureAmbient();
    }
    String payload = "ambient=" + String(system_.state().ambientKpa, 2);
    ok(payload.c_str());
    return;
  }
  // v2 turned telemetry on and off by command; a controller polls `status` now.
  if (same(command, "telemetry") || same(command, "tel")) {
    err("unsupported");
    return;
  }
  if (same(command, "reset")) {
    system_.resetFault();
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

  addFloatToken(buf, "kpa", snapshot_.plant.kpa, 2);
  addFloatToken(buf, "vac", snapshot_.plant.vacuumKpa, 2);
  addFloatToken(buf, "pct", snapshot_.plant.vacuumPercent, 1);
  if (snapshot_.plant.targetActive) {
    addFloatToken(buf, "target", snapshot_.plant.targetKpa, 1);
  } else {
    addToken(buf, "target", "-");
  }
  addFloatToken(buf, "floor", snapshot_.plant.floorKpa, 1);
  addFloatToken(buf, "duty", snapshot_.plant.duty, 1);
  addToken(buf, "state", runStateName(snapshot_.plant.runState));
  if (snapshot_.batteryPresent) {
    addU32Token(buf, "batt", snapshot_.batteryPercent);
  } else {
    addToken(buf, "batt", "-");
  }
  addToken(buf, "sensor", snapshot_.plant.sensorOk ? "1" : "0");
  addToken(buf, "fault", snapshot_.plant.faulted ? snapshot_.plant.faultCode : "none");
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

}  // namespace gardener
