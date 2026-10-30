// The control tick: what one beat applies from the queue, how a burst collapses to what the machine
// should do rather than replaying every write, the reboot the composition takes back, and the run
// log. The tick is shaped to be reached by a case, so it is driven here with a clock the case owns.

#include <doctest.h>

#include <string>
#include <vector>

#include "Clock.h"
#include "core/SystemState.h"
#include "fakes/Parts.h"
#include "machine/CommandQueue.h"
#include "machine/ControlTick.h"
#include "machine/MachineCommand.h"
#include "machine/VacuumSystem.h"

namespace {

using gardener::CommandOutbox;
using gardener::CommandQueue;
using gardener::ControlTick;
using gardener::MachineCommand;
using gardener::RunState;
using gardener::SystemState;
using gardener::VacuumSystem;
using gardener::test::FakePump;
using gardener::test::FakeSensor;
using gardener::test::FakeValve;
using gardener::test::RecordingLogger;
using Op = gardener::MachineCommand::Op;

// A queue with a depth of the case's choosing, so a full one can be reached.
class BoundedQueue : public CommandQueue {
 public:
  explicit BoundedQueue(size_t capacity = 8) : capacity_(capacity) {}

  bool post(const MachineCommand& command) override {
    if (commands_.size() >= capacity_) return false;
    commands_.push_back(command);
    return true;
  }

  bool take(MachineCommand& command) override {
    if (commands_.empty()) return false;
    command = commands_.front();
    commands_.erase(commands_.begin());
    return true;
  }

  size_t count() const { return commands_.size(); }
  const std::vector<MachineCommand>& commands() const { return commands_; }

 private:
  size_t capacity_;
  std::vector<MachineCommand> commands_;
};

// The machine a tick drives: the three fakes, the loop over them, the queue, the log, and the state
// the tick publishes its slice into.
struct Rig {
  Rig() : tick(system, logger, queue, published) {
    gardener::host::reset();
    sensor.read(20.0f);
    system.begin();
  }

  FakePump pump;
  FakeSensor sensor;
  FakeValve valve;
  VacuumSystem system{&pump, &sensor, &valve};
  RecordingLogger logger;
  SystemState published;
  BoundedQueue queue;
  ControlTick tick;
};

MachineCommand target(float kpa) { return MachineCommand{Op::SetTarget, kpa}; }
MachineCommand op(Op which) { return MachineCommand{which, 0.0f}; }

int linesWith(const RecordingLogger& logger, const char* part) {
  int count = 0;
  for (const std::string& line : logger.lines()) {
    if (line.find(part) != std::string::npos) count++;
  }
  return count;
}

}  // namespace

TEST_CASE("a queued command reaches the machine on the next tick") {
  Rig rig;
  REQUIRE(rig.queue.post(target(20.0f)));
  rig.tick.tick(0);
  CHECK_EQ(rig.system.targetPressureKpa(), 20.0f);
}

// The beat's own publication: what the panel draws and what a page's read answers is this slice.
TEST_CASE("a tick publishes the machine's slice") {
  Rig rig;
  REQUIRE(rig.queue.post(target(40.0f)));
  REQUIRE(rig.queue.post(op(Op::Start)));
  rig.tick.tick(0);

  CHECK_EQ(rig.published.targetPressureKpa, 40.0f);
  CHECK_EQ(rig.published.currentPressureKpa, 20.0f);
  CHECK(rig.published.runState == RunState::Pulling);
  CHECK(rig.published.running);
  CHECK(rig.published.canStart);
}

TEST_CASE("a start a page would be refused for is published that way") {
  Rig rig;
  rig.sensor.detach();
  rig.tick.tick(0);

  CHECK_FALSE(rig.published.canStart);
}

TEST_CASE("a stop in a burst discards the targets before it") {
  Rig rig;
  REQUIRE(rig.queue.post(target(30.0f)));
  REQUIRE(rig.queue.post(op(Op::Stop)));
  rig.tick.tick(0);
  CHECK_EQ(rig.system.targetPressureKpa(), 0.0f);
  CHECK(rig.logger.saw("dropped target 30.0 before a stop"));
}

TEST_CASE("repeated targets reduce to the last one") {
  Rig rig;
  REQUIRE(rig.queue.post(target(10.0f)));
  REQUIRE(rig.queue.post(target(25.0f)));
  rig.tick.tick(0);
  CHECK_EQ(rig.system.targetPressureKpa(), 25.0f);
  CHECK(rig.logger.saw("dropped target 10.0 for a later target"));
}

TEST_CASE("a reboot is handed back rather than applied") {
  Rig rig;
  const RunState before = rig.system.runState();
  REQUIRE(rig.queue.post(op(Op::Reboot)));
  rig.tick.tick(0);
  CHECK(rig.tick.takeReboot());
  CHECK_FALSE(rig.tick.takeReboot());
  CHECK_EQ(rig.system.runState(), before);
}

TEST_CASE("the run log names a state change once") {
  Rig rig;
  REQUIRE(rig.queue.post(op(Op::Stop)));
  rig.tick.tick(0);
  CHECK_EQ(rig.system.runState(), RunState::Venting);
  CHECK_EQ(linesWith(rig.logger, "IDLE -> VENT"), 1);
  rig.tick.tick(50);
  CHECK_EQ(linesWith(rig.logger, "IDLE -> VENT"), 1);
}

TEST_CASE("the outbox holds what a full queue could not take and flushes in order") {
  BoundedQueue queue(1);
  CommandOutbox outbox(queue);

  CHECK(outbox.post(op(Op::ResetFault)));
  CHECK_FALSE(outbox.post(target(5.0f)));
  CHECK_FALSE(outbox.post(target(15.0f)));

  MachineCommand taken{Op::ResetFault, 0.0f};
  REQUIRE(queue.take(taken));
  outbox.flush();
  CHECK_EQ(queue.count(), static_cast<size_t>(1));

  MachineCommand first{Op::ResetFault, 0.0f};
  REQUIRE(queue.take(first));
  CHECK_EQ(first.op, Op::SetTarget);
  CHECK_EQ(first.value, 5.0f);

  outbox.flush();
  MachineCommand second{Op::ResetFault, 0.0f};
  REQUIRE(queue.take(second));
  CHECK_EQ(second.op, Op::SetTarget);
  CHECK_EQ(second.value, 15.0f);
}

TEST_CASE("the outbox refuses the newest when its own capacity is reached") {
  BoundedQueue queue(1);
  CommandOutbox outbox(queue);
  REQUIRE(outbox.post(op(Op::ResetFault)));
  for (int i = 0; i < 8; i++) CHECK_FALSE(outbox.post(target(static_cast<float>(i))));
  CHECK_FALSE(outbox.post(target(99.0f)));
}
