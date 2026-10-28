// The control law on a clock the case owns: the states a run passes through, the refusals and the
// faults, and the order the pump and the valve are driven in. The rig is fake and the time is the
// suite's, so an hour of a run costs nothing.

#include <doctest.h>

#include <string>
#include <vector>

#include "Clock.h"
#include "fakes/Parts.h"
#include "machine/Machine.h"
#include "machine/VacuumSystem.h"

namespace {

using gardener::CONTROL_MAX_RUN_MS;
using gardener::CONTROL_MIN_TARGET_KPA;
using gardener::CONTROL_OVERSHOOT_KPA;
using gardener::CONTROL_OVERSHOOT_READINGS;
using gardener::CONTROL_PERIOD_MS;
using gardener::CONTROL_PROGRESS_KPA;
using gardener::CONTROL_PROGRESS_TIMEOUT_MS;
using gardener::RunState;
using gardener::VentValve;
using gardener::VacuumSystem;
using gardener::VENT_FLOOR_KPA;
using gardener::VENT_MARGIN_KPA;
using gardener::VENT_TIMEOUT_MS;
using gardener::test::FakePump;
using gardener::test::FakeSensor;
using gardener::test::FakeValve;

// The chamber a case drives: the three parts, the loop over them, and a clock that only the case
// moves. The event log records every pump and valve call in the order the loop made them.
struct Rig {
  void beat(int count = 1) {
    for (int i = 0; i < count; i++) {
      gardener::host::advance(CONTROL_PERIOD_MS);
      system.service(gardener::host::nowMs());
    }
  }

  // Beats until the clock reaches `ms`, so a case can name the time it is asserting at.
  void beatUntil(unsigned long ms) {
    while (gardener::host::nowMs() < ms) {
      gardener::host::advance(CONTROL_PERIOD_MS);
      system.service(gardener::host::nowMs());
    }
  }

  int eventIndex(const char* what) const {
    for (size_t i = 0; i < events.size(); i++) {
      if (events[i] == what) return static_cast<int>(i);
    }
    return -1;
  }

  std::vector<std::string> events;
  FakePump pump{&events};
  FakeSensor sensor;
  FakeValve valve{false, &events};
  VacuumSystem system{&pump, &sensor, &valve};

  Rig() { gardener::host::reset(); }
};

// A run pulled to `target`, with the chamber answering `reading` so a case decides where in the run
// it is.
void runFrom(Rig& rig, float reading, float target) {
  rig.sensor.read(reading);
  rig.system.begin();
  rig.system.setTarget(target);
  rig.system.start();
  rig.beat();
}

}  // namespace

TEST_CASE("a target of zero releases the chamber") {
  Rig rig;
  runFrom(rig, 20.0f, 20.0f);
  rig.system.setTarget(0.0f);
  rig.beat(2);
  CHECK_EQ(rig.system.runState(), RunState::Venting);
  CHECK(rig.valve.vents());
  CHECK_EQ(rig.pump.duty(), 0.0f);
}

TEST_CASE("stop from idle changes nothing and starts no vent") {
  Rig rig;
  rig.sensor.read(0.5f);
  rig.system.begin();
  rig.beat(2);
  rig.system.stop();
  rig.beat(2);
  CHECK_EQ(rig.system.runState(), RunState::Idle);
}

TEST_CASE("stop while a release vents does not restart its window") {
  Rig rig;
  runFrom(rig, 20.0f, 20.0f);
  rig.system.stop();
  rig.beatUntil(5000);
  rig.system.stop();
  rig.beatUntil(VENT_TIMEOUT_MS + 1000);
  CHECK_EQ(std::string(rig.system.faultCode()), std::string("vent_timeout"));
}

TEST_CASE("a start while running keeps the first deadline") {
  Rig rig;
  runFrom(rig, 40.0f, 20.0f);
  rig.system.start(10);
  CHECK_EQ(rig.system.holdSeconds(), CONTROL_MAX_RUN_MS / 1000);
}

TEST_CASE("a hold of zero asks for the ceiling") {
  Rig rig;
  runFrom(rig, 40.0f, 20.0f);
  rig.system.start(0);
  CHECK_EQ(rig.system.holdSeconds(), CONTROL_MAX_RUN_MS / 1000);
}

TEST_CASE("a hold past the ceiling asks for the ceiling") {
  Rig rig;
  runFrom(rig, 40.0f, 20.0f);
  rig.system.start(901);
  CHECK_EQ(rig.system.holdSeconds(), CONTROL_MAX_RUN_MS / 1000);
}

TEST_CASE("the deadline ends the run as a release and reports itself once") {
  Rig rig;
  rig.sensor.read(40.0f);
  rig.system.begin();
  rig.system.setTarget(20.0f);
  REQUIRE(rig.system.start(1));
  rig.beatUntil(2100);
  CHECK_EQ(rig.system.runState(), RunState::Venting);
  CHECK(rig.system.takeDeadlineExpired());
  CHECK_FALSE(rig.system.takeDeadlineExpired());
}

TEST_CASE("a target set shallower than the reading vents to it and carries on") {
  Rig rig;
  runFrom(rig, 40.0f, 40.0f);
  rig.beat(2);
  rig.system.setTarget(20.0f);
  rig.beat();
  CHECK(rig.valve.vents());
  while (rig.sensor.pressureKpa() > 20.0f + 1.0f) {
    rig.sensor.read(rig.sensor.pressureKpa() - 1.0f);
    rig.beat();
  }
  rig.beat();
  CHECK_FALSE(rig.valve.vents());
  CHECK_EQ(rig.system.runState(), RunState::Pulling);
}

TEST_CASE("a sensor that stops answering faults the run") {
  Rig rig;
  runFrom(rig, 40.0f, 20.0f);
  rig.sensor.detach();
  rig.beat();
  CHECK_EQ(std::string(rig.system.faultCode()), std::string("sensor"));
  CHECK(rig.valve.vents());
  CHECK_EQ(std::string(rig.system.startRefusal()), std::string("latch"));
}

TEST_CASE("a release that misses the vent floor faults on its timeout") {
  Rig rig;
  runFrom(rig, 20.0f, 20.0f);
  rig.system.stop();
  rig.beatUntil(VENT_TIMEOUT_MS + CONTROL_PERIOD_MS);
  CHECK_EQ(std::string(rig.system.faultCode()), std::string("vent_timeout"));
}

TEST_CASE("a pull with no headway at high duty faults") {
  Rig rig;
  runFrom(rig, 10.0f, 40.0f);
  while (gardener::host::nowMs() < CONTROL_PROGRESS_TIMEOUT_MS + CONTROL_PERIOD_MS) {
    rig.sensor.read(10.0f);
    rig.beat();
  }
  CHECK_EQ(std::string(rig.system.faultCode()), std::string("no_progress"));
}

TEST_CASE("readings past the target fault the run") {
  Rig rig;
  runFrom(rig, 20.0f, 20.0f);
  for (int i = 0; i < CONTROL_OVERSHOOT_READINGS + 1; i++) {
    rig.sensor.read(20.0f + CONTROL_OVERSHOOT_KPA);
    rig.beat();
  }
  CHECK_EQ(std::string(rig.system.faultCode()), std::string("overshoot"));
}

TEST_CASE("the duty never exceeds the pump's ceiling") {
  Rig rig;
  runFrom(rig, 90.0f, CONTROL_FLOOR_KPA);
  for (int i = 0; i < 200; i++) {
    rig.sensor.read(90.0f - static_cast<float>(i) * 0.1f);
    rig.beat();
    CHECK(rig.pump.duty() <= PUMP_DUTY_MAX_PERCENT);
  }
}

TEST_CASE("a release stops the pump before it opens the valve") {
  Rig rig;
  runFrom(rig, 40.0f, 20.0f);
  rig.events.clear();
  rig.system.stop();
  rig.beat();
  const int stopped = rig.eventIndex("pump.stop");
  const int opened = rig.eventIndex("valve.open");
  REQUIRE(stopped >= 0);
  REQUIRE(opened >= 0);
  CHECK(stopped < opened);
}

TEST_CASE("the reported floor and deadband are the loop's own numbers") {
  CHECK_EQ(CONTROL_FLOOR_KPA, 60.0f);
  CHECK_EQ(CONTROL_DEADBAND_KPA, 0.2f);
  CHECK_EQ(CONTROL_MIN_TARGET_KPA, 5.0f);
  CHECK_EQ(VENT_FLOOR_KPA, 1.0f);
  CHECK_EQ(VENT_MARGIN_KPA, 2.0f);
  CHECK_EQ(CONTROL_PROGRESS_KPA, 1.0f);
}
