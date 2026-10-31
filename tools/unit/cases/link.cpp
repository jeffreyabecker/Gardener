// The serial link's framing: which bytes end a domain token, what a line the link cannot dispatch
// answers, and what happens to a half-read token when the controller goes away. The bench is the
// real surface over fakes — a grammar's short reply is enough to see that the body reached it; the
// machine's own commands are in vacuum_commands.cpp and device_commands.cpp.

#include <doctest.h>

#include <Arduino.h>

#include <string>
#include <vector>

#include "Clock.h"
#include "TextStream.h"
#include "core/SystemState.h"
#include "fakes/MemoryLibrary.h"
#include "fakes/Parts.h"
#include "machine/CommandQueue.h"
#include "machine/Machine.h"
#include "panel/parts/BuzzerPlayer.h"
#include "serial/BoardFacts.h"
#include "serial/SerialLink.h"
#include "storage/DeviceSettings.h"

namespace {

using gardener::BoardFacts;
using gardener::CommandQueue;
using gardener::DeviceSettings;
using gardener::Machine;
using gardener::MachineCommand;
using gardener::SerialLink;
using gardener::SystemState;
using gardener::host::TextStream;
using gardener::panel::parts::BuzzerPlayer;
using gardener::test::FakeBattery;
using gardener::test::FakeBuzzer;
using gardener::test::FakePump;
using gardener::test::FakeSensor;
using gardener::test::FakeValve;
using gardener::test::MemoryLibrary;
using gardener::test::RecordingLogger;

bool controllerPresent = true;

bool presentFn() { return controllerPresent; }

// The board's facts, as the sketch's own pair answer them.
const char* chipModel() { return "ESP32-S3"; }
uint32_t heapFacts() { return 123456; }

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

// One link over the fakes, with the piezo's grammar as the short reply a framing case reads.
struct Bench {
  Bench()
      : machine(logger, &pump, &sensor, &valve), settings(&library, state), player(buzzerPart),
        link(io, &presentFn, logger, queue,
             gardener::SerialParts{machine, board, battery, settings, player, library}) {
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
  TextStream io;
  FakeBattery battery;
  SystemState state;
  MemoryLibrary library;
  DeviceSettings settings;
  FakeBuzzer buzzerPart;
  BuzzerPlayer player;
  const BoardFacts board{&chipModel, &heapFacts};
  SerialLink link;
};

}  // namespace

TEST_CASE("a lowercase domain dispatches") {
  Bench bench;
  bench.send("buzzer stop\n");
  CHECK_EQ(bench.io.printed(), std::string("ok playing=off\n"));
}

TEST_CASE("an uppercase domain dispatches") {
  Bench bench;
  bench.send("BUZZER stop\n");
  CHECK_EQ(bench.io.printed(), std::string("ok playing=off\n"));
}

TEST_CASE("a mixed case domain dispatches") {
  Bench bench;
  bench.send("BuZzEr stop\n");
  CHECK_EQ(bench.io.printed(), std::string("ok playing=off\n"));
}

TEST_CASE("a domain token with nothing after it is a bad argument") {
  Bench bench;
  bench.send("vacuum\n");
  CHECK_EQ(bench.io.printed(), std::string("err bad_argument\n"));
}

TEST_CASE("a comment answers nothing and the next line still dispatches") {
  Bench bench;
  bench.send("# a note about the line below\nbuzzer stop\n");
  CHECK_EQ(bench.io.printed(), std::string("ok playing=off\n"));
}

TEST_CASE("a tab ends the domain token") {
  Bench bench;
  bench.send("buzzer\tstop\n");
  CHECK_EQ(bench.io.printed(), std::string("ok playing=off\n"));
}

TEST_CASE("a CRLF line leaves no carriage return in the body") {
  Bench bench;
  bench.send("buzzer stop\r\n");
  CHECK_EQ(bench.io.printed(), std::string("ok playing=off\n"));
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
  bench.send("buzzer tone 1000 50\n");
  CHECK_EQ(bench.io.printed(), std::string("ok tone=1000,50\n"));
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
