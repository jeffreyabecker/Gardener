#include "app/SerialLink.h"

#include <stdlib.h>
#include <string.h>

#include "app/VacuumSystem.h"

namespace gardener {
namespace {

constexpr size_t kMaxArgs = 3;  // verb [key] [value]; the grammar has nothing else

// `set term` takes a case sensitive marker, with \n, \r, \t and \\ standing for the characters
// a command line cannot carry directly. False for a trailing or unknown escape, never a silent
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

}  // namespace

SerialLink::SerialLink(VacuumSystem& system, Display& display, Stream& stream, PresenceFn present,
                       const SystemSnapshot& snapshot)
    : system_(system),
      display_(display),
      stream_(stream),
      present_(present),
      snapshot_(snapshot) {}

void SerialLink::begin() {
  attached_ = controllerAttached();
  buffer_ = "";
  swallowing_ = false;
  terminator_ = cfg::LINE_TERMINATOR;
  customTerminator_ = false;
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

void SerialLink::dispatchLine(const String& line) {
  // The grammar is parsed in place, so a line gets a writable copy. It is sized by the line
  // rather than by a fixed maximum, because a custom terminator lets one line be a whole
  // program's source.
  size_t length = line.length();
  char* text = (char*)malloc(length + 1);
  if (!text) {
    err("bad_argument");
    return;
  }
  memcpy(text, line.c_str(), length);
  text[length] = '\0';
  dispatch(text);
  free(text);
}

// Splits the buffer on the terminator and dispatches every complete line, in arrival order.
// A '\r' immediately before the terminator is dropped, so a host that ends its lines with
// "\r\n" still works against the default terminator. Whatever follows the last terminator is an
// unterminated line and stays in the buffer for a later poll.
void SerialLink::drainLines() {
  if (terminator_.length() == 0) return;
  const size_t limit = customTerminator_ ? cfg::COMMAND_PAYLOAD_MAX : cfg::COMMAND_LINE_MAX;

  // The rest of an overlong line is thrown away up to its terminator, which may only arrive in
  // a later poll.
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
    if (length >= limit) {
      // Answer once and drop it rather than truncating into something that parses as a
      // different command.
      err("bad_argument");
    } else if (length > 0) {
      dispatchLine(buffer_.substring(0, (unsigned int)length));
    }
    buffer_.remove(0, (unsigned int)end + terminator_.length());
  }

  // A tail that already fills the whole allowance can never become a valid line. Refuse it
  // once and swallow the rest until its terminator arrives.
  if (!swallowing_ && buffer_.length() >= limit) {
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
    // A new controller starts from newline framing rather than inheriting a marker it may not
    // know about.
    terminator_ = cfg::LINE_TERMINATOR;
    customTerminator_ = false;
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

// `set term <string>` points the line terminator at a marker so a host can send a whole
// multi-line payload as one command; `set term off` puts newline framing back.
void SerialLink::setTerminator(const char* token) {
  String word = token;
  word.toLowerCase();
  if (same(word.c_str(), "off") || same(word.c_str(), "-")) {
    terminator_ = cfg::LINE_TERMINATOR;
    customTerminator_ = false;
    ok("term=off");
    return;
  }

  String decoded;
  if (!decodeTerminator(token, decoded)) {
    err("bad_argument");
    return;
  }
  if (decoded.length() == 0 || decoded.length() > cfg::TERMINATOR_MAX) {
    err("out_of_range");
    return;
  }
  terminator_ = decoded;
  customTerminator_ = true;

  // Echo the token as it was written: it has no spaces, and the escapes keep it round
  // trippable, so the reply is one unambiguous item whatever the marker is.
  String payload = "term=";
  payload += token;
  ok(payload.c_str());
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
    const char* key = argv[1];
    const char* value = argv[2];
    // `set term` takes a case sensitive marker, so it reads the token before it is folded.
    if (same(key, "term")) {
      setTerminator(value);
      return;
    }
    lower(argv[2]);

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
    // v2 configured a telemetry period and could drive the pump directly, and the ambient
    // reference used to be typed in. All three are gone, and a controller that still asks gets
    // told that rather than that it typed it wrong.
    if (same(key, "period") || same(key, "duty") || same(key, "ambient")) {
      err("unsupported");
      return;
    }
    err("unknown_command");
    return;
  }
  if (same(command, "ambient")) {
    // The reference is taken from the sensor and never typed in: a second way to set it was one
    // more way for the rig to be holding against a number nobody measured.
    if (argc > 1) {
      err("unsupported");
      return;
    }
    system_.captureAmbient();
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

}  // namespace gardener
