// The panel's three beats: the keys' policy, the fitted parts and the page. Each is driven here
// with the parts it takes, so the sequence a beat runs is reached without the RTOS task.

#include <doctest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "CaptureDisplay.h"
#include "Clock.h"
#include "core/Logger.h"
#include "core/SystemState.h"
#include "fakes/MemoryLibrary.h"
#include "fakes/Parts.h"
#include "Canvas.h"
#include "DisplayDevice.h"
#include "core/Battery.h"
#include "core/Button.h"
#include "core/Library.h"
#include "machine/CommandQueue.h"
#include "machine/MachineCommand.h"
#include "machine/VacuumSystem.h"
#include "panel/parts/ButtonsTick.h"
#include "panel/parts/BuzzerPlayer.h"
#include "panel/parts/PartsTick.h"
#include "routines/Interpreter.h"
#include "routines/Routines.h"
#include "routines/RoutinesTick.h"
#include "storage/DeviceSettings.h"

namespace {

using gardener::Battery;
using gardener::Button;
using gardener::panel::parts::ButtonsTick;
using gardener::panel::parts::BuzzerPlayer;
using gardener::CommandOutbox;
using gardener::CommandQueue;
using gardener::DeviceSettings;
using gardener::DisplayUnit;
using gardener::Interpreter;
using gardener::Library;
using gardener::Logger;
using gardener::MachineCommand;
using gardener::panel::parts::PartsTick;
using gardener::Press;
using gardener::Routines;
using gardener::RoutinesTick;
using gardener::RunState;
using gardener::SystemState;
using gardener::VacuumSystem;
using gardener::gfx::View;
using gardener::gfx::Canvas;
using gardener::gfx::DisplayDevice;
using gardener::panel::GestureQueue;
using gardener::panel::page::Scene;
using gardener::CaptureDisplay;
using gardener::test::FakeBattery;
using gardener::test::FakeButtonPanel;
using gardener::test::FakeBuzzer;
using gardener::test::FakePump;
using gardener::test::FakeSensor;
using gardener::test::FakeValve;
using gardener::test::MemoryLibrary;
using gardener::test::RecordingLogger;
using Op = gardener::MachineCommand::Op;

// The queue the outbox posts into: everything is taken, and a case reads back what arrived.
class MemoryQueue : public CommandQueue {
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

  size_t count() const { return commands_.size(); }
  const MachineCommand& at(size_t index) const { return commands_[index]; }

 private:
  std::vector<MachineCommand> commands_;
};

// What the scenes a fake interpreter answers have done, kept outside them so a case can read the
// figures after the stack has replaced one.
struct SceneCounter {
  int activates = 0;
  int ticks = 0;
  int roots = 0;
};

class CountingScene : public Scene {
 public:
  CountingScene(GestureQueue& gestures, SceneCounter& counter)
      : Scene(gestures), counter_(counter) {}

  void activate() override { counter_.activates++; }

 protected:
  void tick(uint32_t) override { counter_.ticks++; }

 private:
  SceneCounter& counter_;
};

// The interpreter the page's scenes come from: a fresh counting scene for each root, and the
// figures the cases read. The queue its scenes are driven from is its own, since the cases count
// what the stack does rather than what a press does.
class FakeInterpreter : public Interpreter {
 public:
  bool load(const String&, const String&) override { return true; }
  bool ready() const override { return true; }

  std::unique_ptr<Scene> rootScene() override {
    counter_.roots++;
    return std::make_unique<CountingScene>(gestures_, counter_);
  }

  const SceneCounter& counter() const { return counter_; }

 private:
  SceneCounter counter_;
  GestureQueue gestures_;
};

// The routines a page tick drives: the fake interpreter, an empty order file so no image is read,
// and the glass and state the root scene is put on. The rig owns the published state, so a case can
// say what the overlay draws.
struct PageRig {
  PageRig()
      : settings(&library, state),
        buzzer(buzzerPart),
        vacuum(&pump, &sensor, &valve),
        routines(interpreter, display, state) {
    state.batteryPresent = battery.present();
    state.batteryPercent = battery.percent();
    state.batteryLow = battery.low();

    library.put("/system/order", "");
    ok = routines.begin(logger, library);
  }

  MemoryLibrary library;
  RecordingLogger logger;
  SystemState state;
  DeviceSettings settings;
  CaptureDisplay display{128, 64};
  FakeBuzzer buzzerPart;
  BuzzerPlayer buzzer;
  FakePump pump;
  FakeSensor sensor;
  FakeValve valve;
  VacuumSystem vacuum;
  FakeBattery battery;
  FakeInterpreter interpreter;
  Routines routines;
  bool ok = false;
};

// The buttons tick and the parts it answers into.
struct ButtonsRig {
  explicit ButtonsRig(GestureQueue& presses) : tick(buttons, presses, outbox, logger) {}

  FakeButtonPanel buttons;
  MemoryQueue queue;
  CommandOutbox outbox{queue};
  RecordingLogger logger;
  ButtonsTick tick;
};

}  // namespace

TEST_CASE("the reset hold is handed back and posts nothing") {
  GestureQueue presses;
  ButtonsRig rig{presses};

  rig.buttons.hold(Button::Ok, 5000);
  rig.buttons.hold(Button::Cancel, 5000);
  rig.tick.tick();

  CHECK(rig.tick.takeReset());
  CHECK_FALSE(rig.tick.takeReset());
  CHECK_FALSE(rig.tick.takeHome());
  CHECK_EQ(rig.queue.count(), static_cast<size_t>(0));
  CHECK(rig.logger.saw("panel: reset held"));
}

TEST_CASE("the cancel hold stops the machine and asks for home") {
  GestureQueue presses;
  ButtonsRig rig{presses};

  rig.buttons.hold(Button::Cancel, 5000);
  rig.tick.tick();

  CHECK(rig.tick.takeHome());
  CHECK_FALSE(rig.tick.takeHome());
  CHECK_FALSE(rig.tick.takeReset());
  REQUIRE_EQ(rig.queue.count(), static_cast<size_t>(1));
  CHECK_EQ(rig.queue.at(0).op, Op::Stop);
  CHECK(rig.logger.saw("panel: cancel held"));
}

TEST_CASE("a gesture reaches the queue the page drains") {
  PageRig page;
  REQUIRE(page.ok);
  ButtonsRig rig{page.routines.presses()};

  rig.buttons.press(Button::Back);
  rig.tick.tick();
  rig.buttons.release(Button::Back);
  rig.buttons.classify(Button::Back, Press::Single);
  rig.tick.tick();

  String gesture;
  REQUIRE(page.routines.takePress(gesture));
  CHECK_EQ(std::string(gesture.c_str()), std::string("back:single"));
}

TEST_CASE("the gauge and the piezo are updated on the same millisecond") {
  gardener::host::reset();
  FakeBattery battery;
  FakeBuzzer buzzerPart;
  BuzzerPlayer buzzer{buzzerPart};
  SystemState state;
  PartsTick tick{battery, buzzer, state};

  buzzer.tone(440, 10);
  REQUIRE(buzzer.sounding());
  tick.tick(5);
  CHECK(buzzer.sounding());
  tick.tick(20);
  CHECK_FALSE(buzzer.sounding());
  CHECK_EQ(battery.updates(), 2);
  CHECK_EQ(battery.lastUpdateMs(), 20u);

  // The beat also publishes the pack it read, which is what the overlay draws.
  CHECK(state.batteryPresent);
  CHECK_EQ(state.batteryPercent, battery.percent());
  CHECK(state.batteryLow == battery.low());
}

TEST_CASE("a pack with no gauge behind it publishes no percentage") {
  gardener::host::reset();
  FakeBattery battery;
  battery.setPresent(false);
  battery.setPercent(73);
  FakeBuzzer buzzerPart;
  BuzzerPlayer buzzer{buzzerPart};
  SystemState state;
  PartsTick tick{battery, buzzer, state};

  tick.tick(10);
  CHECK_FALSE(state.batteryPresent);
  CHECK_EQ(state.batteryPercent, 0);
}

TEST_CASE("the page activates on begin, polls on a tick and goes home") {
  PageRig rig;
  REQUIRE(rig.ok);
  const int activates = rig.interpreter.counter().activates;
  const int roots = rig.interpreter.counter().roots;
  RoutinesTick page{rig.routines};

  page.begin();
  CHECK_EQ(rig.interpreter.counter().activates, activates + 1);

  page.tick(300);
  CHECK_EQ(rig.interpreter.counter().ticks, 1);

  page.home();
  CHECK_EQ(rig.interpreter.counter().roots, roots + 1);
}

// The glass is sent the frame once per beat and once when a page goes up, whatever drew in it: the
// scene draws into the panel's canvas and the beat is what hands it over.
TEST_CASE("a page going up sends the frame once") {
  PageRig rig;
  REQUIRE(rig.ok);
  RoutinesTick page{rig.routines};
  rig.display.clear();

  page.begin();

  CHECK_EQ(rig.display.flushCount(), 1);
}

TEST_CASE("a beat sends the frame once whether it drew or not") {
  PageRig rig;
  REQUIRE(rig.ok);
  RoutinesTick page{rig.routines};
  page.begin();
  rig.display.clear();

  // The repaint cadence has passed, so the scene draws its page and the send follows it.
  page.tick(300);
  CHECK_EQ(rig.display.flushCount(), 1);

  // Inside the cadence nothing is drawn, and the beat still sends what it has once.
  page.tick(310);
  CHECK_EQ(rig.display.flushCount(), 2);
}

// The overlay's four cells are drawn from the published state, so a slice whose writer stopped, or a
// field one stopped writing, is a cell that draws the same thing whatever the machine says. Each case
// changes one field and compares the frame the beat sent.
std::vector<uint8_t> beatFrame(PageRig& rig) {
  rig.display.clear();
  rig.display.beginCapture();

  REQUIRE(rig.routines.poll(1000));
  const std::vector<uint8_t>* frame = rig.display.frame(0);
  REQUIRE_MESSAGE(frame != nullptr, "the beat kept no frame");
  return *frame;
}

TEST_CASE("the overlay draws the unit the document publishes") {
  PageRig rig;
  REQUIRE(rig.ok);
  RoutinesTick page{rig.routines};
  page.begin();

  rig.settings.setUnit(DisplayUnit::Kpa);
  const std::vector<uint8_t> kpaFrame = beatFrame(rig);

  rig.settings.setUnit(DisplayUnit::Psi);
  CHECK(kpaFrame != beatFrame(rig));
}

TEST_CASE("the overlay draws the pack the parts beat publishes") {
  PageRig rig;
  REQUIRE(rig.ok);
  RoutinesTick page{rig.routines};
  page.begin();

  rig.state.batteryPresent = true;
  rig.state.batteryPercent = 80;
  const std::vector<uint8_t> fittedFrame = beatFrame(rig);

  rig.state.batteryPresent = false;
  CHECK(fittedFrame != beatFrame(rig));
}

TEST_CASE("the overlay draws the run state and both links") {
  PageRig rig;
  REQUIRE(rig.ok);
  RoutinesTick page{rig.routines};
  page.begin();
  const std::vector<uint8_t> idleFrame = beatFrame(rig);

  rig.state.runState = RunState::Pulling;
  CHECK(idleFrame != beatFrame(rig));

  rig.state.runState = RunState::Idle;
  rig.state.serialConnected = true;
  rig.state.bluetoothConnected = true;
  CHECK(idleFrame != beatFrame(rig));
}
