#include "serial/SerialLink.h"

#include "core/Logger.h"

namespace gardener {
namespace {

// A heartbeat every five seconds: often enough that a controller sees the machine is alive and what
// it is doing, rarely enough that it stays out of the way of a command's reply.
constexpr uint32_t HeartbeatPeriodMs = 5000;

}  // namespace

SerialLink::SerialLink(Stream& stream, PresenceFn present, Logger& logger, SystemControl& control)
    : stream_(stream), present_(present), logger_(logger), control_(control) {}

void SerialLink::begin() {
  attached_ = controllerAttached();
  buffer_ = "";
  overlong_ = false;
  logger_.info("serial: interface ready");
}

bool SerialLink::controllerAttached() const { return present_ ? present_() : true; }

// --- replies ---------------------------------------------------------------
// The link writes its own errors and diagnostics; a grammar formats its reply through the stream it
// is handed. The heartbeat is the one line the link speaks with the machine's own words.

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
  // The line is the whole command: the link assembles it to its LF, drops the line ending and hands
  // the text to the grammar. A line that outgrew CommandLineMax is answered once and its remainder
  // dropped, so a host that never sends an LF cannot grow the buffer without bound.
  while (stream_.available() > 0) {
    int value = stream_.read();
    if (value < 0) break;

    if (value == '\n') {
      String line = buffer_;
      buffer_ = "";
      if (overlong_ || line.length() > CommandLineMax) {
        overlong_ = false;
        linkError("bad_argument", "");
        continue;
      }
      line.trim();
      if (line.isEmpty() || line.charAt(0) == '#') continue;
      execute(stream_, line);
      continue;
    }

    if (overlong_) continue;
    if (buffer_.length() > CommandLineMax) {
      buffer_ = "";
      overlong_ = true;
      continue;
    }
    buffer_ += (char)value;
  }

  bool attached = controllerAttached();
  if (attached_ && !attached) {
    buffer_ = "";
    overlong_ = false;
    logger_.warn("serial: controller detached");
  }
  attached_ = attached;
  heartbeat(nowMs);
}

// Unsolicited, so it carries a word of its own: a controller reading `ok` and `err` replies does not
// mistake a heartbeat for an answer. Nothing has it while nobody is listening: there is no one to
// read it, and the lines would only sit in the USB CDC's ring.
void SerialLink::heartbeat(uint32_t nowMs) {
  if (!attached_) return;
  if (nowMs - lastHeartbeatMs_ < HeartbeatPeriodMs) return;
  lastHeartbeatMs_ = nowMs;
  writeStatus(stream_, "hb ");
}

}  // namespace gardener