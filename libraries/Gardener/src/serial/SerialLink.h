#pragma once

#include <Arduino.h>

#include "core/Knob.h"

namespace gardener {
class Logger;
class SystemControl;

// The longest command line the link serves: a longer one is refused and its remainder dropped.
GARDENER_KNOB_VALID(uint16_t, CommandLineMax, GARDENER_SERIAL_COMMAND_LINE_MAX, 128,
                    GARDENER_SERIAL_COMMAND_LINE_MAX >= 32 &&
                        GARDENER_SERIAL_COMMAND_LINE_MAX <= 1024,
                    "a line the link can hold, in bytes");

// The serial interface to the controller. It reads a line to its LF and hands the line, trimmed and
// bounded, to the machine's grammar, which parses the verb and its argument and formats the reply. A
// verb nothing serves is answered as an unknown verb, and the link owns only its own errors and
// diagnostics.
//
// Its whole power over the device is the control it is handed: the grammar parses a line and formats
// a reply, and every decision behind them is the control's, so no link can queue a sequence of its
// own.
class SerialLink {
 public:
  // True while a host has the port open. Null means the stream cannot tell (a plain UART), in which
  // case a lost link is simply never reported.
  using PresenceFn = bool (*)();

  SerialLink(Stream& stream, PresenceFn present, Logger& logger, SystemControl& control);

  void begin();
  void poll(uint32_t nowMs);

 private:
  bool controllerAttached() const;

  // The machine's commands: one line, already trimmed and bounded, parsed into a verb and its
  // argument.
  void execute(Stream& io, const String& line);
  // The status line: the fields `get` answers with, which the heartbeat repeats. It goes out in one
  // write, because the S3's USB CDC drops a write of its own when its ring is busy and a line built
  // from twenty of them loses whichever ones lost the race.
  void writeStatus(Stream& io, const char* prefix) const;
  // The unsolicited status line, one per HeartbeatPeriodMs of the link's own clock.
  void heartbeat(uint32_t nowMs);
  // The link's own errors: an unknown verb, or a line it could not accept.
  void linkError(const char* code, const String& detail);

  Stream& stream_;
  PresenceFn present_;
  Logger& logger_;
  SystemControl& control_;
  // The line being assembled, without its LF.
  String buffer_;
  // Set when a line outgrew CommandLineMax: the rest of it is dropped and it is answered once.
  bool overlong_ = false;
  bool attached_ = false;
  // When the heartbeat last went out, in millis.
  uint32_t lastHeartbeatMs_ = 0;
};

}  // namespace gardener