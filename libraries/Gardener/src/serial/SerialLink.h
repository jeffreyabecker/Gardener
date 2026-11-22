#pragma once

#include <Arduino.h>

#include "core/Knob.h"
#include "serial/CommandPlugin.h"

namespace gardener {
class Logger;

// How many prefixes one link serves: the parts this build registers and no more.
GARDENER_KNOB_VALID(uint8_t, SerialPluginMax, GARDENER_SERIAL_PLUGIN_MAX, 4,
                    GARDENER_SERIAL_PLUGIN_MAX > 0, "a table of nothing serves no prefix");

// The longest command line the link serves: a longer one is refused and its remainder dropped.
GARDENER_KNOB_VALID(uint16_t, CommandLineMax, GARDENER_SERIAL_COMMAND_LINE_MAX, 128,
                    GARDENER_SERIAL_COMMAND_LINE_MAX >= 32 &&
                        GARDENER_SERIAL_COMMAND_LINE_MAX <= 1024,
                    "a line the link can hold, in bytes");

// The serial interface to the controller. It reads a line to its LF and hands the line, trimmed and
// bounded, to the plugin registered under its first word. A word nothing registered is answered as
// a prefix the link does not serve; everything else a line means is its plugin's.
//
// The link owns its line, its own errors and the clock its heartbeat keeps, and nothing else: a
// part is served by registering a plugin rather than by editing this header, and a build serves
// exactly the parts it has.
class SerialLink {
 public:
  // True while a host has the port open. Null means the stream cannot tell (a plain UART), in which
  // case a lost link is simply never reported.
  using PresenceFn = bool (*)();

  SerialLink(Stream& stream, PresenceFn present, Logger& logger);

  // The prefix this plugin answers and the lines under it. A prefix already taken, or a table with
  // no room, is logged and the plugin is not served.
  void add(CommandPlugin& plugin);

  void begin();
  void poll(uint32_t nowMs);

 private:
  bool controllerAttached() const;

  // One line, already trimmed and bounded: its first word picks the plugin and the rest is that
  // plugin's argument. When nothing serves the word the link answers instead.
  void dispatch(Stream& io, const String& line);
  // The unsolicited line, one pass per HeartbeatPeriodMs of the link's own clock.
  void heartbeat(uint32_t nowMs);
  // The link's own error: a line it could not accept.
  void linkError(const char* code, const String& detail);

  Stream& stream_;
  PresenceFn present_;
  Logger& logger_;
  CommandPlugin* plugins_[SerialPluginMax];
  uint8_t pluginCount_ = 0;
  // The line being assembled, without its LF.
  String buffer_;
  // Set when a line outgrew CommandLineMax: the rest of it is dropped and it is answered once.
  bool overlong_ = false;
  bool attached_ = false;
  // When the heartbeat last went out, in millis.
  uint32_t lastHeartbeatMs_ = 0;
};

}  // namespace gardener
