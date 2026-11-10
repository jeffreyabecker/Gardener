// The control tick: what one beat applies from the queue, how a burst collapses to what the machine
// should do rather than replaying every write, and the run log. The tick is shaped to be reached by
// a case, so it is driven here with a clock the case owns.

#include <doctest.h>

#include <string>
#include <vector>

#include "Clock.h"
#include "beats/MachineTick.h"
#include "fakes/Parts.h"
#include "machine/CommandQueue.h"
#include "machine/MachineCommand.h"
#include "machine/VacuumSystem.h"

namespace {

using gardener::CommandQueue;
using gardener::MachineCommand;
using gardener::MachineTick;
using gardener::RunState;
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

// The machine a tick drives: the three fakes, the loop over them, the queue and the log.
struct Rig {
  Rig() : tick(system, logger, queue) {
    gardener::host::reset();
    sensor.read(20.0f);
    system.begin();
  }

  FakePump pump;
  FakeSensor sensor;
  FakeValve valve;
  VacuumSystem system{&pump, &sensor, &valve};
  RecordingLogger logger;
  BoundedQueue queue;
  MachineTick tick;
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

// The beat's own effect: what the panel used to draw is what the run log says, and the machine's
// state after the tick is the state the queue asked for.
TEST_CASE("a start in a beat puts the machine in the run it was asked for") {
  Rig rig;
  REQUIRE(rig.queue.post(target(40.0f)));
  REQUIRE(rig.queue.post(op(Op::Start)));
  rig.tick.tick(0);

  CHECK_EQ(rig.system.targetPressureKpa(), 40.0f);
  CHECK(rig.system.runState() == RunState::Pulling);
  CHECK(rig.system.running());
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

TEST_CASE("the run log names a state change once") {
  Rig rig;
  REQUIRE(rig.queue.post(op(Op::Stop)));
  rig.tick.tick(0);
  CHECK_EQ(rig.system.runState(), RunState::Venting);
  CHECK_EQ(linesWith(rig.logger, "IDLE -> VENT"), 1);
  rig.tick.tick(50);
  CHECK_EQ(linesWith(rig.logger, "IDLE -> VENT"), 1);
}
