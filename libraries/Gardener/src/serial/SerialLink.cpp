#include "serial/SerialLink.h"

#include "core/Logger.h"

namespace gardener {
inline namespace serial {

SerialLink::SerialLink(Stream& stream, PresenceFn present, Logger& logger, CommandQueue& queue,
                       SerialParts parts)
    : outbox_(queue), stream_(stream), port_(stream), present_(present), logger_(logger),
      parts_(parts) {}

void SerialLink::begin() {
  attached_ = controllerAttached();
  buffer_ = "";
  swallowing_ = false;
  logger_.info("serial: interface ready");
}

bool SerialLink::controllerAttached() const { return present_ ? present_() : true; }

// --- replies ---------------------------------------------------------------
// The link writes only its own errors and diagnostics. A grammar formats its reply through the
// stream it is handed.

void SerialLink::linkError(const char* code, const String& detail) {
  stream_.print("err ");
  stream_.print(code);
  if (!detail.isEmpty()) {
    stream_.print(' ');
    stream_.print(detail);
  }
  stream_.print('\n');
}

// --- bytes -----------------------------------------------------------------

void SerialLink::poll(uint32_t nowMs) {
  (void)nowMs;

  // A command the control task's queue could not take on an earlier pass is retried first, so a
  // momentarily full queue costs this link a beat rather than the command.
  outbox_.flush();

  // The link reads the domain token and nothing else. Once the token is followed by a space or a
  // tab, the stream belongs to the grammar, which parses the rest of the line (and any body).
  while (stream_.available() > 0) {
    int value = stream_.read();
    if (value < 0) break;

    if (swallowing_) {
      if (value == '\n') swallowing_ = false;
      continue;
    }

    if (value == '\n') {
      if (buffer_.length() > 0 && buffer_.charAt(buffer_.length() - 1) == '\r') {
        buffer_.remove(buffer_.length() - 1);
      }
      String token = buffer_;
      buffer_ = "";
      if (token.isEmpty() || token.charAt(0) == '#') continue;
      if (token.length() > COMMAND_LINE_MAX) {
        linkError("bad_argument", "");
        continue;
      }

      token.toLowerCase();
      if (grammarFor(token) != Grammar::None) {
        // A grammar with no command.
        linkError("bad_argument", "");
      } else {
        linkError("unknown_domain", token);
      }
      continue;
    }

    if (value == ' ' || value == '\t') {
      if (buffer_.isEmpty()) continue;
      if (buffer_.charAt(0) == '#') {
        buffer_ = "";
        swallowing_ = true;
        continue;
      }
      String token = buffer_;
      buffer_ = "";
      if (!dispatch(token)) swallowing_ = true;
      continue;
    }

    if (buffer_.length() > COMMAND_LINE_MAX) {
      linkError("bad_argument", "");
      buffer_ = "";
      swallowing_ = true;
      continue;
    }
    buffer_ += (char)value;
  }

  bool attached = controllerAttached();
  if (attached_ && !attached) {
    buffer_ = "";
    swallowing_ = false;
    logger_.warn("serial: controller detached");
  }
  attached_ = attached;
}

// The one list of what the link serves: the dispatcher and the token-only line both read it.
SerialLink::Grammar SerialLink::grammarFor(const String& token) const {
  if (token == "vacuum") return Grammar::Vacuum;
  if (token == "device") return Grammar::Device;
  if (token == "buzzer") return Grammar::Buzzer;
  if (token == "fs") return Grammar::Files;
  return Grammar::None;
}

bool SerialLink::dispatch(const String& token) {
  String name = token;
  name.toLowerCase();

  switch (grammarFor(name)) {
    case Grammar::Vacuum:
      executeVacuum(port_);
      return true;
    case Grammar::Device:
      executeDevice(port_);
      return true;
    case Grammar::Buzzer:
      executeBuzzer(port_);
      return true;
    case Grammar::Files:
      executeFiles(port_);
      return true;
    case Grammar::None:
      break;
  }

  linkError("unknown_domain", name);
  return false;
}

}  // namespace serial
}  // namespace gardener
