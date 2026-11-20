#include "serial/SerialLink.h"

#include "core/Logger.h"

namespace gardener {
namespace {

// A heartbeat every five seconds: often enough that a controller sees the machine is alive and what
// it is doing, rarely enough that it stays out of the way of a command's reply.
constexpr uint32_t HeartbeatPeriodMs = 5000;

}  // namespace

SerialLink::SerialLink(Stream& stream, PresenceFn present, Logger& logger)
    : stream_(stream), present_(present), logger_(logger) {}

void SerialLink::add(CommandPlugin& plugin) {
  if (pluginCount_ >= SerialPluginMax) {
    String line = "serial: no room for the ";
    line += plugin.prefix();
    line += " prefix";
    logger_.warn(line.c_str());
    return;
  }

  for (uint8_t i = 0; i < pluginCount_; i++) {
    if (String(plugins_[i]->prefix()) == plugin.prefix()) {
      String line = "serial: ";
      line += plugin.prefix();
      line += " is already served";
      logger_.warn(line.c_str());
      return;
    }
  }

  plugins_[pluginCount_++] = &plugin;
}

void SerialLink::begin() {
  attached_ = controllerAttached();
  buffer_ = "";
  overlong_ = false;

  // The prefixes are logged with the readiness line, so a build that registered the wrong set says
  // so on the bench rather than three commands later.
  String line = "serial: interface ready";
  for (uint8_t i = 0; i < pluginCount_; i++) {
    line += ' ';
    line += plugins_[i]->prefix();
  }
  logger_.info(line.c_str());
}

bool SerialLink::controllerAttached() const { return present_ ? present_() : true; }

// --- replies ---------------------------------------------------------------
// The link writes its own errors; a plugin formats its reply through the stream it is handed.

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
  // the text to the plugin that serves its first word. A line that outgrew CommandLineMax is
  // answered once and its remainder dropped, so a host that never sends an LF cannot grow the
  // buffer without bound.
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
      dispatch(stream_, line);
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

void SerialLink::dispatch(Stream& io, const String& line) {
  const int space = line.indexOf(' ');
  String prefix = space < 0 ? line : line.substring(0, (unsigned int)space);
  String args = space < 0 ? String() : line.substring((unsigned int)space + 1);
  args.trim();
  prefix.toLowerCase();

  for (uint8_t i = 0; i < pluginCount_; i++) {
    if (prefix == plugins_[i]->prefix()) {
      plugins_[i]->serve(io, args);
      return;
    }
  }

  // The first word is a part's own, so an unserved one names the word rather than a verb shape.
  io.print("err unknown_prefix ");
  io.print(prefix);
  io.print('\n');
}

// Unsolicited, so each plugin's line carries a word of its own: a controller reading `ok` and `err`
// replies does not mistake a heartbeat for an answer. Nothing has it while nobody is listening:
// there is no one to read it, and the lines would only sit in the USB CDC's ring.
void SerialLink::heartbeat(uint32_t nowMs) {
  if (!attached_) return;
  if (nowMs - lastHeartbeatMs_ < HeartbeatPeriodMs) return;
  lastHeartbeatMs_ = nowMs;
  for (uint8_t i = 0; i < pluginCount_; i++)
    plugins_[i]->heartbeat(stream_);
}

}  // namespace gardener
