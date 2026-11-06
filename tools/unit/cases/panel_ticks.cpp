// The panel's three beats: the keys' policy, the fitted parts and the page. Each is driven here
// with the parts it takes, so the sequence a beat runs is reached without the RTOS task.

#include <doctest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Canvas.h"
#include "CaptureDisplay.h"
#include "Clock.h"
#include "DisplayDevice.h"
#include "beats/ButtonsTick.h"
#include "beats/PartsTick.h"
#include "beats/RoutinesTick.h"
#include "core/Battery.h"
#include "core/Button.h"
#include "core/Gestures.h"
#include "core/Library.h"
#include "core/Logger.h"
#include "core/SystemState.h"
#include "fakes/MemoryLibrary.h"
#include "fakes/Parts.h"
#include "machine/CommandQueue.h"
#include "machine/MachineCommand.h"
#include "machine/VacuumSystem.h"
#include "panel/GestureQueue.h"
#include "panel/page/RoutineRegistry.h"
#include "panel/parts/BuzzerPlayer.h"
#include "panel/parts/GestureClassifier.h"
#include "routines/Interpreter.h"
#include "routines/Routines.h"
#include "storage/DeviceSettings.h"

namespace {

using gardener::Battery;
using gardener::Button;
using gardener::ButtonsTick;
using gardener::BuzzerPlayer;
using gardener::CaptureDisplay;
using gardener::CommandQueue;
using gardener::DeviceSettings;
using gardener::DisplayUnit;
using gardener::GestureQueue;
namespace Gestures = gardener::Gestures;
using gardener::GestureClassifier;
using gardener::Interpreter;
using gardener::Library;
using gardener::Logger;
using gardener::MachineCommand;
using gardener::PartsTick;
using gardener::Press;
using gardener::RoutineRegistry;
using gardener::Routines;
using gardener::RoutinesTick;
using gardener::RunState;
using gardener::Scene;
using gardener::SystemState;
using gardener::VacuumSystem;
using gardener::gfx::Canvas;
using gardener::gfx::DisplayDevice;
using gardener::gfx::View;
using gardener::test::FakeBattery;
using gardener::test::FakeButtonPanel;
using gardener::test::FakeBuzzer;
using gardener::test::FakePump;
using gardener::test::FakeSensor;
using gardener::test::FakeValve;
using gardener::test::MemoryLibrary;
using gardener::test::RecordingLogger;
using Op = gardener::MachineCommand::Op;

// The queue the panel posts its own commands into: everything is taken, and a case reads back what
// arrived.
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
  bool begin() override { return true; }
  bool load(const String&, const String&) override { return true; }

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
      : settings(&library, state), buzzer(buzzerPart), vacuum(&pump, &sensor, &valve),
        routines(interpreter, display, state, presses, ux, registry, logger, library) {
    state.batteryPresent = battery.present();
    state.batteryPercent = battery.percent();
    state.batteryLow = battery.low();

    library.put("/system/order", "");
    ok = routines.begin();
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
  GestureQueue presses;
  gardener::Ux ux;
  RoutineRegistry registry{library, logger};
  Routines routines;
  bool ok = false;
};

// The buttons tick and the parts it answers into.
struct ButtonsRig {
  explicit ButtonsRig(GestureQueue& presses) : tick(buttons, presses, queue, logger) {
    tick.begin();
  }

  FakeButtonPanel buttons;
  MemoryQueue queue;
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

TEST_CASE("gestures come back in the order they were posted") {
  constexpr int Presses = 100;
  GestureQueue presses;
  for (int i = 0; i < Presses; i++)
    CHECK(presses.post(i % 2 == 0 ? Gestures::OkSingle : Gestures::BackSingle));

  // The queue grows as presses arrive, so every one of them is waiting in the order it was posted.
  int taken = 0;
  for (const char* gesture = presses.take(); gesture != nullptr; gesture = presses.take()) {
    CHECK(Gestures::is(gesture, taken % 2 == 0 ? Gestures::OkSingle : Gestures::BackSingle));
    taken++;
  }
  CHECK_EQ(taken, Presses);
  CHECK_EQ(presses.take(), nullptr);
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

  const char* gesture = page.routines.takePress();
  REQUIRE(gesture != nullptr);
  CHECK_EQ(std::string(gesture), std::string("back:single"));
}

TEST_CASE("the gesture vocabulary compares a whole gesture and its parts") {
  CHECK(Gestures::is(Gestures::OkSingle, "ok:single"));
  CHECK_FALSE(Gestures::is(Gestures::OkDouble, Gestures::OkSingle));
  CHECK_FALSE(Gestures::is(nullptr, Gestures::OkSingle));
  CHECK_FALSE(Gestures::is(Gestures::OkSingle, nullptr));

  // A page that treats two keys alike names both of them.
  CHECK(Gestures::isAny(Gestures::BackSingle, {Gestures::BackSingle, Gestures::CancelSingle}));
  CHECK(Gestures::isAny(Gestures::CancelSingle, {Gestures::BackSingle, Gestures::CancelSingle}));
  CHECK_FALSE(Gestures::isAny(Gestures::OkSingle, {Gestures::BackSingle, Gestures::CancelSingle}));
  CHECK_FALSE(Gestures::isAny(Gestures::OkSingle, {}));

  // A chord is not either of its parts, and a word is a part only when the whole of it is there.
  CHECK(Gestures::hasPart("ok:single+cancel:5", Gestures::OkSingle));
  CHECK(Gestures::hasPart("ok:single+cancel:5", "cancel:5"));
  CHECK_FALSE(Gestures::hasPart(Gestures::OkDouble, Gestures::OkSingle));
  CHECK_FALSE(Gestures::hasPart("ok:single+cancel:5", "ok"));
  CHECK_FALSE(Gestures::hasPart(nullptr, Gestures::OkSingle));
  CHECK_FALSE(Gestures::hasPart(Gestures::OkSingle, ""));
}

TEST_CASE("an override answers the word a page registered instead of the press") {
  FakeButtonPanel buttons;
  GestureClassifier classifier(buttons);

  // One press, as the reader reports it across two beats: the level arrives, then the release is
  // classified.
  const auto press = [&](Button button, Press press) {
    buttons.press(button);
    classifier.update();
    buttons.release(button);
    buttons.classify(button, press);
    return std::string(classifier.update().c_str());
  };

  CHECK_EQ(press(Button::Back, Press::Single), std::string(Gestures::BackSingle));

  classifier.addOverride(Gestures::BackSingle, Gestures::CancelSingle);
  CHECK_EQ(press(Button::Back, Press::Single), std::string(Gestures::CancelSingle));
  // The press is renamed, not the key: a word nobody registered is answered as it was read.
  CHECK_EQ(press(Button::Ok, Press::Single), std::string(Gestures::OkSingle));
  CHECK_EQ(press(Button::Back, Press::Double), std::string(Gestures::BackDouble));
}

TEST_CASE("an override names a whole gesture rather than its parts") {
  FakeButtonPanel buttons;
  GestureClassifier classifier(buttons);

  // Ok and Cancel down together, released together, as one gesture in panel order.
  const auto chord = [&] {
    buttons.press(Button::Ok);
    classifier.update();
    buttons.press(Button::Cancel);
    classifier.update();
    buttons.release(Button::Ok);
    buttons.release(Button::Cancel);
    buttons.classify(Button::Ok, Press::Single);
    buttons.classify(Button::Cancel, Press::Single);
    return std::string(classifier.update().c_str());
  };

  CHECK_EQ(chord(), std::string("ok:single+cancel:single"));

  // A part is not rewritten, so overriding one key's press leaves the pair as it was read.
  classifier.addOverride(Gestures::CancelSingle, "cancel");
  CHECK_EQ(chord(), std::string("ok:single+cancel:single"));

  // The pair itself can be named, and then it is the whole pair that is replaced.
  classifier.addOverride("ok:single+cancel:single", "menu");
  CHECK_EQ(chord(), std::string("menu"));

  // A replacement is an answer and not another gesture, so it is not looked up again: Cancel
  // answers Back's word, and Back's word is not then turned back by Cancel's.
  classifier.clearOverrides();
  classifier.addOverride(Gestures::BackSingle, Gestures::CancelSingle);
  classifier.addOverride(Gestures::CancelSingle, Gestures::BackSingle);
  buttons.press(Button::Back);
  classifier.update();
  buttons.release(Button::Back);
  buttons.classify(Button::Back, Press::Single);
  CHECK_EQ(std::string(classifier.update().c_str()), std::string(Gestures::CancelSingle));
}

TEST_CASE("an override removed or cleared hands the press back") {
  FakeButtonPanel buttons;
  GestureClassifier classifier(buttons);
  classifier.addOverride(Gestures::BackSingle, "menu");

  const auto pressBack = [&] {
    buttons.press(Button::Back);
    classifier.update();
    buttons.release(Button::Back);
    buttons.classify(Button::Back, Press::Single);
    return std::string(classifier.update().c_str());
  };

  CHECK_EQ(pressBack(), std::string("menu"));

  classifier.removeOverride(Gestures::BackSingle);
  CHECK_EQ(pressBack(), std::string(Gestures::BackSingle));

  // Registering the same word twice keeps the first answer, which is the one removeOverride took.
  classifier.addOverride(Gestures::BackSingle, "menu");
  classifier.addOverride(Gestures::BackSingle, "other");
  CHECK_EQ(pressBack(), std::string("menu"));

  classifier.clearOverrides();
  CHECK_EQ(pressBack(), std::string(Gestures::BackSingle));
}

TEST_CASE("a press beside a held key answers the word a page registered for the pair") {
  FakeButtonPanel buttons;
  GestureClassifier classifier(buttons);

  // Ok held past the mark says its seconds; Cancel pressed beside it joins that mark instead of
  // waiting for both keys to come up.
  buttons.press(Button::Ok);
  classifier.update();
  buttons.hold(Button::Ok, 3000);
  CHECK_EQ(std::string(classifier.update().c_str()), std::string("ok:3"));

  buttons.press(Button::Cancel);
  classifier.update();
  buttons.release(Button::Cancel);
  buttons.classify(Button::Cancel, Press::Single);

  classifier.addOverride("ok:3+cancel:single", "menu");
  CHECK_EQ(std::string(classifier.update().c_str()), std::string("menu"));
}

TEST_CASE("a hold mark answers the word a page registered for it") {
  // The reset is five seconds on Okay and Cancel together, and a page names it as what it means.
  FakeButtonPanel buttons;
  GestureClassifier classifier(buttons);
  classifier.addOverride("ok:5+cancel:5", "system:reboot");

  buttons.hold(Button::Ok, 4000);
  buttons.hold(Button::Cancel, 4000);
  CHECK_EQ(std::string(classifier.update().c_str()), std::string("ok:4+cancel:4"));

  buttons.hold(Button::Ok, 5000);
  buttons.hold(Button::Cancel, 5000);
  CHECK_EQ(std::string(classifier.update().c_str()), std::string("system:reboot"));

  // A single key's mark is nameable too, and a mark nobody named is answered as it was read.
  FakeButtonPanel heldButtons;
  GestureClassifier held(heldButtons);
  held.addOverride("cancel:5", "system:stop");

  heldButtons.hold(Button::Ok, 5000);
  CHECK_EQ(std::string(held.update().c_str()), std::string("ok:5"));

  heldButtons.hold(Button::Cancel, 5000);
  CHECK_EQ(std::string(held.update().c_str()), std::string("system:stop"));
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
