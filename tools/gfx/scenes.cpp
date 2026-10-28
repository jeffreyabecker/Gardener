// The scene stack and the settings list: the frames and the request a scene may hold until the call
// that asked for it returns, and the list that walks a row, opens the row's editor and hands the
// set back.

#include <functional>
#include <math.h>
#include <memory>
#include <utility>
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
  ux.enter(canvas, 400);
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

void aScenePaintsAsSoonAsItIsPushed() {
  NoGestures gestures;
  Watched rootWatch;
  Ux ux;
  reset();
  auto root = aScene(gestures, rootWatch);
  root->onTick = [&ux, &gestures] { ux.push(std::make_unique<InkedScene>(gestures)); };
  ux.reset(std::move(root));
  ux.poll(canvas, 300);
  expect(display.anyInk(0, 0, PageWidth, PageHeight),
         "the scene the beat pushed painted on that beat");
}

void aPagePaintsAgainWhenTheOneAboveItPops() {
  NoGestures gestures;
  Watched childWatch;
  Ux ux;
  reset();
  ux.reset(std::make_unique<InkedScene>(gestures));
  ux.poll(canvas, 300);
  expect(display.anyInk(0, 0, PageWidth, PageHeight), "the page is painted");
  auto child = aScene(gestures, childWatch);
  child->onTick = [&ux] { ux.pop(); };
  ux.push(std::move(child));
  ux.poll(canvas, 320);
  expect(display.anyInk(0, 0, PageWidth, PageHeight),
         "the page that came back is painted again, not the one that left");
}

// --- the settings list ------------------------------------------------------

// A value editor a settings case can open: it answers whatever value the case sets on it.
class TestEditor : public EditorScene {
 public:
  TestEditor(GestureQueue& gestures, Ux& ux, String& label, const std::any& value)
      : EditorScene(gestures, ux, label), value_(value) {}

  std::any value() const override { return value_; }
  void set(const std::any& value) { value_ = value; }

 protected:
  void nudge(int8_t) override {}

 private:
  std::any value_;
};

// A settings row that prints a String value and builds one of those editors.
class TestRow : public SettingsRow {
 public:
  TestRow(GestureQueue& gestures, Ux& ux) : gestures_(gestures), ux_(ux) {}

  void hold(const String& key, const String& label, const String& value) {
    key_ = key;
    label_ = label;
    value_ = value;
  }

  const String& key() const override { return key_; }
  const String& label() const override { return label_; }
  std::any defaultValue() const override { return value_; }

  String text(const std::any& value) const override {
    return value.has_value() ? std::any_cast<String>(value) : String("--");
  }

  String stored(const std::any& value) const override {
    return value.has_value() ? std::any_cast<String>(value) : String();
  }

  std::any parse(const String& stored) const override {
    return stored.isEmpty() ? defaultValue() : std::any(stored);
  }

  std::unique_ptr<EditorScene> scene(const std::any& value) override {
    return std::make_unique<TestEditor>(gestures_, ux_, label_, value);
  }

 private:
  GestureQueue& gestures_;
  Ux& ux_;
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
  TestRow target(gestures, ux);
  TestRow unit(gestures, ux);
  target.hold("target", "Target", "50");
  unit.hold("unit", "Unit", "kPa");
  std::vector<SettingsRow*> rows{&target, &unit};
  auto list = std::make_unique<SettingsProbe>(gestures, ux, title, rows, SettingValues());
  SettingsProbe* settings = list.get();
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
  TestRow target(gestures, ux);
  target.hold("target", "Target", "50");
  std::vector<SettingsRow*> rows{&target};
  auto list = std::make_unique<SettingsProbe>(gestures, ux, title, rows, SettingValues());
  SettingsProbe* settings = list.get();
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
  TestRow target(gestures, ux);
  TestRow unit(gestures, ux);
  target.hold("target", "Target", "50");
  unit.hold("unit", "Unit", "kPa");
  SettingsProbe list(gestures, ux, title, {&target, &unit}, {});
  reset();
  list.drawPage(canvas);
  expect(display.anyInk(0, 0, PageWidth, 8), "the heading is drawn");
  expect(display.anyInk(0, 8, PageWidth, 8), "the first row is drawn under it");
  expect(display.inkCount(0, 8, PageWidth, 8) > display.inkCount(0, 16, PageWidth, 8),
         "and the row showing is inverted across its width");
}

// The two entries the choice row offers, and the keys they store.
std::vector<ChoiceItem> choices() {
  return {{"lo", "Low", nullptr}, {"hi", "High", nullptr}};
}

void aSettingsListEditsEveryKind() {
  Pressed gestures;
  Ux ux;
  String title("SETTINGS");
  NumberRow target(gestures, ux, "target", "Target", 5.0f, 1, 0, 10, false);
  BoolRow power(gestures, ux, "power", "Power", false);
  ChoiceRow unit(gestures, ux, "unit", "Unit", "lo", choices(), false);
  DurationRow hold(gestures, ux, "hold", "Hold", 90, 5, 0, 3600, false);
  std::vector<SettingsRow*> rows{&target, &power, &unit, &hold};
  auto list = std::make_unique<SettingsScene>(gestures, ux, title, rows, SettingValues());

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

// Each kind's row prints the value the list shows and opens an editor over it, which hands the same
// kind of value back: the row and its scene agree about what the setting holds.
void aRowPrintsItsValueAndOpensItsEditor() {
  NoGestures gestures;
  Ux ux;
  NumberRow number(gestures, ux, "target", "Target", 5.0f, 1, 0, 10, false);
  BoolRow power(gestures, ux, "power", "Power", true);
  ChoiceRow unit(gestures, ux, "unit", "Unit", "hi", choices(), false);
  DurationRow hold(gestures, ux, "hold", "Hold", 90, 5, 0, 3600, false);
  NumberRow unset(gestures, ux, "ceiling", "Ceiling", NAN, 1, 0, 10, true);

  expectText(number.text(number.defaultValue()).c_str(), "5", "a number prints its digits");
  expectText(power.text(power.defaultValue()).c_str(), "ON", "a bool prints its state");
  expectText(unit.text(unit.defaultValue()).c_str(), "High", "a choice prints the title it names");
  expectText(hold.text(hold.defaultValue()).c_str(), "1m30s", "a duration prints compact");
  expectText(hold.text(std::any()).c_str(), "--", "a value holding nothing prints dashes");

  expectEq(std::any_cast<float>(number.scene(number.defaultValue())->value()) == 5.0f, 1,
           "the number editor opens on the value the row holds");
  expectEq(std::any_cast<bool>(power.scene(power.defaultValue())->value()), 1,
           "the bool editor opens on the state the row holds");
  expectText(std::any_cast<String>(unit.scene(unit.defaultValue())->value()).c_str(), "hi",
             "the choice editor opens on the key the row holds");
  expectEq(std::any_cast<int32_t>(hold.scene(hold.defaultValue())->value()), 90,
           "the duration editor opens on the seconds the row holds");

  expectEq(unset.defaultValue().has_value(), 0, "a number whose default is nothing answers none");
  expectText(unset.text(unset.defaultValue()).c_str(), "--", "and prints dashes, as its editor");
  expectEq(unset.scene(unset.defaultValue())->isNull(), 1, "opens on nothing");
}

// The list is seeded from the set the document holds: what the set says for a key it carries, the
// row's own default for a key it does not, and nothing for a key the set holds nothing for. What
// goes in is what Back and Cancel put back, so the list opened on it.
void aSettingsListSeedsFromTheDocument() {
  Pressed gestures;
  Ux ux;
  String title("SETTINGS");
  BoolRow power(gestures, ux, "power", "Power", true);
  NumberRow target(gestures, ux, "target", "Target", 5.0f, 1, 0, 10, false);
  NumberRow ceiling(gestures, ux, "ceiling", "Ceiling", 9.0f, 1, 0, 10, true);
  SettingValues held;
  held["power"] = false;
  held["ceiling"] = std::any();
  std::vector<SettingsRow*> rows{&power, &target, &ceiling};
  auto list = std::make_unique<SettingsProbe>(gestures, ux, title, rows, held);
  SettingsProbe* settings = list.get();

  expectEq(std::any_cast<bool>(settings->values().at("power")), 0,
           "the set's value wins over the row's default");
  expectEq(std::any_cast<float>(settings->values().at("target")) == 5.0f, 1,
           "a key the set does not carry starts from the row's default");
  expectEq(settings->values().at("ceiling").has_value(), 0,
           "a key the set holds nothing for starts as nothing");

  Watched rootWatch;
  ux.reset(aScene(gestures, rootWatch));
  ux.push(std::move(list), nullptr);

  settings->press("forward:single");
  expectText(static_cast<BoolScene*>(ux.top())->text().c_str(), "OFF",
             "the row's editor opens on the set's value");
  ux.pop();
  settings->press("down:single");
  settings->press("down:single");
  settings->press("forward:single");
  expectEq(ux.top()->isNull(), 1, "a setting the set holds nothing for opens on nothing");
  ux.pop();
  expectEq(std::any_cast<bool>(settings->values().at("power")), 0,
           "the editor hands the set back as it opened");
}

// Each kind's row writes the document's form of a value, which is not the form the list prints, and
// reads that text back as the value it wrote.
void aRowStoresItsDocumentForm() {
  NoGestures gestures;
  Ux ux;
  NumberRow number(gestures, ux, "target", "Target", 5.0f, 1, 0, 10, false);
  BoolRow power(gestures, ux, "power", "Power", true);
  ChoiceRow unit(gestures, ux, "unit", "Unit", "hi", choices(), false);
  DurationRow hold(gestures, ux, "hold", "Hold", 90, 5, 0, 3600, false);
  NumberRow unset(gestures, ux, "ceiling", "Ceiling", NAN, 1, 0, 10, true);

  expectText(number.stored(std::any(7.5f)).c_str(), "7.50", "a number stores at its decimals");
  expectText(power.stored(std::any(true)).c_str(), "on", "a bool stores a word, not its state");
  expectText(unit.stored(std::any(String("lo"))).c_str(), "lo", "a choice stores its key");
  expectText(hold.stored(std::any(static_cast<int32_t>(150))).c_str(), "150",
             "a duration stores whole seconds");

  expectEq(std::any_cast<float>(number.parse(String("7.50"))) == 7.5f, 1,
           "a number reads back the value it stored");
  expectEq(std::any_cast<bool>(power.parse(String("off"))), 0, "a bool reads the word it stores");
  expectEq(std::any_cast<bool>(power.parse(String("YES"))), 1, "and the words a config reads too");
  expectText(std::any_cast<String>(unit.parse(String("lo"))).c_str(), "lo",
             "a choice reads back the key it stored");
  expectEq(std::any_cast<int32_t>(hold.parse(String("150"))), 150,
           "a duration reads back the seconds it stored");

  expectText(number.stored(std::any()).c_str(), "", "nothing stores as nothing");
  expectText(unset.stored(std::any(NAN)).c_str(), "", "and so does a number holding none");
  expectEq(std::any_cast<float>(number.parse(String())) == 5.0f, 1,
           "empty text answers the row's default");
  expectEq(unset.parse(String()).has_value(), 0, "which for a nullable row is nothing");
  expectEq(std::any_cast<bool>(power.parse(String("maybe"))), 1,
           "a word the kind cannot read answers the default");
}

// The check 5_ramped_static's declaration gives its total: the profile is the two holds plus a
// ramp, so the total covers the holds. This is the declaration's form of a check — the declaration,
// the set in force and the value proposed — which is what a row's constructor is handed.
std::pair<bool, String> totalCoversHolds(const std::vector<SettingsRow*>&,
                                         const SettingValues& values,
                                         const std::any& proposed) {
  const auto initial = values.find("initial_duration");
  const auto target = values.find("target_duration");
  const int32_t holds =
      (initial == values.end() ? 0 : std::any_cast<int32_t>(initial->second)) +
      (target == values.end() ? 0 : std::any_cast<int32_t>(target->second));
  const int32_t total = std::any_cast<int32_t>(proposed);
  return total < holds ? std::make_pair(false, String("TOTAL < HOLDS"))
                       : std::make_pair(true, String());
}

// The check 2_ramped_interval's declaration gives its final pressure: a ramp ends no lower than it
// starts.
std::pair<bool, String> finalUnderStart(const std::vector<SettingsRow*>&,
                                        const SettingValues& values, const std::any& proposed) {
  const auto initial = values.find("initial_pressure");
  const float start = initial == values.end() ? 0.0f : std::any_cast<float>(initial->second);
  const float end = std::any_cast<float>(proposed);
  return end < start ? std::make_pair(false, String("FINAL < START"))
                     : std::make_pair(true, String());
}

// A row's check curried over the declaration and the set the list holds refuses the values a
// routine cannot run and accepts the ones it can, and a row the declaration gave none answers none.
void aRowsCheckRefusesWhatTheSetCannotUse() {
  NoGestures gestures;
  Ux ux;
  DurationRow initial(gestures, ux, "initial_duration", "Start time", 60, 1, 0, 7200, false);
  DurationRow target(gestures, ux, "target_duration", "Target time", 60, 1, 0, 7200, false);
  DurationRow total(gestures, ux, "total_duration", "Total time", 300, 1, 1, 7200, false,
                    totalCoversHolds);
  NumberRow start(gestures, ux, "initial_pressure", "Start", 5.0f, 1, 0, 100, false);
  NumberRow last(gestures, ux, "final_pressure", "Final", 5.0f, 1, 0, 100, false, nullptr, 2,
                 finalUnderStart);
  std::vector<SettingsRow*> rows{&initial, &target, &total, &start, &last};

  SettingValues values;
  values["initial_duration"] = static_cast<int32_t>(60);
  values["target_duration"] = static_cast<int32_t>(60);
  values["total_duration"] = static_cast<int32_t>(300);
  values["initial_pressure"] = 5.0f;
  values["final_pressure"] = 5.0f;

  const CurriedValidator holds = total.check(rows, values);
  expectEq(holds != nullptr, 1, "a row the declaration gave a check carries it");
  if (!holds) return;
  std::pair<bool, String> refused = holds(std::any(static_cast<int32_t>(30)));
  expectEq(refused.first, 0, "a total of 30 under holds of 60 and 60 is refused");
  expectText(refused.second.c_str(), "TOTAL < HOLDS", "with the text that refuses it");
  expectEq(holds(std::any(static_cast<int32_t>(400))).first, 1, "a total of 400 is accepted");

  const CurriedValidator pressure = last.check(rows, values);
  expectEq(pressure != nullptr, 1, "the final pressure row carries its check too");
  if (!pressure) return;
  refused = pressure(std::any(3.0f));
  expectEq(refused.first, 0, "a final pressure of 3.0 under a start of 5.0 is refused");
  expectText(refused.second.c_str(), "FINAL < START", "with the text that refuses it");
  expectEq(pressure(std::any(5.0f)).first, 1, "a final pressure at the start is accepted");

  expectEq(initial.check(rows, values) == nullptr, 1, "a row given no check carries none");
}

// The list hands a row's check to the editor it opens, so a refused value keeps that editor on the
// stack and never reaches the set: the list closes on the value it was opened with.
void aSettingsListKeepsARefusedValueOut() {
  Pressed gestures;
  Ux ux;
  String title("SETTINGS");
  NumberRow start(gestures, ux, "initial_pressure", "Start", 5.0f, 1, 0, 100, false);
  NumberRow last(gestures, ux, "final_pressure", "Final", 5.0f, 1, 0, 100, false, nullptr, 2,
                 finalUnderStart);
  std::vector<SettingsRow*> rows{&start, &last};
  auto list = std::make_unique<SettingsProbe>(gestures, ux, title, rows, SettingValues());
  SettingsProbe* settings = list.get();

  SettingValues handed;
  Watched rootWatch;
  ux.reset(aScene(gestures, rootWatch));
  ux.push(std::move(list), [&handed](Scene& scene) {
    handed = static_cast<SettingsScene&>(scene).values();
  });

  settings->press("down:single");
  settings->press("forward:single");
  expectEq(ux.depth(), 3, "the row's editor is open");
  press(ux, gestures, "down:single", 1000);
  press(ux, gestures, "down:single", 1010);
  press(ux, gestures, "ok:single", 1020);
  expectEq(ux.depth(), 3, "a refused value keeps the editor open");
  auto* editor = static_cast<NumberScene*>(ux.top());
  expectEq(std::any_cast<float>(editor->value()) == 3.0f, 1, "on the value it was refused");

  press(ux, gestures, "cancel:single", 1030);
  expectEq(ux.depth(), 2, "cancel still closes it");
  settings->press("ok:single");
  expectEq(ux.depth(), 1, "OK closes the list");
  expectEq(std::any_cast<float>(handed.at("final_pressure")) == 5.0f, 1,
           "and the refused value was never written");
}

// What a document holds is held to its row on the way in: a duration or a number outside its bounds
// answers the bound, and a choice naming no entry answers its default.
void aParsedValueIsHeldToItsRow() {
  NoGestures gestures;
  Ux ux;
  DurationRow hold(gestures, ux, "hold", "Hold", 90, 5, 0, 7200, false);
  NumberRow target(gestures, ux, "initial_pressure", "Start", 5.0f, 1, 0, 100, false);
  ChoiceRow unit(gestures, ux, "unit", "Unit", "hi", choices(), false);

  expectEq(std::any_cast<int32_t>(hold.parse(String("99999"))), 7200,
           "a duration past its top bound answers the bound");
  expectEq(std::any_cast<int32_t>(hold.parse(String("-5"))), 0,
           "and one under its floor answers that");
  expectEq(std::any_cast<float>(target.parse(String("400"))) == 100.0f, 1,
           "a number past its top bound answers the bound");
  expectText(std::any_cast<String>(unit.parse(String("middle"))).c_str(), "hi",
             "a choice naming no entry answers its default");
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
  cases.push_back({"a scene paints as soon as it is pushed", aScenePaintsAsSoonAsItIsPushed});
  cases.push_back({"a page paints again when the one above it pops",
                   aPagePaintsAgainWhenTheOneAboveItPops});
  cases.push_back({"a settings list moves and edits", aSettingsListMovesAndEdits});
  cases.push_back({"a settings list cancel puts the set back", aSettingsListCancelPutsTheSetBack});
  cases.push_back({"a settings list draws its rows", aSettingsListDrawsItsRows});
  cases.push_back({"a settings list edits every kind", aSettingsListEditsEveryKind});
  cases.push_back({"a settings list is seeded from the document", aSettingsListSeedsFromTheDocument});
  cases.push_back({"a row prints its value and opens its editor", aRowPrintsItsValueAndOpensItsEditor});
  cases.push_back({"a row stores the document's form", aRowStoresItsDocumentForm});
  cases.push_back(
      {"a row's check refuses what the set cannot use", aRowsCheckRefusesWhatTheSetCannotUse});
  cases.push_back(
      {"a settings list keeps a refused value out", aSettingsListKeepsARefusedValueOut});
  cases.push_back({"a parsed value is held to its row", aParsedValueIsHeldToItsRow});
}

}  // namespace gardener::gfx::test
