#include "hosts/serial/SerialService.h"

#include <string.h>

#include "hosts/commands/CommandRouter.h"

namespace eggplan {

SerialService::SerialService(CommandRouter& router, Stream& stream, PresenceFn present,
                             const LinkSettings& settings, const SystemSnapshot& snapshot)
    : router_(router),
      stream_(stream),
      present_(present),
      settings_(settings),
      snapshot_(snapshot) {}

void SerialService::begin() {
  attached_ = controllerAttached();
  length_ = 0;
  oversize_ = false;
  info("serial: interface ready");
}

bool SerialService::controllerAttached() const { return present_ ? present_() : true; }

bool SerialService::consumeLinkLost() {
  bool lost = linkLost_;
  linkLost_ = false;
  return lost;
}

void SerialService::logLine(char level, const char* text) {
  if (!settings_.logEnabled || !text) return;
  stream_.write((uint8_t)'[');
  stream_.write((uint8_t)level);
  stream_.write((uint8_t)']');
  stream_.write((uint8_t)' ');
  stream_.write((const uint8_t*)text, strlen(text));
  stream_.write((uint8_t)'\n');
}

void SerialService::info(const char* text) { logLine('I', text); }

void SerialService::warn(const char* text) { logLine('W', text); }

void SerialService::error(const char* text) { logLine('E', text); }

void SerialService::writeLine(const char* prefix, const char* text) {
  stream_.write((const uint8_t*)prefix, strlen(prefix));
  stream_.write((const uint8_t*)text, strlen(text));
  stream_.write((uint8_t)'\n');
}

void SerialService::handleLine() {
  line_[length_] = '\0';
  router_.handleLine(line_, stream_, snapshot_);
  length_ = 0;
}

void SerialService::poll(uint32_t nowMs) {
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
        writeLine("err bad_argument", "");
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

}  // namespace eggplan
