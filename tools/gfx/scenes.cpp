// The scene stack and the settings list: the frames and the request a scene may hold until the call
// that asked for it returns, and the list that walks a row, opens the row's editor and hands the
// set back.

#include <stdio.h>

#include <functional>
#include <memory>
#include <vector>

#include "suite.h"

namespace gardener::gfx::test {
namespace {

using namespace gardener::gfx::ui;

// What a case watches a scene by. The stack owns its scenes, so a scene that pops is gone: a case
// reads what it did from counts that live outside it.
struct Watched {
  int activations = 0;
  int leaves = 0;
};

// A scene that counts the calls the stack makes on it, and runs whatever a case asks it to.
class TestScene : public Scene {
 public:
  TestScene(GestureQueue& gestures, Watched& watched) : Scene(gestures), watched_(watched) {}

  void activate() override {
    watched_.activations++;
    if (onActivate) onActivate();
  }
  void leave() override { watched_.leaves++; }

  std::function<void()> onActivate;
  std::function<void()> onTick;

 protected:
  void tick(uint32_t) override {
    if (onTick) onTick();
  }

 private:
  Watched& watched_;
};

std::unique_ptr<TestScene> aScene(GestureQueue& gestures, Watched& watched) {
  return std::make_unique<TestScene>(gestures, watched);
}

void theRootSceneIsPlacedAndDropped() {
  NoGestures gestures;
  Watched watched;
  Ux ux;
  expectEq(ux.reset(aScene(gestures, watched)), 1, "a root scene shows");
  expectEq(ux.depth(), 1, "the root is the whole stack");
  expectEq(ux.top() != nullptr, 1, "and is on top");
  expectEq(watched.activations, 1, "the root was activated");
  expectEq(ux.reset(nullptr), 0, "no root shows nothing");
  expectEq(ux.depth(), 0, "the stack is empty");
  expectEq(watched.leaves, 1, "and the root was told it left");
}

void aPopHandsTheSceneBack() {
  NoGestures gestures;
  Watched rootWatch;
  Watched listWatch;
  Ux ux;
  ux.reset(aScene(gestures, rootWatch));
  Scene* popped = nullptr;
  expectEq(ux.push(aScene(gestures, listWatch),
                   [&popped](Scene& scene) { popped = &scene; }),
           1, "a push is taken");
  Scene* showing = ux.top();
  expectEq(listWatch.activations, 1, "the pushed scene was activated");
  expectEq(ux.pop(), 1, "a pop is taken");
  expectEq(popped == showing, 1, "the pusher's callback was handed the scene that popped");
  expectEq(listWatch.leaves, 1, "and the scene was told it left");
}

void aPopWithNoCallbackReactivatesBelow() {
  NoGestures gestures;
  Watched rootWatch;
  Watched listWatch;
  Ux ux;
  ux.reset(aScene(gestures, rootWatch));
  ux.push(aScene(gestures, listWatch));
  ux.pop();
  expectEq(rootWatch.activations, 2, "the scene below was activated again");
}

void theRootIsNotPopped() {
  NoGestures gestures;
  Watched rootWatch;
  Watched listWatch;
  Ux ux;
  ux.reset(aScene(gestures, rootWatch));
  expectEq(ux.pop(), 0, "a pop at the root is refused");
  ux.push(aScene(gestures, listWatch));
  expectEq(ux.popToRoot(), 1, "the stack goes home");
  expectEq(ux.depth(), 1, "only the root is left");
  expectEq(listWatch.leaves, 1, "and the scene above it was told it left");
}

void aPushFromActivateIsHeld() {
  NoGestures gestures;
  Watched rootWatch;
  Watched pusherWatch;
  Watched heldWatch;
  Ux ux;
  ux.reset(aScene(gestures, rootWatch));
  auto pusher = aScene(gestures, pusherWatch);
  pusher->onActivate = [&ux, &gestures, &heldWatch] { ux.push(aScene(gestures, heldWatch)); };
  expectEq(ux.push(std::move(pusher)), 1, "the pusher is placed");
  expectEq(ux.depth(), 2, "the push from inside activate was held");
  expectEq(heldWatch.activations, 0, "so the scene it pushed was not activated yet");
  ux.enter(canvas);
  expectEq(ux.depth(), 3, "the held push landed on the next call in");
  expectEq(heldWatch.activations, 1, "and the scene it pushed was activated");
}

void aPopFromAPollLandsAfterThePoll() {
  NoGestures gestures;
  Watched rootWatch;
  Watched editorWatch;
  Ux ux;
  ux.reset(aScene(gestures, rootWatch));
  auto editor = aScene(gestures, editorWatch);
  editor->onTick = [&ux] { ux.pop(); };
  ux.push(std::move(editor));
  expectEq(ux.poll(canvas, 1000), 1, "the beat is taken");
  expectEq(ux.depth(), 1, "the pop landed after the poll");
  expectEq(editorWatch.leaves, 1, "and the scene that popped is gone");
}

void theStackHoldsEightFrames() {
  NoGestures gestures;
  Watched watched;
  Ux ux;
  ux.reset(aScene(gestures, watched));
  while (ux.depth() < Ux::MaxFrames) ux.push(aScene(gestures, watched));
  expectEq(ux.depth(), Ux::MaxFrames, "the stack fills");
  expectEq(ux.push(aScene(gestures, watched)), 0, "a push past the cap is refused");
  expectEq(ux.depth(), Ux::MaxFrames, "and changes nothing");
}

// A page that inks the whole canvas it is given, so a case can see the band drawn over it.
class InkedScene : public Scene {
 public:
  explicit InkedScene(GestureQueue& gestures) : Scene(gestures) {}

 protected:
  void drawPage(Canvas& own) override {
    own.draw(gfx::shapes::FillRect(0, 0, own.width(), own.height(), Ink));
  }
};

void theStackDrawsTheBandOverThePage() {
  NoGestures gestures;
  Ux ux;
  reset();
  ux.setBand({Paint()});
  ux.reset(std::make_unique<InkedScene>(gestures));
  ux.poll(canvas, 300);
  expect(display.anyInk(0, 0, PageWidth, PageHeight - BandHeight), "the page inks its own rows");
  expectEq(display.inkCount(0, PageHeight - BandHeight, PageWidth, BandHeight), 0,
           "and the band covers the rows it owns");
}

void theStackDrawsTheBandItWasGiven() {
  NoGestures gestures;
  Watched watched;
  Ux ux;
  reset();
  ux.setBand({[](Canvas& cell) {
    cell.draw(gfx::shapes::FillRect(0, 0, cell.width(), cell.height(), Ink));
  }});
  ux.reset(aScene(gestures, watched));
  ux.poll(canvas, 300);
  expect(display.anyInk(0, PageHeight - BandHeight, PageWidth, BandHeight),
         "the band's cell is drawn in the bottom rows");
  expectEq(display.inkCount(0, 0, PageWidth, PageHeight - BandHeight), 0,
           "and a page that drew nothing leaves the rest bare");
}

void aStackShowingNothingDrawsNoBand() {
  NoGestures gestures;
  Ux ux;
  reset();
  ux.setBand({[](Canvas& cell) {
    cell.draw(gfx::shapes::FillRect(0, 0, cell.width(), cell.height(), Ink));
  }});
  expectEq(ux.poll(canvas, 300), 0, "an empty stack shows nothing");
  expectEq(display.inkCount(0, 0, PageWidth, PageHeight), 0, "so no band is drawn");
}

// --- the settings list ------------------------------------------------------

// A value editor a settings case can open: it answers whatever value the case sets on it.
class TestEditor : public Scene {
 public:
  TestEditor(GestureQueue& gestures, const std::any& value) : Scene(gestures), value_(value) {}

  std::any value() const override { return value_; }
  void set(const std::any& value) { value_ = value; }

 private:
  std::any value_;
};

// A settings row that prints a String value and builds one of those editors.
class TestRow : public SettingsRow {
 public:
  explicit TestRow(GestureQueue& gestures) : gestures_(gestures) {}

  void hold(const String& key, const String& label, const String& value) {
    key_ = key;
    label_ = label;
    value_ = value;
  }

  const String& key() const override { return key_; }
  const String& label() const override { return label_; }
  std::any currentValue() const override { return value_; }

  String text(const std::any& value) const override {
    return value.has_value() ? std::any_cast<String>(value) : String("--");
  }

  std::unique_ptr<Scene> scene(const std::any& value) override {
    return std::make_unique<TestEditor>(gestures_, value);
  }

 private:
  GestureQueue& gestures_;
  String key_;
  String label_;
  String value_;
};

// A settings list with the hooks it answers opened up, so a case can press the keys and draw it.
class SettingsProbe : public SettingsScene {
 public:
  using SettingsScene::SettingsScene;
  using SettingsScene::drawPage;

  bool press(const char* gesture) { return handle(gesture); }
};

// A queue a case fills: the scene on top drains it on its next beat, as the panel's queue does.
class Pressed : public GestureQueue {
 public:
  void press(const char* gesture) { queued_.push_back(gesture); }

  const char* take() override {
    if (queued_.empty()) return nullptr;
    const char* gesture = queued_.front();
    queued_.erase(queued_.begin());
    return gesture;
  }

 private:
  std::vector<const char*> queued_;
};

// One gesture through the stack: queued, then taken by whatever is on top. A scene that closes
// itself pops on this call rather than inside its own handle, which is how the stack is driven.
void press(Ux& ux, Pressed& gestures, const char* gesture, uint32_t nowMs) {
  gestures.press(gesture);
  ux.poll(canvas, nowMs);
}

void aSettingsListMovesAndEdits() {
  Pressed gestures;
  Ux ux;
  String title("SETTINGS");
  TestRow target(gestures);
  TestRow unit(gestures);
  target.hold("target", "Target", "50");
  unit.hold("unit", "Unit", "kPa");
  auto list = std::make_unique<SettingsProbe>(gestures, ux, title);
  SettingsProbe* settings = list.get();
  settings->set({&target, &unit});
  expectEq(settings->selected(), 0, "the list starts on its first row");
  settings->press("down:single");
  expectEq(settings->selected(), 1, "down moves to the next row");
  settings->press("down:single");
  expectEq(settings->selected(), 0, "and wraps at the end");
  settings->press("up:single");
  expectEq(settings->selected(), 1, "up wraps the other way");

  SettingValues handed;
  Scene* popped = nullptr;
  Watched rootWatch;
  ux.reset(aScene(gestures, rootWatch));
  ux.push(std::move(list), [&handed, &popped](Scene& scene) {
    popped = &scene;
    handed = static_cast<SettingsScene&>(scene).values();
  });

  press(ux, gestures, "forward:single", 1000);
  expectEq(ux.depth(), 3, "forward pushes the row's editor");
  auto* editor = static_cast<TestEditor*>(ux.top());
  editor->set(String("psi"));
  ux.pop();
  expectEq(ux.depth(), 2, "the editor pops back to the list");

  press(ux, gestures, "ok:single", 1010);
  expectEq(ux.depth(), 1, "OK closes the list");
  expectEq(popped == settings, 1, "handing it back");
  const String edited = std::any_cast<String>(handed.at("unit"));
  expectText(edited.c_str(), "psi", "with what the editor handed back under the row's key");
  const String untouched = std::any_cast<String>(handed.at("target"));
  expectText(untouched.c_str(), "50", "and the rows it did not open as they were");
}

void aSettingsListCancelPutsTheSetBack() {
  Pressed gestures;
  Ux ux;
  String title("SETTINGS");
  TestRow target(gestures);
  target.hold("target", "Target", "50");
  auto list = std::make_unique<SettingsProbe>(gestures, ux, title);
  SettingsProbe* settings = list.get();
  settings->set({&target});
  SettingValues handed;
  Watched rootWatch;
  ux.reset(aScene(gestures, rootWatch));
  ux.push(std::move(list), [&handed](Scene& scene) {
    handed = static_cast<SettingsScene&>(scene).values();
  });

  settings->press("forward:single");
  static_cast<TestEditor*>(ux.top())->set(String("70"));
  ux.pop();
  press(ux, gestures, "cancel:single", 1000);
  const String back = std::any_cast<String>(handed.at("target"));
  expectText(back.c_str(), "50", "Cancel hands back the set as it was opened");
}

void aSettingsListDrawsItsRows() {
  NoGestures gestures;
  Ux ux;
  String title("SETTINGS");
  TestRow target(gestures);
  TestRow unit(gestures);
  target.hold("target", "Target", "50");
  unit.hold("unit", "Unit", "kPa");
  SettingsProbe list(gestures, ux, title);
  list.set({&target, &unit});
  reset();
  list.drawPage(canvas);
  expect(display.anyInk(0, 0, PageWidth, 8), "the heading is drawn");
  expect(display.anyInk(0, 8, PageWidth, 8), "the first row is drawn under it");
  expect(display.inkCount(0, 8, PageWidth, 8) > display.inkCount(0, 16, PageWidth, 8),
         "and the row showing is inverted across its width");
}

// The number a row prints, and the two entries the choice row offers.
String numberText(float value) {
  char printed[16];
  snprintf(printed, sizeof(printed), "%g", static_cast<double>(value));
  return String(printed);
}

std::vector<ChoiceItem> choices() {
  return {{"lo", "Low", nullptr}, {"hi", "High", nullptr}};
}

// One row per value kind: it prints the kind's text and opens the kind's editor over the value the
// list holds. A real settings page builds these one class each; the case needs the four together.
class KindRow : public SettingsRow {
 public:
  enum class Kind { Number, Bool, Choice, Duration };

  KindRow(GestureQueue& gestures, Ux& ux, Kind kind, const char* key, const char* label,
          const std::any& value)
      : gestures_(gestures), ux_(ux), kind_(kind), key_(key), label_(label), value_(value) {}

  const String& key() const override { return key_; }
  const String& label() const override { return label_; }
  std::any currentValue() const override { return value_; }

  String text(const std::any& value) const override {
    if (!value.has_value()) return "--";
    if (kind_ == Kind::Number) return numberText(std::any_cast<float>(value));
    if (kind_ == Kind::Bool) return std::any_cast<bool>(value) ? "ON" : "OFF";
    if (kind_ == Kind::Choice) return std::any_cast<String>(value);
    char printed[16];
    durationText(DurationFormat::Minutes, std::any_cast<int32_t>(value), true, printed,
                 sizeof(printed));
    return String(printed);
  }

  std::unique_ptr<Scene> scene(const std::any& value) override {
    if (kind_ == Kind::Number) {
      const auto current = std::any_cast<float>(value);
      return std::make_unique<NumberScene>(gestures_, ux_, label_, current, 1, 0, 10, false);
    }
    if (kind_ == Kind::Bool) {
      const String current = std::any_cast<bool>(value) ? "true" : "false";
      return std::make_unique<BoolScene>(gestures_, ux_, label_, current, false);
    }
    if (kind_ == Kind::Choice) {
      return std::make_unique<ChoiceScene>(gestures_, ux_, label_, std::any_cast<String>(value),
                                           choices(), false);
    }
    const auto current = static_cast<float>(std::any_cast<int32_t>(value));
    return std::make_unique<DurationScene>(gestures_, ux_, label_, current, 5, 0, 3600, false);
  }

 private:
  GestureQueue& gestures_;
  Ux& ux_;
  Kind kind_;
  String key_;
  String label_;
  std::any value_;
};

void aSettingsListEditsEveryKind() {
  Pressed gestures;
  Ux ux;
  String title("SETTINGS");
  KindRow target(gestures, ux, KindRow::Kind::Number, "target", "Target", 5.0f);
  KindRow power(gestures, ux, KindRow::Kind::Bool, "power", "Power", false);
  KindRow unit(gestures, ux, KindRow::Kind::Choice, "unit", "Unit", String("lo"));
  KindRow hold(gestures, ux, KindRow::Kind::Duration, "hold", "Hold", int32_t(90));
  auto list = std::make_unique<SettingsScene>(gestures, ux, title);
  list->set({&target, &power, &unit, &hold});

  SettingValues handed;
  Watched rootWatch;
  ux.reset(aScene(gestures, rootWatch));
  ux.push(std::move(list), [&handed](Scene& scene) {
    handed = static_cast<SettingsScene&>(scene).values();
  });

  uint32_t now = 1000;
  // The number: two steps of one, from 5 to 7.
  press(ux, gestures, "forward:single", now += 10);
  press(ux, gestures, "up:single", now += 10);
  press(ux, gestures, "up:single", now += 10);
  press(ux, gestures, "ok:single", now += 10);

  // The bool: one step turns it from OFF to ON.
  press(ux, gestures, "down:single", now += 10);
  press(ux, gestures, "forward:single", now += 10);
  press(ux, gestures, "up:single", now += 10);
  press(ux, gestures, "ok:single", now += 10);

  // The choice: one step moves from the first entry to the second.
  press(ux, gestures, "down:single", now += 10);
  press(ux, gestures, "forward:single", now += 10);
  press(ux, gestures, "up:single", now += 10);
  press(ux, gestures, "ok:single", now += 10);

  // The duration: one step in the minutes stage takes 1m30s to 2m30s.
  press(ux, gestures, "down:single", now += 10);
  press(ux, gestures, "forward:single", now += 10);
  press(ux, gestures, "up:single", now += 10);
  press(ux, gestures, "ok:single", now += 10);

  press(ux, gestures, "ok:single", now += 10);

  expectEq(ux.depth(), 1, "the list closed back to the root");
  expectEq(std::any_cast<float>(handed.at("target")) == 7.0f, 1, "the number took its steps");
  expectEq(std::any_cast<bool>(handed.at("power")), 1, "the bool turned on");
  const String chosen = std::any_cast<String>(handed.at("unit"));
  expectText(chosen.c_str(), "hi", "the choice moved to its second entry");
  expectEq(std::any_cast<int32_t>(handed.at("hold")), 150, "the duration took a minute");
}

}  // namespace

void addSceneCases(std::vector<Case>& cases) {
  cases.push_back({"the root scene is placed and dropped", theRootSceneIsPlacedAndDropped});
  cases.push_back({"a pop hands the scene back", aPopHandsTheSceneBack});
  cases.push_back({"a pop with no callback reactivates below", aPopWithNoCallbackReactivatesBelow});
  cases.push_back({"the root is not popped", theRootIsNotPopped});
  cases.push_back({"a push from activate is held", aPushFromActivateIsHeld});
  cases.push_back({"a pop from a poll lands after the poll", aPopFromAPollLandsAfterThePoll});
  cases.push_back({"the stack holds eight frames", theStackHoldsEightFrames});
  cases.push_back({"the stack draws the band over the page", theStackDrawsTheBandOverThePage});
  cases.push_back({"the stack draws the band it was given", theStackDrawsTheBandItWasGiven});
  cases.push_back({"a stack showing nothing draws no band", aStackShowingNothingDrawsNoBand});
  cases.push_back({"a settings list moves and edits", aSettingsListMovesAndEdits});
  cases.push_back({"a settings list cancel puts the set back", aSettingsListCancelPutsTheSetBack});
  cases.push_back({"a settings list draws its rows", aSettingsListDrawsItsRows});
  cases.push_back({"a settings list edits every kind", aSettingsListEditsEveryKind});
}

}  // namespace gardener::gfx::test
