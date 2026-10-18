#pragma once

#include <Arduino.h>

#include "app/Logger.h"
#include "app/serial/Items.h"
#include "app/serial/LineGrammar.h"
#include "app/serial/SerialSurface.h"
#include "core/Config.h"

namespace gardener {

class AppContext;

// The serial interface to the controller: the bytes, the dispatch, the replies and the diagnostics.
// One protocol over several files — the line grammar in app/serial/LineGrammar.h, the wire surface as
// data in app/serial/SerialSurface.h, the payload text in app/serial/SerialPayload.cpp, the discovery
// pages in app/serial/SerialHelp.cpp, the diagnostics in app/serial/Log.h — because text is what makes
// a single file hard to read. The byte half stays here: on a wire it is available/read/write, three
// calls, and a transport split would need tasks, callbacks and an MTU before it bought anything.
//
// A command is a resource and an op: `vacuum target=40`, `device reboot`, `link term=off`. The
// first token says what is being touched and the second says what to do with it, so no verb has to
// cover a vacuum-system actuator, a framing setting and a diagnostics switch at once, and no
// actuator is a bare verb while its neighbour is a `set`. Everything after the op belongs to the
// op: a write carries several fields, and an action takes no argument at all.
//
// Every reply payload is `key=value` items and nothing else: an action reports `result=<action>`
// and the fields it changed, a read reports its fields, and a write reports the field the way a
// read would. So every field a host can write it can also read, and every refusal carries the
// field, value, bound, reason or mode that caused it rather than leaving it in a log a client may
// have turned off.
//
// It reads the machine through the AppContext's accessors when it formats a reply, so there is no
// second copy of the state to keep in step.
//
// Everything runs on the main loop: lines are assembled here and dispatched inline, and replies go
// straight back out. There is no second task, no queue, and no framing above the line. Diagnostics
// share the stream, marked with '[' so a controller can tell a record from a reply.
//
// The end of a line is a string, not a fixed byte: `link term` can point it at a marker rather than
// newline. Replies are always newline terminated.
//
// The stream and the presence check are injected rather than assumed, so pointing the interface at
// UART0 instead of USB CDC is a one line change in App.
class SerialLink : public Logger {
 public:
  // True while a host has the port open. Null means the stream cannot tell (a plain UART), in which
  // case a lost link is simply never reported.
  using PresenceFn = bool (*)();

  SerialLink(AppContext* context, Stream& stream, PresenceFn present);

  void begin();
  void poll(uint32_t nowMs);

  bool controllerAttached() const;
  // One shot: the controller went away. App decides what that means (stop and vent); the interface
  // only reports it.
  bool consumeLinkLost();

  // --- Logger --------------------------------------------------------------
  // Diagnostics, through the interface that owns the stream, which is what makes `device log=off`
  // total.
  void info(const char* text) override;
  void warn(const char* text) override;
  void error(const char* text) override;

 private:
  // --- bytes ---------------------------------------------------------------
  void drainLines();
  void dispatchLine(const String& line);
  void logLine(char level, const char* text);
  void writeLine(const char* prefix, const char* text);

  // --- the command surface -------------------------------------------------
  // The resource picks a table from SerialPayload.cpp and the op is one of its fields, one of its
  // actions, a write, or `get`.
  void command(Tokens& tokens);
  void resourceCommand(const ResourceSpec& resource, Tokens& tokens);
  // The whole surface, as data, in the order `help` lists it. Defined in app/serial/SerialPayload.cpp.
  static const ResourceSpec* resources(uint8_t& count);
  static const FieldSpec* findField(const ResourceSpec& resource, const char* name);
  void renderFields(const ResourceSpec& resource, Items& out) const;
  // One field of one resource by name, which is how an action reports what it changed without a
  // second formatter for it.
  void renderField(const char* resource, const char* field, Items& out) const;
  WireView wireView() const;

  // A writer applies every field of one write, or refuses the whole command: false means it has
  // already answered with the error for the first field it would not take, and nothing was applied.
  // Which fields a resource has is the table's business; that they are writable is checked before
  // any writer runs, so a writer only has to know its own.
  bool writeDevice(const FieldWrite* writes, uint8_t count);
  bool writeVacuum(const FieldWrite* writes, uint8_t count);
  bool writeLink(const FieldWrite* writes, uint8_t count);

  // --- actions -------------------------------------------------------------
  void deviceReboot();
  void vacuumStart();
  void vacuumStop();
  void vacuumReset();

  // --- replies -------------------------------------------------------------
  void ok(const String& items);
  void err(const char* code);
  void err(const char* code, const String& items);
  void okResult(const char* action);
  void errUnknown(const char* token);
  void errBadArgument(const char* token);
  void errUnsupported(const char* field);
  void errOutOfRange(const char* field, const char* value, const float* min, const float* max);
  // A row of a page, and the line that closes one: `end` then the same items as the page's `ok`.
  void row(const String& items);
  void endPage(const String& headerItems);

  // --- framing -------------------------------------------------------------
  // Applies a marker, or answers with the error. It does not reply on success: the echo of a
  // `link term=` write is the field read back, like every other write.
  bool applyTerminator(const char* token);

  // --- the help pages ------------------------------------------------------
  void help(Tokens& tokens);
  void emitResourceList();
  void emitResourcePage(const ResourceSpec& resource);

  AppContext* context_;
  Stream& stream_;
  PresenceFn present_;

  bool logEnabled_ = true;
  // Where the device looks for the end of a command. Newline by default; `link term` points it at a
  // marker instead, which is how a whole multi-line payload arrives as one command.
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