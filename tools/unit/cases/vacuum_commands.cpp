// The machine's `vacuum` commands: the field list `get` publishes, the bounds `set` holds its
// argument to, the run a set queues, and the refusals it answers. They run over the machine's real
// VacuumSystem with the parts faked, because the answers are the loop's state.

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
#include "panel/GestureQueue.h"
#include "panel/parts/BuzzerPlayer.h"
#include "serial/BoardFacts.h"
#include "serial/SerialLink.h"
#include "storage/DeviceSettings.h"

namespace {

using gardener::BoardFacts;
using gardener::BuzzerPlayer;
using gardener::CommandQueue;
using gardener::DeviceSettings;
using gardener::GestureQueue;
using gardener::Machine;
using gardener::MachineCommand;
using gardener::SerialLink;
using gardener::SystemState;
using gardener::host::TextStream;
using gardener::test::FakeBattery;
using gardener::test::FakeBuzzer;
using gardener::test::FakePump;
using gardener::test::FakeSensor;
using gardener::test::FakeValve;
using gardener::test::MemoryLibrary;
using gardener::test::RecordingLogger;

// The board's facts, as the sketch's own pair answer them.
const char* chipModel() { return "ESP32-S3"; }
uint32_t heapFacts() { return 123456; }

// The queue the link posts into: everything fits, so a case reads back what the grammar queued.
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

  bool empty() const { return commands_.empty(); }
  size_t count() const { return commands_.size(); }

 private:
  std::vector<MachineCommand> commands_;
};

// The machine the wire is built over, one per case, with a clock that starts at zero. A case that
// needs a document's maximum hands one in; without one the device keeps its shipped fifteen minutes.
struct Bench {
  explicit Bench(const char* settingsDocument = nullptr)
      : machine(logger, &pump, &sensor, &valve), settings(&library, state), player(buzzerPart),
        link(io, nullptr, logger, queue,
             gardener::SerialParts{machine, board, battery, settings, player, library, presses}) {
    if (settingsDocument != nullptr) library.put("/cfg/settings.ini", settingsDocument);
    settings.begin();
    gardener::host::reset();
    machine.boot();
    link.begin();
  }

  void send(const char* text) {
    io.clearPrinted();
    io.type(text);
    link.poll(gardener::host::nowMs());
  }

  std::string reply() const { return io.printed(); }

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
  GestureQueue presses;
  DeviceSettings settings;
  FakeBuzzer buzzerPart;
  BuzzerPlayer player;
  const BoardFacts board{&chipModel, &heapFacts};
  SerialLink link;
};

bool contains(const std::string& text, const char* part) {
  return text.find(part) != std::string::npos;
}

}  // namespace

TEST_CASE("get answers the pressure to two decimals and the rest to one") {
  Bench bench;
  bench.send("vacuum get\n");
  const std::string reply = bench.reply();
  CHECK(contains(reply, "ok pressure_kpa=0.00 "));
  CHECK(contains(reply, " target=0.0 "));
  CHECK(contains(reply, " duty=0.0 "));
}

TEST_CASE("get answers the state and the fault") {
  Bench bench;
  bench.send("vacuum get\n");
  const std::string reply = bench.reply();
  CHECK(contains(reply, " state="));
  CHECK(contains(reply, " fault=none\n"));
}

TEST_CASE("a set inside the range queues the target and a start") {
  Bench bench;
  bench.sensor.read(12.0f);
  bench.send("vacuum set 5\n");
  CHECK_EQ(bench.reply(), std::string("ok target=5.0 state=PULL\n"));

  MachineCommand command{MachineCommand::Op::Stop};
  REQUIRE(bench.queue.take(command));
  CHECK(command.op == MachineCommand::Op::SetTarget);
  CHECK_EQ(command.value, 5.0f);
  REQUIRE(bench.queue.take(command));
  CHECK(command.op == MachineCommand::Op::Start);
  CHECK(bench.queue.empty());
}

TEST_CASE("the ambient target is zero and the shorthand dash, and both release") {
  Bench bench;
  bench.send("vacuum set 0\n");
  CHECK_EQ(bench.reply(), std::string("ok target=0.0 state=VENT\n"));
  bench.send("vacuum set -\n");
  CHECK_EQ(bench.reply(), std::string("ok target=0.0 state=VENT\n"));

  MachineCommand command{MachineCommand::Op::Start};
  REQUIRE(bench.queue.take(command));
  CHECK(command.op == MachineCommand::Op::SetTarget);
  CHECK_EQ(command.value, 0.0f);
  REQUIRE(bench.queue.take(command));
  CHECK(command.op == MachineCommand::Op::Stop);
}

TEST_CASE("the floor itself is an accepted target") {
  Bench bench;
  bench.sensor.read(12.0f);
  bench.send("vacuum set 60\n");
  CHECK_EQ(bench.reply(), std::string("ok target=60.0 state=PULL\n"));
}

TEST_CASE("a target below ambient is refused with its minimum") {
  Bench bench;
  bench.send("vacuum set -1\n");
  CHECK_EQ(bench.reply(), std::string("err out_of_range field=target value=-1 min=0.0\n"));
  CHECK(bench.queue.empty());
}

TEST_CASE("a target that is not a number is refused with its minimum") {
  Bench bench;
  bench.send("vacuum set nan\n");
  CHECK_EQ(bench.reply(), std::string("err out_of_range field=target value=nan min=0.0\n"));
}

TEST_CASE("a target past the floor is refused with its maximum") {
  Bench bench;
  bench.send("vacuum set 60.1\n");
  CHECK_EQ(bench.reply(), std::string("err out_of_range field=target value=60.1 max=60.0\n"));
}

TEST_CASE("a second target token is refused by name") {
  Bench bench;
  bench.send("vacuum set 5 6\n");
  CHECK_EQ(bench.reply(), std::string("err bad_argument 6\n"));
  CHECK(bench.queue.empty());
}

TEST_CASE("a set with no reading refuses and queues nothing") {
  Bench bench;
  bench.send("vacuum set 40\n");
  CHECK_EQ(bench.reply(), std::string("err not_ready no_reading\n"));
  CHECK(bench.queue.empty());
}

TEST_CASE("a latched fault refuses a set") {
  Bench bench;
  bench.sensor.read(12.0f);
  bench.machine.vacuum().setTarget(40.0f);
  bench.machine.vacuum().start();
  bench.machine.vacuum().service(gardener::host::nowMs());
  bench.sensor.detach();
  gardener::host::advance(50);
  bench.machine.vacuum().service(gardener::host::nowMs());
  REQUIRE_EQ(std::string(bench.machine.vacuum().faultCode()), std::string("sensor"));

  bench.send("vacuum set 40\n");
  CHECK_EQ(bench.reply(), std::string("err not_ready latch\n"));
}

TEST_CASE("a set asks for the machine's ceiling rather than a document's") {
  Bench bench("[device]\nmax_duration=600\n");
  bench.sensor.read(12.0f);

  bench.send("vacuum set 40\n");
  CHECK_EQ(bench.reply(), std::string("ok target=40.0 state=PULL\n"));

  MachineCommand command{MachineCommand::Op::Stop};
  REQUIRE(bench.queue.take(command));
  REQUIRE(bench.queue.take(command));
  CHECK(command.op == MachineCommand::Op::Start);
  // Zero is the machine's own "give me the ceiling"; the document's 600 is not consulted.
  CHECK_EQ(command.value, 0.0f);
  CHECK(bench.queue.empty());
}

TEST_CASE("vent answers the release it queues") {
  Bench bench;
  bench.send("vacuum vent\n");
  CHECK_EQ(bench.reply(), std::string("ok state=VENT\n"));
  CHECK_FALSE(bench.queue.empty());
}

TEST_CASE("reset answers the idle state it queues") {
  Bench bench;
  bench.send("vacuum reset\n");
  CHECK_EQ(bench.reply(), std::string("ok state=IDLE\n"));
  CHECK_FALSE(bench.queue.empty());
}

TEST_CASE("a refused set leaves the queue empty and a queued one fills it") {
  Bench bench;
  bench.send("vacuum set 40\n");
  CHECK(bench.queue.empty());
  bench.sensor.read(12.0f);
  bench.send("vacuum set 40\n");
  CHECK_EQ(bench.queue.count(), static_cast<size_t>(2));
}
