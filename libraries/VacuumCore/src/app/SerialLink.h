#pragma once

#include <Arduino.h>

#include "core/Config.h"
#include "core/Snapshot.h"
#include "hal/Display.h"

namespace gardener {

class VacuumSystem;
class Tokens;

// The serial interface to the controller: the bytes, the command grammar and the payload
// text, in one place because they are three faces of one protocol. On a wire the byte half is
// available/read/write — three calls — so splitting it from the grammar the way a transport
// with tasks, callbacks and MTU negotiation needed to would buy nothing.
//
// It is also the display driver: the host draws the panel with `draw` commands and puts a frame
// on the glass with `flush`. The display it holds is the overlay, so every flush composites the
// battery on the way out and a host cannot erase it.
//
// Everything runs on the main loop: lines are assembled here and dispatched inline, and
// replies go straight back out. There is no second task, no queue, and no framing above the
// line. Diagnostics share the stream, marked with '[' so a controller can tell a record
// from a reply.
//
// The end of a line is a string, not a fixed byte: `set term` can point it at a marker, which
// is how a whole multi-line payload — a program's source — arrives as a single command rather
// than in chunks. Replies are always newline terminated.
//
// The stream and the presence check are injected rather than assumed, so pointing the
// interface at UART0 instead of USB CDC is a one line change in App.
class SerialLink {
 public:
  // True while a host has the port open. Null means the stream cannot tell (a plain UART),
  // in which case a lost link is simply never reported.
  using PresenceFn = bool (*)();

  SerialLink(VacuumSystem& system, Display& display, Stream& stream, PresenceFn present,
             const SystemSnapshot& snapshot);

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
  void drainLines();
  void dispatchLine(const String& line);
  void logLine(char level, const char* text);
  void writeLine(const char* prefix, const char* text);

  // --- grammar -------------------------------------------------------------
  void dispatch(char* line);
  void setTerminator(const char* token);
  void emitHelp();
  void ok(const char* payload);
  void err(const char* code);

  // --- drawing -------------------------------------------------------------
  // The op, then the numbers it takes, then the rest of the line for the one op that draws a
  // string. Split lazily rather than up front, because a string can contain spaces.
  void draw(Tokens& tokens);
  bool coords(Tokens& tokens, size_t count, int16_t* out);
  // Reads `count` coordinates, refuses anything left over, calls, and answers. The reply logic
  // lives here so seventeen ops cannot each get it slightly wrong.
  template <typename Fn>
  void apply(Tokens& tokens, size_t count, Fn call) {
    int16_t args[6];  // the widest op is a triangle
    if (!coords(tokens, count, args) || tokens.rest() != nullptr) {
      err("bad_argument");
      return;
    }
    call(display_, args);
    ok("drawn");
  }

  // --- payload text --------------------------------------------------------
  String statusBody() const;
  String infoBody() const;

  VacuumSystem& system_;
  Display& display_;
  Stream& stream_;
  PresenceFn present_;
  const SystemSnapshot& snapshot_;

  bool logEnabled_ = true;
  // Where the device looks for the end of a command. Newline by default; `set term` points it
  // at a marker instead, which is how a whole multi-line payload arrives as one command.
  String terminator_ = cfg::LINE_TERMINATOR;
  bool customTerminator_ = false;
  // Everything the host has sent that has not been dispatched yet: complete lines still to be
  // serviced, then at most one line that has not been terminated. Bytes go in, lines come out.
  String buffer_;
  // An overlong line was answered already and the rest of it is being thrown away.
  bool swallowing_ = false;
  bool attached_ = false;
  bool linkLost_ = false;
};

}  // namespace gardener
