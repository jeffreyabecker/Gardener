#pragma once

#include <Arduino.h>

#include "machine/CommandQueue.h"
#include "machine/MachineCommand.h"
#include "serial/BoardFacts.h"

namespace gardener {
inline namespace core {
class Battery;
class Library;
class Logger;
}
inline namespace machine {
class Machine;
}
inline namespace storage {
class DeviceSettings;
}

namespace panel::parts {
class BuzzerPlayer;
}  // namespace panel::parts

inline namespace serial {

// The longest command line a grammar serves: the link refuses a longer one, and so does each grammar
// handed one. A command body, when a grammar defines one, is not bounded here.
constexpr uint16_t COMMAND_LINE_MAX = 128;

// The wire revision this link serves, reported as `proto` by `device get`. The wire is still
// unsettled, so the number stays 1 and does not move per revision; a controller must not treat it
// as a compatibility guarantee.
constexpr uint32_t PROTOCOL_VERSION = 1;

// Everything the wire answers from: the machine, the board's own facts, and the panel's parts — the
// gauge, the document, the piezo and the store the files ride on. The sketch builds it; the link
// names no part of its own.
struct SerialParts {
  Machine& machine;
  const BoardFacts& board;
  const Battery& battery;
  DeviceSettings& settings;
  panel::parts::BuzzerPlayer& buzzer;
  Library& files;
};

// The serial interface to the controller. It reads the first token of each line and hands the rest of
// the line, body included, to that token's grammar: `vacuum`, `device`, `buzzer` and `fs`, one file
// each beside this one. A token nothing serves is answered as an unknown domain, and the link owns
// only its own errors and diagnostics.
class SerialLink {
 public:
  // True while a host has the port open. Null means the stream cannot tell (a plain UART), in which
  // case a lost link is simply never reported.
  using PresenceFn = bool (*)();

  SerialLink(Stream& stream, PresenceFn present, Logger& logger, CommandQueue& queue,
             SerialParts parts);

  void begin();
  void poll(uint32_t nowMs);

  bool controllerAttached() const;

 private:
  // The port as a grammar sees it: the bytes the link reads, with a CR the LF follows taken off. The
  // port's terminator is LF alone, so "\r\n" ends a line the way "\n" does and no grammar has to
  // know which one the host wrote.
  class Port : public Stream {
   public:
    explicit Port(Stream& port) : port_(port) {}

    int available() override { return port_.available(); }
    // One byte of lookahead, so the CR is dropped only when the LF is already behind it.
    int read() override {
      int value = port_.read();
      if (value == '\r' && port_.peek() == '\n') value = port_.read();
      return value;
    }
    int peek() override { return port_.peek(); }
    void flush() override { port_.flush(); }
    size_t write(uint8_t byte) override { return port_.write(byte); }
    size_t write(const uint8_t* buffer, size_t size) override { return port_.write(buffer, size); }

   private:
    Stream& port_;
  };

  // The grammars this link serves, resolved from a line's first token by the one list in
  // `grammarFor`.
  enum class Grammar : uint8_t { None, Vacuum, Device, Buzzer, Files };

  // The grammar `token` names, which must already be lowercase.
  Grammar grammarFor(const String& token) const;
  // Hands the stream to the grammar named by `token`. False when there is no such grammar, in which
  // case the caller discards the rest of the line.
  bool dispatch(const String& token);
  // Each grammar: the rest of the line, and any body, is read from the stream it is handed.
  void executeVacuum(Stream& io);
  void executeDevice(Stream& io);
  void executeBuzzer(Stream& io);
  void executeFiles(Stream& io);
  // The dispatcher's own errors: an unknown domain, or a header it could not accept.
  void linkError(const char* code, const String& detail);

  CommandOutbox outbox_;
  Stream& stream_;
  // The stream the grammars read and answer on: `stream_` with the line ending's CR taken off.
  Port port_;
  PresenceFn present_;
  Logger& logger_;
  SerialParts parts_;
  // The domain token being assembled. It never holds a command or a body.
  String buffer_;
  // The rest of a line that was already answered, or a comment, is being thrown away.
  bool swallowing_ = false;
  bool attached_ = false;
};

}  // namespace serial
}  // namespace gardener
