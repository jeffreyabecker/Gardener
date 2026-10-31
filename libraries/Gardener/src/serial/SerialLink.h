#pragma once

#include <Arduino.h>
#include <vector>

#include "machine/CommandQueue.h"
#include "machine/MachineCommand.h"
#include "serial/Domain.h"

namespace gardener {
inline namespace core { class Logger; }
inline namespace machine { class Machine; }

// The wire revision this link serves, reported as `proto` by `device get`. The wire is still
// unsettled, so the number stays 1 and does not move per revision; a controller must not treat it
// as a compatibility guarantee.
constexpr uint32_t PROTOCOL_VERSION = 1;

// The serial interface to the controller. It reads the first token of each command, finds that
// domain and hands it the stream at that point; everything after the domain token, including any
// body, belongs to the domain. Replies are the domains' business: this class owns only the link's
// own errors and diagnostics.
class SerialLink {
 public:
  // True while a host has the port open. Null means the stream cannot tell (a plain UART), in which
  // case a lost link is simply never reported.
  using PresenceFn = bool (*)();

  SerialLink(Stream& stream, PresenceFn present, Logger& logger, CommandQueue& queue);

  // The surface is whatever the build registers: the machine adds its domains, and a panel build
  // adds its own, so the link itself names none of them.
  void registerDomain(Domain* domain) { domains_.push_back(domain); }

  // The link's queue, for the domains the build registers.
  CommandOutbox& outbox() { return outbox_; }
  void flushCommands() { outbox_.flush(); }

  void begin();
  void poll(uint32_t nowMs);

  bool controllerAttached() const;

 private:
  // What a build registers: the machine's domains and a panel's, with room to spare.
  static constexpr uint8_t MaxDomains = 8;

  // The port as a domain sees it: the bytes the link reads, with a CR the LF follows taken off. The
  // port's terminator is LF alone, so "\r\n" ends a line the way "\n" does and no domain has to
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

  // Hands the stream to the domain named by `token`. False when there is no such domain, in which
  // case the caller discards the rest of the line.
  bool dispatchDomain(const String& token);
  // The domain registered under `name`, in registration order.
  Domain* findDomain(const char* name) const;
  // The dispatcher's own errors: an unknown domain, or a header it could not accept.
  void linkError(const char* code, const String& detail);

  CommandOutbox outbox_;
  Stream& stream_;
  // The stream the domains read and answer on: `stream_` with the line ending's CR taken off.
  Port port_;
  PresenceFn present_;
  Logger& logger_;
  std::vector<Domain*> domains_;
  // The domain token being assembled. It never holds a command or a body.
  String buffer_;
  // The rest of a line that was already answered, or a comment, is being thrown away.
  bool swallowing_ = false;
  bool attached_ = false;
};

}  // namespace gardener
