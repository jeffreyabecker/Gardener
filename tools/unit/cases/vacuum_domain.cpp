// The machine's `vacuum` domain: the field list `get` publishes, the bounds `target` and `start`
// hold their arguments to, and the refusals `start` answers. It runs over the machine's real
// VacuumSystem with the parts faked, because the domain's answers are the loop's state.

#include <doctest.h>

#include <Arduino.h>

#include <string>
#include <vector>

#include "Clock.h"
#include "TextStream.h"
#include "fakes/Parts.h"
#include "machine/CommandQueue.h"
#include "machine/Machine.h"
#include "serial/Domains.h"
#include "serial/SerialLink.h"

namespace {

using gardener::CommandQueue;
using gardener::Domain;
using gardener::Machine;
using gardener::MachineCommand;
using gardener::test::RecordingLogger;
using gardener::SerialLink;
using gardener::host::TextStream;
using gardener::test::FakePump;
using gardener::test::FakeSensor;
using gardener::test::FakeValve;

// The queue the link posts into: everything fits, so a case reads back what the domain queued.
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

// The machine the wire is built over, one per case, with a clock that starts at zero.
struct Bench {
  Bench() : machine(logger, &pump, &sensor, &valve), link(io, nullptr, logger, queue) {
    gardener::host::reset();
    machine.boot();
    link.registerDomain(gardener::createVacuumDomain(machine, link.outbox()));
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
  CHECK(contains(reply, " floor=60.0 "));
  CHECK(contains(reply, " duty=0.0 "));
}

TEST_CASE("get answers the state and the parts' health") {
  Bench bench;
  bench.send("vacuum get\n");
  const std::string reply = bench.reply();
  CHECK(contains(reply, " state="));
  CHECK(contains(reply, " sensor=1 "));
  CHECK(contains(reply, " fault=none "));
}

TEST_CASE("get answers the hold, the ceiling and the uptime") {
  Bench bench;
  bench.send("vacuum get\n");
  const std::string reply = bench.reply();
  CHECK(contains(reply, " hold=0 "));
  CHECK(contains(reply, " max_run=900 "));
  CHECK(contains(reply, " up=0\n"));
}

TEST_CASE("a target inside the range queues and answers its depth") {
  Bench bench;
  bench.send("vacuum target 5\n");
  CHECK_EQ(bench.reply(), std::string("ok target=5.0\n"));
  CHECK_EQ(bench.queue.count(), static_cast<size_t>(1));
}

TEST_CASE("the ambient target is zero and the shorthand dash") {
  Bench bench;
  bench.send("vacuum target 0\n");
  CHECK_EQ(bench.reply(), std::string("ok target=0.0\n"));
  bench.send("vacuum target -\n");
  CHECK_EQ(bench.reply(), std::string("ok target=0.0\n"));
}

TEST_CASE("the floor itself is an accepted target") {
  Bench bench;
  bench.send("vacuum target 60\n");
  CHECK_EQ(bench.reply(), std::string("ok target=60.0\n"));
}

TEST_CASE("a target below ambient is refused with its minimum") {
  Bench bench;
  bench.send("vacuum target -1\n");
  CHECK_EQ(bench.reply(), std::string("err out_of_range field=target value=-1 min=0.0\n"));
}

TEST_CASE("a target that is not a number is refused with its minimum") {
  Bench bench;
  bench.send("vacuum target nan\n");
  CHECK_EQ(bench.reply(), std::string("err out_of_range field=target value=nan min=0.0\n"));
}

TEST_CASE("a target past the floor is refused with its maximum") {
  Bench bench;
  bench.send("vacuum target 60.1\n");
  CHECK_EQ(bench.reply(), std::string("err out_of_range field=target value=60.1 max=60.0\n"));
}

TEST_CASE("a second target token is refused by name") {
  Bench bench;
  bench.send("vacuum target 5 6\n");
  CHECK_EQ(bench.reply(), std::string("err bad_argument 6\n"));
  CHECK(bench.queue.empty());
}

TEST_CASE("start with no reading refuses and queues nothing") {
  Bench bench;
  bench.send("vacuum start\n");
  CHECK_EQ(bench.reply(), std::string("err not_ready no_reading\n"));
  CHECK(bench.queue.empty());
}

TEST_CASE("start with a reading queues and answers the state it causes") {
  Bench bench;
  bench.sensor.read(12.0f);
  bench.send("vacuum target 40\n");
  bench.send("vacuum start\n");
  CHECK_EQ(bench.reply(), std::string("ok state=PULL\n"));
  CHECK_FALSE(bench.queue.empty());
}

TEST_CASE("a latched fault refuses a start") {
  Bench bench;
  bench.sensor.read(12.0f);
  bench.machine.vacuum().setTarget(40.0f);
  bench.machine.vacuum().start();
  bench.machine.vacuum().service(gardener::host::nowMs());
  bench.sensor.detach();
  gardener::host::advance(50);
  bench.machine.vacuum().service(gardener::host::nowMs());
  REQUIRE_EQ(std::string(bench.machine.vacuum().faultCode()), std::string("sensor"));

  bench.send("vacuum start\n");
  CHECK_EQ(bench.reply(), std::string("err not_ready latch\n"));
}

TEST_CASE("a hold of zero is refused with its minimum") {
  Bench bench;
  bench.sensor.read(12.0f);
  bench.send("vacuum start 0\n");
  CHECK_EQ(bench.reply(), std::string("err out_of_range field=hold value=0 min=1\n"));
}

TEST_CASE("a fractional hold is refused with its minimum") {
  Bench bench;
  bench.sensor.read(12.0f);
  bench.send("vacuum start 1.5\n");
  CHECK_EQ(bench.reply(), std::string("err out_of_range field=hold value=1.5 min=1\n"));
}

TEST_CASE("a hold past the ceiling is refused with its maximum") {
  Bench bench;
  bench.sensor.read(12.0f);
  bench.send("vacuum start 901\n");
  CHECK_EQ(bench.reply(), std::string("err out_of_range field=hold value=901 max=900\n"));
}

TEST_CASE("stop answers the release it queues") {
  Bench bench;
  bench.send("vacuum stop\n");
  CHECK_EQ(bench.reply(), std::string("ok state=VENT\n"));
  CHECK_FALSE(bench.queue.empty());
}

TEST_CASE("reset answers the idle state it queues") {
  Bench bench;
  bench.send("vacuum reset\n");
  CHECK_EQ(bench.reply(), std::string("ok state=IDLE\n"));
  CHECK_FALSE(bench.queue.empty());
}

TEST_CASE("a refused start leaves the queue empty and a queued one fills it") {
  Bench bench;
  bench.send("vacuum start\n");
  CHECK(bench.queue.empty());
  bench.send("vacuum stop\n");
  CHECK_EQ(bench.queue.count(), static_cast<size_t>(1));
}
