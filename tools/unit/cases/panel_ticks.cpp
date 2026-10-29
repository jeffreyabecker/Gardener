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
#include "fakes/MemoryLibrary.h"
#include "fakes/Parts.h"
#include "gfx/Canvas.h"
#include "gfx/DisplayDevice.h"
#include "hal/Battery.h"
#include "hal/Button.h"
#include "hal/Library.h"
#include "machine/CommandQueue.h"
#include "machine/MachineCommand.h"
#include "machine/VacuumSystem.h"
#include "panel/ButtonsTick.h"
#include "panel/BuzzerPlayer.h"
#include "panel/GestureClassifier.h"
#include "panel/PageTick.h"
#include "panel/PanelContext.h"
#include "panel/PartsTick.h"
#include "routines/Interpreter.h"
#include "routines/Routines.h"
#include "storage/Config.h"

namespace {

using gardener::Battery;
using gardener::Button;
using gardener::ButtonsTick;
using gardener::BuzzerPlayer;
using gardener::CommandOutbox;
using gardener::CommandQueue;
using gardener::Config;
using gardener::GestureClassifier;
using gardener::Interpreter;
using gardener::Library;
using gardener::Logger;
using gardener::MachineCommand;
using gardener::PageTick;
using gardener::PanelContext;
using gardener::PartsTick;
using gardener::Press;
using gardener::Routines;
using gardener::VacuumSystem;
using gardener::gfx::Canvas;
using gardener::gfx::DisplayDevice;
using gardener::gfx::ui::GestureQueue;
using gardener::gfx::ui::Scene;
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
// figures the cases read.
class FakeInterpreter : public Interpreter {
 public:
  bool begin(PanelContext&, Routines&) override { return true; }
  bool load(const String&, const String&) override { return true; }

  std::unique_ptr<Scene> rootScene(GestureQueue& gestures, PanelContext&) override {
    counter_.roots++;
    return std::make_unique<CountingScene>(gestures, counter_);
  }

  const SceneCounter& counter() const { return counter_; }

 private:
  SceneCounter counter_;
};

// The panel a routines object draws through, assembled from the fakes and a stub glass.
class PageContext : public PanelContext {
 public:
  PageContext(DisplayDevice& display, Canvas& canvas, Config& config, VacuumSystem& vacuum,
              Battery& battery, BuzzerPlayer& buzzer, Library& library, Logger& logger)
      : display_(display),
        canvas_(canvas),
        config_(config),
        vacuum_(vacuum),
        battery_(battery),
        buzzer_(buzzer),
        library_(library),
        logger_(logger) {}

  DisplayDevice& display() override { return display_; }
  Canvas& canvas() override { return canvas_; }
  Config& config() override { return config_; }
  VacuumSystem& vacuum() override { return vacuum_; }
  Battery& battery() override { return battery_; }
  BuzzerPlayer& buzzer() override { return buzzer_; }
  Library& library() override { return library_; }
  Logger& logger() override { return logger_; }
  bool serialConnected() const override { return false; }
  bool bluetoothConnected() const override { return false; }
  bool post(const MachineCommand&) override { return true; }

 private:
  DisplayDevice& display_;
  Canvas& canvas_;
  Config& config_;
  VacuumSystem& vacuum_;
  Battery& battery_;
  BuzzerPlayer& buzzer_;
  Library& library_;
  Logger& logger_;
};

// The routines a page tick drives: the fake interpreter, an empty order file so no image is read,
// and the context the root scene is put on.
struct PageRig {
  PageRig()
      : config(&library),
        canvas(display.rootCanvas()),
        buzzer(buzzerPart),
        vacuum(&pump, &sensor, &valve),
        context(display, canvas, config, vacuum, battery, buzzer, library, logger),
        routines(interpreter) {
    library.put("/system/order", "");
    ok = routines.begin(context);
  }

  MemoryLibrary library;
  RecordingLogger logger;
  Config config;
  CaptureDisplay display{128, 64};
  Canvas canvas;
  FakeBuzzer buzzerPart;
  BuzzerPlayer buzzer;
  FakePump pump;
  FakeSensor sensor;
  FakeValve valve;
  VacuumSystem vacuum;
  FakeBattery battery;
  FakeInterpreter interpreter;
  PageContext context;
  Routines routines;
  bool ok = false;
};

// The buttons tick and the parts it answers into.
struct ButtonsRig {
  explicit ButtonsRig(Routines& routines)
      : classifier(buttons), tick(classifier, routines, outbox, logger) {}

  FakeButtonPanel buttons;
  GestureClassifier classifier;
  MemoryQueue queue;
  CommandOutbox outbox{queue};
  RecordingLogger logger;
  ButtonsTick tick;
};

}  // namespace

TEST_CASE("the reset hold is handed back and posts nothing") {
  FakeInterpreter interpreter;
  Routines routines{interpreter};
  ButtonsRig rig{routines};

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
  FakeInterpreter interpreter;
  Routines routines{interpreter};
  ButtonsRig rig{routines};

  rig.buttons.hold(Button::Cancel, 5000);
  rig.tick.tick();

  CHECK(rig.tick.takeHome());
  CHECK_FALSE(rig.tick.takeHome());
  CHECK_FALSE(rig.tick.takeReset());
  REQUIRE_EQ(rig.queue.count(), static_cast<size_t>(1));
  CHECK_EQ(rig.queue.at(0).op, Op::Stop);
  CHECK(rig.logger.saw("panel: cancel held"));
}

TEST_CASE("a gesture goes to the active page rather than the log") {
  PageRig page;
  REQUIRE(page.ok);
  ButtonsRig rig{page.routines};

  rig.buttons.press(Button::Back);
  rig.tick.tick();
  rig.buttons.release(Button::Back);
  rig.buttons.classify(Button::Back, Press::Single);
  rig.tick.tick();

  String gesture;
  REQUIRE(page.routines.takePress(gesture));
  CHECK_EQ(std::string(gesture.c_str()), std::string("back:single"));
  CHECK_FALSE(rig.logger.saw("btn:"));
}

TEST_CASE("a gesture with no page is logged") {
  FakeInterpreter interpreter;
  Routines routines{interpreter};
  ButtonsRig rig{routines};

  rig.buttons.press(Button::Back);
  rig.tick.tick();
  rig.buttons.release(Button::Back);
  rig.buttons.classify(Button::Back, Press::Single);
  rig.tick.tick();

  CHECK(rig.logger.saw("btn: back:single"));
}

TEST_CASE("the gauge and the piezo are updated on the same millisecond") {
  gardener::host::reset();
  FakeBattery battery;
  FakeBuzzer buzzerPart;
  BuzzerPlayer buzzer{buzzerPart};
  PartsTick tick{battery, buzzer};

  buzzer.tone(440, 10);
  REQUIRE(buzzer.sounding());
  tick.tick(5);
  CHECK(buzzer.sounding());
  tick.tick(20);
  CHECK_FALSE(buzzer.sounding());
  CHECK_EQ(battery.updates(), 2);
  CHECK_EQ(battery.lastUpdateMs(), 20u);
}

TEST_CASE("the page activates on begin, polls on a tick and goes home") {
  PageRig rig;
  REQUIRE(rig.ok);
  const int activates = rig.interpreter.counter().activates;
  const int roots = rig.interpreter.counter().roots;
  PageTick page{rig.routines};

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
  PageTick page{rig.routines};
  rig.display.clear();

  page.begin();

  CHECK_EQ(rig.display.flushCount(), 1);
}

TEST_CASE("a beat sends the frame once whether it drew or not") {
  PageRig rig;
  REQUIRE(rig.ok);
  PageTick page{rig.routines};
  page.begin();
  rig.display.clear();

  // The repaint cadence has passed, so the scene draws its page and the send follows it.
  page.tick(300);
  CHECK_EQ(rig.display.flushCount(), 1);

  // Inside the cadence nothing is drawn, and the beat still sends what it has once.
  page.tick(310);
  CHECK_EQ(rig.display.flushCount(), 2);
}
