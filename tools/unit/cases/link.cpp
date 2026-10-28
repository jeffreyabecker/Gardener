// The serial link's framing: which bytes end a domain token, what a line the link cannot dispatch
// answers, and what happens to a half-read token when the controller goes away. The domains are
// fakes here; the machine's own are in vacuum_domain.cpp and device_domain.cpp.

#include <doctest.h>

#include <Arduino.h>

#include <string>
#include <vector>

#include "TextStream.h"
#include "fakes/Parts.h"
#include "machine/CommandQueue.h"
#include "serial/Domain.h"
#include "serial/SerialLink.h"

namespace {

using gardener::CommandOutbox;
using gardener::CommandQueue;
using gardener::Domain;
using gardener::MachineCommand;
using gardener::test::RecordingLogger;
using gardener::SerialLink;
using gardener::host::TextStream;

bool controllerPresent = true;

bool presentFn() { return controllerPresent; }

// The queue the link posts into: everything fits, and a case can see what was posted.
class OpenQueue : public CommandQueue {
 public:
  bool post(const MachineCommand& command) override {
    commands_.push_back(command);
    return true;
  }

  bool take(MachineCommand& command) override {
    if (commands_.empty()) return false;
    command = commands_.front();
    commands_.erase(commands_.begin());
    return true;
  }

 private:
  std::vector<MachineCommand> commands_;
};

// A domain that answers with the body it was handed, so a case can see exactly what reached
// execute().
class EchoDomain : public Domain {
 public:
  EchoDomain(const char* name, CommandOutbox& outbox) : Domain(outbox), name_(name) {}

  const char* name() const override { return name_; }

  void execute(Stream& io) override {
    const String body = io.readStringUntil('\n');
    body_ = body.c_str();
    io.print("ok ");
    io.print(body);
    io.print('\n');
  }

  const std::string& body() const { return body_; }

 private:
  const char* name_;
  std::string body_;
};

// One link with two domains over a stream the case types into.
struct Bench {
  Bench() : link(io, &presentFn, logger, queue) {
    link.begin();
    vacuum = new EchoDomain("vacuum", link.outbox());
    device = new EchoDomain("device", link.outbox());
    link.registerDomain(vacuum);
    link.registerDomain(device);
  }

  // Types a line and runs one poll, which is what a link pass with a whole command in the buffer
  // does.
  void send(const char* text) {
    io.type(text);
    link.poll(0);
  }

  TextStream io;
  RecordingLogger logger;
  OpenQueue queue;
  SerialLink link;
  EchoDomain* vacuum = nullptr;
  EchoDomain* device = nullptr;
};

}  // namespace

TEST_CASE("a lowercase domain dispatches") {
  Bench bench;
  bench.send("vacuum get\n");
  CHECK_EQ(bench.io.printed(), std::string("ok get\n"));
}

TEST_CASE("an uppercase domain dispatches") {
  Bench bench;
  bench.send("VACUUM get\n");
  CHECK_EQ(bench.io.printed(), std::string("ok get\n"));
}

TEST_CASE("a mixed case domain dispatches") {
  Bench bench;
  bench.send("VaCuUm get\n");
  CHECK_EQ(bench.io.printed(), std::string("ok get\n"));
}

TEST_CASE("a domain token with nothing after it is a bad argument") {
  Bench bench;
  bench.send("vacuum\n");
  CHECK_EQ(bench.io.printed(), std::string("err bad_argument\n"));
}

TEST_CASE("a comment answers nothing and the next line still dispatches") {
  Bench bench;
  bench.send("# a note about the line below\nvacuum get\n");
  CHECK_EQ(bench.io.printed(), std::string("ok get\n"));
}

TEST_CASE("a tab ends the domain token") {
  Bench bench;
  bench.send("vacuum\tget\n");
  CHECK_EQ(bench.io.printed(), std::string("ok get\n"));
}

TEST_CASE("a CRLF line leaves no carriage return in the body") {
  Bench bench;
  bench.send("vacuum get\r\n");
  CHECK_EQ(bench.io.printed(), std::string("ok get\n"));
  CHECK_EQ(bench.vacuum->body(), std::string("get"));
}

TEST_CASE("a token longer than the line maximum is refused") {
  Bench bench;
  std::string line(129, 'v');
  line += "\n";
  bench.send(line.c_str());
  CHECK_EQ(bench.io.printed(), std::string("err bad_argument\n"));
}

TEST_CASE("an unknown domain names the token it could not dispatch") {
  Bench bench;
  bench.send("Bogus get\n");
  CHECK_EQ(bench.io.printed(), std::string("err unknown_domain bogus\n"));
}

TEST_CASE("a domain's body reaches execute intact") {
  Bench bench;
  bench.send("vacuum set target=5 hold=60\n");
  CHECK_EQ(bench.vacuum->body(), std::string("set target=5 hold=60"));
}

TEST_CASE("a lost controller is reported and a half-read token is dropped") {
  Bench bench;
  controllerPresent = true;
  bench.io.type("vac");
  bench.link.poll(0);

  controllerPresent = false;
  bench.link.poll(0);
  CHECK(bench.logger.saw("serial: controller detached"));

  controllerPresent = true;
  bench.send("uum get\n");
  CHECK_EQ(bench.io.printed(), std::string("err unknown_domain uum\n"));
}
