#include "app/serial/SerialLink.h"

#include "app/Logger.h"
#include "core/Config.h"

namespace gardener {

SerialLink::SerialLink(Stream& stream, PresenceFn present, Logger& logger)
    : stream_(stream), present_(present), logger_(logger) {}

void SerialLink::begin() {
  attached_ = controllerAttached();
  buffer_ = "";
  swallowing_ = false;
  logger_.info("serial: interface ready");
}

bool SerialLink::controllerAttached() const { return present_ ? present_() : true; }

bool SerialLink::consumeLinkLost() {
  bool lost = linkLost_;
  linkLost_ = false;
  return lost;
}

// --- replies ---------------------------------------------------------------
// The link writes only its own errors and diagnostics. A domain formats its reply through the
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

  // The link reads the domain token and nothing else. Once the token is followed by a space or a
  // tab, the stream belongs to the domain, which parses the rest of the line (and any body).
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
      if (token.length() > cfg::COMMAND_LINE_MAX) {
        linkError("bad_argument", "");
        continue;
      }

      token.toLowerCase();
      if (domains_.find(token.c_str())) {
        // A domain with no command.
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
      if (!dispatchDomain(token)) swallowing_ = true;
      continue;
    }

    if (buffer_.length() > cfg::COMMAND_LINE_MAX) {
      linkError("bad_argument", "");
      buffer_ = "";
      swallowing_ = true;
      continue;
    }
    buffer_ += (char)value;
  }

  bool attached = controllerAttached();
  if (attached_ && !attached) {
    linkLost_ = true;
    buffer_ = "";
    swallowing_ = false;
    logger_.warn("serial: controller detached");
  }
  attached_ = attached;
}

bool SerialLink::dispatchDomain(const String& token) {
  String name = token;
  name.toLowerCase();

  Domain* domain = domains_.find(name.c_str());
  if (!domain) {
    linkError("unknown_domain", name);
    return false;
  }

  domain->execute(stream_);
  return true;
}

}  // namespace gardener
