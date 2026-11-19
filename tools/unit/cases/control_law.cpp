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

using gardener::ControlAbovePercent;
using gardener::ControlBelowPercent;
using gardener::ControlFloorKpa;
using gardener::ControlMaxRunMs;
using gardener::ControlOvershootReadings;
using gardener::ControlOvershootRecoveries;
using gardener::ControlPeriodMs;
using gardener::ControlProgressKpa;
using gardener::ControlProgressTimeoutMs;
using gardener::RunState;
using gardener::VacuumSystem;
using gardener::VentFloorKpa;
using gardener::VentMarginKpa;
using gardener::VentTimeoutMs;
using gardener::VentValve;
using gardener::test::FakePump;
using gardener::test::FakeSensor;
using gardener::test::FakeValve;

// The chamber a case drives: the three parts, the loop over them, and a clock that only the case
// moves. The event log records every pump and valve call in the order the loop made them.
struct Rig {
  void beat(int count = 1) {
    for (int i = 0; i < count; i++) {
      gardener::host::advance(ControlPeriodMs);
      system.service(gardener::host::nowMs());
    }
  }

  // Beats until the clock reaches `ms`, so a case can name the time it is asserting at.
  void beatUntil(unsigned long ms) {
    while (gardener::host::nowMs() < ms) {
      gardener::host::advance(ControlPeriodMs);
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

// The band either side of `target`, as depths: the edges the relay and the valve work from.
float shallowEdge(float target) { return target * (1.0f - ControlBelowPercent / 100.0f); }
float deepEdge(float target) { return target * (1.0f + ControlAbovePercent / 100.0f); }

}  // namespace

TEST_CASE("a target of zero releases the chamber") {
  Rig rig;
  runFrom(rig, 20.0f, 20.0f);
  rig.system.setTarget(0.0f);
  rig.beat(2);
  CHECK_EQ(rig.system.runState(), RunState::Venting);
  CHECK(rig.valve.vents());
  CHECK_FALSE(rig.pump.active());
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
  rig.beatUntil(VentTimeoutMs + 1000);
  CHECK_EQ(std::string(rig.system.faultCode()), std::string("vent_timeout"));
}

TEST_CASE("a start while running keeps the first deadline") {
  Rig rig;
  runFrom(rig, 40.0f, 20.0f);
  rig.system.start(10);
  CHECK_EQ(rig.system.holdSeconds(), ControlMaxRunMs / 1000);
}

TEST_CASE("a hold of zero asks for the ceiling") {
  Rig rig;
  rig.sensor.read(40.0f);
  rig.system.begin();
  rig.system.setTarget(20.0f);
  REQUIRE(rig.system.start(0));
  CHECK_EQ(rig.system.holdSeconds(), ControlMaxRunMs / 1000);
}

TEST_CASE("a hold past the ceiling asks for the ceiling") {
  Rig rig;
  rig.sensor.read(40.0f);
  rig.system.begin();
  rig.system.setTarget(20.0f);
  REQUIRE(rig.system.start(ControlMaxRunMs / 1000 + 1));
  CHECK_EQ(rig.system.holdSeconds(), ControlMaxRunMs / 1000);
}

TEST_CASE("a hold inside the ceiling is granted as asked") {
  Rig rig;
  rig.sensor.read(40.0f);
  rig.system.begin();
  rig.system.setTarget(20.0f);
  REQUIRE(rig.system.start(901));
  CHECK_EQ(rig.system.holdSeconds(), static_cast<uint32_t>(901));
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
  CHECK_EQ(rig.system.runState(), RunState::Holding);
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
  rig.beatUntil(VentTimeoutMs + ControlPeriodMs);
  CHECK_EQ(std::string(rig.system.faultCode()), std::string("vent_timeout"));
}

TEST_CASE("a pump that makes no headway faults") {
  Rig rig;
  runFrom(rig, 10.0f, 40.0f);
  while (gardener::host::nowMs() < ControlProgressTimeoutMs + ControlPeriodMs) {
    rig.sensor.read(10.0f);
    rig.beat();
  }
  CHECK_EQ(std::string(rig.system.faultCode()), std::string("no_progress"));
}

TEST_CASE("the pump runs below the band and stops inside it") {
  Rig rig;
  runFrom(rig, 0.0f, 20.0f);
  rig.beat();
  CHECK(rig.pump.active());

  rig.sensor.read(20.0f);
  rig.beat();
  CHECK_FALSE(rig.pump.active());

  rig.sensor.read(shallowEdge(20.0f));
  rig.beat();
  CHECK(rig.pump.active());
}

TEST_CASE("a reading past the band vents back instead of faulting") {
  Rig rig;
  runFrom(rig, 20.0f, 20.0f);
  for (int i = 0; i < ControlOvershootReadings; i++) {
    rig.sensor.read(deepEdge(20.0f) + 1.0f);
    rig.beat();
  }
  CHECK_EQ(std::string(rig.system.faultCode()), std::string(""));
  CHECK(rig.valve.vents());
  CHECK_EQ(rig.system.runState(), RunState::Venting);

  // The vent lands just past the target, and the pump takes the band back up from the leak.
  rig.sensor.read(19.5f);
  rig.beat();
  CHECK_FALSE(rig.valve.vents());
  CHECK_EQ(rig.system.runState(), RunState::Holding);

  rig.sensor.read(18.9f);
  rig.beat();
  CHECK(rig.pump.active());
}

TEST_CASE("a run that keeps overshooting the same way faults") {
  Rig rig;
  runFrom(rig, 20.0f, 20.0f);
  rig.sensor.read(deepEdge(20.0f) + 1.0f);
  rig.beatUntil((VentTimeoutMs + 1000) * (ControlOvershootRecoveries + 1));
  CHECK_EQ(std::string(rig.system.faultCode()), std::string("overshoot"));
  CHECK(rig.valve.vents());
}

TEST_CASE("a release stops the pump before it opens the valve") {
  Rig rig;
  runFrom(rig, 40.0f, 20.0f);
  rig.events.clear();
  rig.system.stop();
  rig.beat();
  const int stopped = rig.eventIndex("pump.off");
  const int opened = rig.eventIndex("valve.open");
  REQUIRE(stopped >= 0);
  REQUIRE(opened >= 0);
  CHECK(stopped < opened);
}

TEST_CASE("the reported floor and band are the loop's own numbers") {
  CHECK_EQ(ControlFloorKpa, 60.0f);
  CHECK_EQ(ControlBelowPercent, 5.0f);
  CHECK_EQ(ControlAbovePercent, 5.0f);
  CHECK_EQ(VentFloorKpa, 1.0f);
  CHECK_EQ(VentMarginKpa, 2.0f);
  CHECK_EQ(ControlProgressKpa, 1.0f);
}
