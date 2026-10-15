#pragma once

#include <Arduino.h>

#include "core/Config.h"
#include "core/Snapshot.h"

namespace eggplan {

class CommandRouter;

// The serial interface to the controller: one line grammar over a Stream, and nothing
// else.
//
// Everything runs on the main loop, so inbound bytes are assembled here and dispatched
// inline, and replies go straight back out. There is no second task, no queue, and no
// framing above the line.
//
// The stream and the presence check are injected rather than assumed, so pointing the
// interface at UART0 instead of USB CDC is a one line change in App.
class SerialService {
 public:
  // True while a host has the port open. Null means the stream cannot tell (a plain
  // UART), in which case a lost link is simply never reported.
  using PresenceFn = bool (*)();

  SerialService(CommandRouter& router, Stream& stream, PresenceFn present,
                const LinkSettings& settings, const SystemSnapshot& snapshot);

  void begin();
  void poll(uint32_t nowMs);

  bool controllerAttached() const;
  // One shot: the controller went away. App decides what that means (stop and vent); the
  // interface only reports it.
  bool consumeLinkLost();

  // Diagnostics: the log shares the stream with the protocol, distinguished by the `[`
  // prefix, so a controller can tell a record from a reply. Everything that wants to say
  // something does it through here, which is also what makes `set log off` total.
  void info(const char* text);
  void warn(const char* text);
  void error(const char* text);

 private:
  void handleLine();
  void logLine(char level, const char* text);
  void writeLine(const char* prefix, const char* text);

  CommandRouter& router_;
  Stream& stream_;
  PresenceFn present_;
  const LinkSettings& settings_;
  const SystemSnapshot& snapshot_;

  char line_[cfg::COMMAND_LINE_MAX];
  size_t length_ = 0;
  bool oversize_ = false;  // the current line is too long and was answered already
  bool attached_ = false;
  bool linkLost_ = false;
};

}  // namespace eggplan
