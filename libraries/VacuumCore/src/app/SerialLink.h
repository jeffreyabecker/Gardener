#pragma once

#include <Arduino.h>

#include "core/Config.h"
#include "core/Snapshot.h"
#include "core/Text.h"

namespace eggplan {

class PressureSensor;
class Pump;
class VacuumControl;

// The serial interface to the controller: the bytes, the command grammar and the payload
// text, in one place because they are three faces of one protocol. On a wire the byte half is
// available/read/write — three calls — so splitting it from the grammar the way a transport
// with tasks, callbacks and MTU negotiation needed to would buy nothing.
//
// Everything runs on the main loop: lines are assembled here and dispatched inline, and
// replies go straight back out. There is no second task, no queue, and no framing above the
// line. Diagnostics share the stream, marked with '[' so a controller can tell a record
// from a reply.
//
// The stream and the presence check are injected rather than assumed, so pointing the
// interface at UART0 instead of USB CDC is a one line change in App.
class SerialLink {
 public:
  // True while a host has the port open. Null means the stream cannot tell (a plain UART),
  // in which case a lost link is simply never reported.
  using PresenceFn = bool (*)();

  SerialLink(Pump& pump, PressureSensor& sensor, VacuumControl& control, Stream& stream,
             PresenceFn present, const SystemSnapshot& snapshot);

  void begin();
  void poll(uint32_t nowMs);

  bool controllerAttached() const;
  // One shot: the controller went away. App decides what that means (stop and vent); the
  // interface only reports it.
  bool consumeLinkLost();

  // Diagnostics, through the interface that owns the stream, which is what makes
  // `set log off` total.
  void info(const char* text);
  void warn(const char* text);
  void error(const char* text);

 private:
  // --- bytes ---------------------------------------------------------------
  void handleLine();
  void logLine(char level, const char* text);
  void writeLine(const char* prefix, const char* text);

  // --- grammar -------------------------------------------------------------
  void dispatch(char* line);
  void emitHelp();
  void ok(const char* payload);
  void err(const char* code);

  // --- payload text --------------------------------------------------------
  size_t statusBody(char* out, size_t size) const;
  size_t infoBody(char* out, size_t size) const;

  Pump& pump_;
  PressureSensor& sensor_;
  VacuumControl& control_;
  Stream& stream_;
  PresenceFn present_;
  const SystemSnapshot& snapshot_;

  bool logEnabled_ = true;
  char line_[cfg::COMMAND_LINE_MAX];
  size_t length_ = 0;
  bool oversize_ = false;  // the current line is too long and was answered already
  bool attached_ = false;
  bool linkLost_ = false;
  char body_[224];
};

}  // namespace eggplan
