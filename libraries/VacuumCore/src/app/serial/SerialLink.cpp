#include "app/serial/SerialLink.h"

#include <string.h>

#include "app/AppContext.h"
#include "app/serial/Domains.h"
#include "core/Config.h"

namespace gardener {
namespace {

// The receive buffer and the port behind one Stream. A domain reads the rest of its command from
// here; the buffer is served first, so a complete header never waits on the port. The timeout is
// zero: a domain must not block the loop waiting for bytes that have not arrived.
class CommandStream : public Stream {
 public:
  CommandStream(String& buffer, Stream& port, size_t start)
      : buffer_(buffer), port_(port), index_(start) {
    setTimeout(0);
  }

  int available() override {
    return (int)(buffer_.length() - index_) + port_.available();
  }

  int read() override {
    if (index_ < buffer_.length()) return (uint8_t)buffer_.charAt((unsigned int)index_++);
    return port_.read();
  }

  int peek() override {
    if (index_ < buffer_.length()) return (uint8_t)buffer_.charAt((unsigned int)index_);
    return port_.peek();
  }

  void flush() override {}

  // Input only. Print's pure virtual is here because Stream inherits it, not because anything
  // writes through a command stream.
  size_t write(uint8_t) override { return 0; }

  size_t position() const { return index_; }

 private:
  String& buffer_;
  Stream& port_;
  size_t index_;
};

String named(const char* key, const char* value) {
  String out;
  addItem(out, key, value);
  return out;
}

}  // namespace

SerialLink::SerialLink(AppContext* context, Stream& stream, PresenceFn present, bool& logEnabled)
    : stream_(stream),
      present_(present),
      logEnabled_(logEnabled),
      domainContext_{*context, stream_, logEnabled} {
  domains_.add(createDeviceDomain(domainContext_));
  domains_.add(createVacuumDomain(domainContext_));
  domains_.add(createBuzzerDomain(domainContext_));
  domains_.add(createHelpDomain(domainContext_, domains_));
}

void SerialLink::begin() {
  attached_ = controllerAttached();
  buffer_ = "";
  swallowing_ = false;
  diag("[I] ", "serial: interface ready");
}

bool SerialLink::controllerAttached() const { return present_ ? present_() : true; }

bool SerialLink::consumeLinkLost() {
  bool lost = linkLost_;
  linkLost_ = false;
  return lost;
}

// --- replies ---------------------------------------------------------------
// The link writes only its own errors and diagnostics. A domain formats its own reply through
// `ctx_.out`, which is what lets the future fs domain send a raw body.

void SerialLink::writeLine(const char* prefix, const char* text) {
  stream_.write((const uint8_t*)prefix, strlen(prefix));
  if (text && *text) stream_.write((const uint8_t*)text, strlen(text));
  stream_.write((uint8_t)'\n');
}

void SerialLink::linkError(const char* code, const String& items) {
  String line = "- - ";
  line += code;
  if (!items.isEmpty()) {
    line += ' ';
    line += items;
  }
  writeLine("err ", line.c_str());
}

void SerialLink::diag(const char* prefix, const char* text) {
  if (logEnabled_) writeLine(prefix, text);
}

// --- bytes -----------------------------------------------------------------

void SerialLink::poll(uint32_t nowMs) {
  (void)nowMs;

  // Take everything the host has sent this pass. There is no per pass line budget: a burst of
  // commands is read whole and drained below.
  while (stream_.available() > 0) {
    int value = stream_.read();
    if (value < 0) break;
    buffer_ += (char)value;
  }

  drainCommands();

  bool attached = controllerAttached();
  if (attached_ && !attached) {
    linkLost_ = true;
    buffer_ = "";
    swallowing_ = false;
    diag("[W] ", "serial: controller detached");
  }
  attached_ = attached;
}

void SerialLink::drainCommands() {
  while (dispatchOne()) {
  }
}

// One command, or no progress. The domain token is all this layer parses; the domain consumes
// everything after it, through its own LF (and body, where it has one).
bool SerialLink::dispatchOne() {
  if (swallowing_) {
    int end = buffer_.indexOf('\n');
    if (end < 0) {
      buffer_ = "";
      return false;
    }
    buffer_.remove(0, (unsigned int)end + 1);
    swallowing_ = false;
    return true;
  }

  int lf = buffer_.indexOf('\n');
  if (lf < 0) {
    if (buffer_.length() > cfg::COMMAND_LINE_MAX) {
      linkError("bad_argument", "");
      buffer_ = "";
      swallowing_ = true;
    }
    return false;
  }

  size_t length = (size_t)lf;
  if (length > 0 && buffer_.charAt((unsigned int)(length - 1)) == '\r') length--;

  if (length > cfg::COMMAND_LINE_MAX) {
    // Answer once and drop it rather than truncating into something that parses as a different
    // command.
    linkError("bad_argument", "");
    buffer_.remove(0, (unsigned int)lf + 1);
    return true;
  }

  size_t first = 0;
  while (first < length && (buffer_.charAt((unsigned int)first) == ' ' ||
                            buffer_.charAt((unsigned int)first) == '\t')) {
    first++;
  }
  if (first >= length || buffer_.charAt((unsigned int)first) == '#') {
    buffer_.remove(0, (unsigned int)lf + 1);
    return true;
  }

  size_t tokenEnd = first;
  while (tokenEnd < length && buffer_.charAt((unsigned int)tokenEnd) != ' ' &&
         buffer_.charAt((unsigned int)tokenEnd) != '\t') {
    tokenEnd++;
  }

  String domainName = buffer_.substring((unsigned int)first, (unsigned int)tokenEnd);
  domainName.toLowerCase();

  Domain* domain = domains_.find(domainName.c_str());
  if (!domain) {
    linkError("unknown_domain", named("token", domainName.c_str()));
    buffer_.remove(0, (unsigned int)lf + 1);
    return true;
  }

  CommandStream input(buffer_, stream_, tokenEnd);
  domain->execute(input);

  // A domain must consume through its LF. The floor keeps a domain that reads nothing from
  // leaving the same header at the front of the buffer forever.
  size_t consumed = input.position();
  if (consumed < (size_t)lf + 1) consumed = (size_t)lf + 1;
  buffer_.remove(0, (unsigned int)consumed);
  return true;
}

}  // namespace gardener
