// The serial link's framing: which bytes end a domain token, what a line the link cannot dispatch
// answers, and what happens to a half-read token when the controller goes away. The bench is the
// real surface over fakes — a short reply is enough to see that the body reached the grammar; the
// machine's own commands are in vacuum_commands.cpp.

#include <doctest.h>

#include <Arduino.h>

#include <string>
#include <vector>

#include "Clock.h"
#include "TextStream.h"
#include "fakes/Parts.h"
#include "machine/CommandQueue.h"
#include "machine/Machine.h"
#include "machine/SystemControl.h"
#include "serial/SerialLink.h"

namespace {

using gardener::CommandQueue;
using gardener::Machine;
using gardener::MachineCommand;
using gardener::SerialLink;
using gardener::SystemControl;
using gardener::host::TextStream;
using gardener::test::FakePump;
using gardener::test::FakeSensor;
using gardener::test::FakeValve;
using gardener::test::RecordingLogger;

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

// One link over the fakes, with the machine's own grammar as the reply a framing case reads.
struct Bench {
  Bench()
      : machine(logger, &pump, &sensor, &valve), control(machine.vacuum(), queue),
        link(io, &presentFn, logger, control) {
    gardener::host::reset();
    machine.boot();
    link.begin();
  }

  // Types a line and runs one poll, which is what a link pass with a whole command in the buffer
  // does.
  void send(const char* text) {
    io.type(text);
    link.poll(gardener::host::nowMs());
  }

  FakePump pump;
  FakeSensor sensor;
  FakeValve valve;
  RecordingLogger logger;
  OpenQueue queue;
  Machine machine;
  SystemControl control;
  TextStream io;
  SerialLink link;
};

}  // namespace

TEST_CASE("a lowercase domain dispatches") {
  Bench bench;
  bench.send("vacuum vent\n");
  CHECK_EQ(bench.io.printed(), std::string("ok state=VENT\n"));
}

TEST_CASE("an uppercase domain dispatches") {
  Bench bench;
  bench.send("VACUUM vent\n");
  CHECK_EQ(bench.io.printed(), std::string("ok state=VENT\n"));
}

TEST_CASE("a mixed case domain dispatches") {
  Bench bench;
  bench.send("VaCuUm vent\n");
  CHECK_EQ(bench.io.printed(), std::string("ok state=VENT\n"));
}

TEST_CASE("a domain token with nothing after it is a bad argument") {
  Bench bench;
  bench.send("vacuum\n");
  CHECK_EQ(bench.io.printed(), std::string("err bad_argument\n"));
}

TEST_CASE("a comment answers nothing and the next line still dispatches") {
  Bench bench;
  bench.send("# a note about the line below\nvacuum vent\n");
  CHECK_EQ(bench.io.printed(), std::string("ok state=VENT\n"));
}

TEST_CASE("a tab ends the domain token") {
  Bench bench;
  bench.send("vacuum\tvent\n");
  CHECK_EQ(bench.io.printed(), std::string("ok state=VENT\n"));
}

TEST_CASE("a CRLF line leaves no carriage return in the body") {
  Bench bench;
  bench.send("vacuum vent\r\n");
  CHECK_EQ(bench.io.printed(), std::string("ok state=VENT\n"));
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

TEST_CASE("a domain's body reaches its grammar intact") {
  Bench bench;
  bench.sensor.read(12.0f);
  bench.send("vacuum set 40\n");
  CHECK_EQ(bench.io.printed(), std::string("ok target=40.0 state=PULL\n"));
}

TEST_CASE("a lost controller is reported and a half-read token is dropped") {
  Bench bench;
  controllerPresent = true;
  bench.io.type("vac");
  bench.link.poll(gardener::host::nowMs());

  controllerPresent = false;
  bench.link.poll(gardener::host::nowMs());
  CHECK(bench.logger.saw("serial: controller detached"));

  controllerPresent = true;
  bench.send("uum get\n");
  CHECK_EQ(bench.io.printed(), std::string("err unknown_domain uum\n"));
}
