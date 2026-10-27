// The scene stack: the frames, the callbacks a pop runs, and the one request a scene may hold until
// the call that asked for it returns.

#include <functional>
#include <memory>

#include "suite.h"

namespace gardener::gfx::test {
namespace {

using namespace gardener::gfx::ui;

// A scene that counts the calls the stack makes on it, and runs whatever a case asks it to.
class TestScene : public Scene {
 public:
  explicit TestScene(GestureQueue& gestures) : Scene(gestures) {}

  void activate() override {
    activations++;
    if (onActivate) onActivate();
  }
  void leave() override { leaves++; }

  int activations = 0;
  int leaves = 0;
  std::function<void()> onActivate;
  std::function<void()> onTick;

 protected:
  void tick(uint32_t) override {
    if (onTick) onTick();
  }
};

void theRootSceneIsPlacedAndDropped() {
  NoGestures gestures;
  TestScene root(gestures);
  Ux ux;
  expectEq(ux.reset(&root), 1, "a root scene shows");
  expectEq(ux.depth(), 1, "the root is the whole stack");
  expectEq(ux.top() == &root, 1, "and is on top");
  expectEq(root.activations, 1, "the root was activated");
  expectEq(ux.reset(nullptr), 0, "no root shows nothing");
  expectEq(ux.depth(), 0, "the stack is empty");
  expectEq(root.leaves, 1, "and the root was told it left");
}

void aPopHandsTheSceneBack() {
  NoGestures gestures;
  TestScene root(gestures);
  TestScene list(gestures);
  Ux ux;
  ux.reset(&root);
  Scene* popped = nullptr;
  expectEq(ux.push(list, [&popped](Scene& scene) { popped = &scene; }), 1, "a push is taken");
  expectEq(list.activations, 1, "the pushed scene was activated");
  expectEq(ux.pop(), 1, "a pop is taken");
  expectEq(popped == &list, 1, "the pusher's callback was handed the scene that popped");
  expectEq(list.leaves, 1, "and the scene was told it left");
}

void aPopWithNoCallbackReactivatesBelow() {
  NoGestures gestures;
  TestScene root(gestures);
  TestScene list(gestures);
  Ux ux;
  ux.reset(&root);
  ux.push(list);
  ux.pop();
  expectEq(root.activations, 2, "the scene below was activated again");
}

void theRootIsNotPopped() {
  NoGestures gestures;
  TestScene root(gestures);
  TestScene list(gestures);
  Ux ux;
  ux.reset(&root);
  expectEq(ux.pop(), 0, "a pop at the root is refused");
  ux.push(list);
  expectEq(ux.popToRoot(), 1, "the stack goes home");
  expectEq(ux.depth(), 1, "only the root is left");
  expectEq(list.leaves, 1, "and the scene above it was told it left");
}

void aPushFromActivateIsHeld() {
  NoGestures gestures;
  TestScene root(gestures);
  TestScene pusher(gestures);
  TestScene held(gestures);
  Ux ux;
  ux.reset(&root);
  pusher.onActivate = [&ux, &held] { ux.push(held); };
  ux.push(pusher);
  expectEq(ux.depth(), 2, "the push from inside activate was held");
  expectEq(held.activations, 0, "so the scene it pushed was not activated yet");
  ux.enter();
  expectEq(ux.depth(), 3, "the held push landed on the next call in");
  expectEq(held.activations, 1, "and the scene it pushed was activated");
}

void aPopFromAPollLandsAfterThePoll() {
  NoGestures gestures;
  TestScene root(gestures);
  TestScene editor(gestures);
  Ux ux;
  ux.reset(&root);
  ux.push(editor);
  editor.onTick = [&ux] { ux.pop(); };
  expectEq(ux.poll(canvas, 1000), 1, "the beat is taken");
  expectEq(ux.depth(), 1, "the pop landed after the poll");
  expectEq(ux.top() == &root, 1, "and the scene below is showing again");
}

void theStackHoldsEightFrames() {
  NoGestures gestures;
  TestScene root(gestures);
  TestScene other(gestures);
  Ux ux;
  ux.reset(&root);
  while (ux.depth() < Ux::MaxFrames) ux.push(other);
  expectEq(ux.depth(), Ux::MaxFrames, "the stack fills");
  expectEq(ux.push(other), 0, "a push past the cap is refused");
  expectEq(ux.depth(), Ux::MaxFrames, "and changes nothing");
}

// A value editor a settings case can push: it answers whatever value the case sets on it.
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

  Scene* scene(const std::any& value) override {
    editor_ = std::make_unique<TestEditor>(gestures_, value);
    return editor_.get();
  }

  TestEditor* editor() const { return editor_.get(); }

 private:
  GestureQueue& gestures_;
  String key_;
  String label_;
  String value_;
  std::unique_ptr<TestEditor> editor_;
};

// A settings list with the hooks it answers opened up, so a case can press the keys and draw it.
class SettingsProbe : public SettingsScene {
 public:
  using SettingsScene::SettingsScene;
  using SettingsScene::drawPage;

  bool press(const char* gesture) { return handle(gesture); }
};

void aSettingsListMovesAndEdits() {
  NoGestures gestures;
  Ux ux;
  String title("SETTINGS");
  SettingsProbe list(gestures, ux, title);
  TestRow target(gestures);
  TestRow unit(gestures);
  target.hold("target", "Target", "50");
  unit.hold("unit", "Unit", "kPa");
  list.set({&target, &unit});
  expectEq(list.selected(), 0, "the list starts on its first row");
  list.press("down:single");
  expectEq(list.selected(), 1, "down moves to the next row");
  list.press("down:single");
  expectEq(list.selected(), 0, "and wraps at the end");
  list.press("up:single");
  expectEq(list.selected(), 1, "up wraps the other way");

  Scene* handed = nullptr;
  TestScene root(gestures);
  ux.reset(&root);
  ux.push(list, [&handed](Scene& scene) { handed = &scene; });

  list.press("forward:single");
  expectEq(ux.depth(), 3, "forward pushes the row's editor");
  expectEq(target.editor() == nullptr, 1, "only the row showing opens");
  expectEq(unit.editor() != nullptr, 1, "and it is the selected row's");
  unit.editor()->set(String("psi"));
  ux.pop();
  expectEq(ux.depth(), 2, "the editor pops back to the list");
  const String edited = std::any_cast<String>(list.values().at("unit"));
  expectText(edited.c_str(), "psi", "what the editor handed back is kept under the row's key");

  list.press("ok:single");
  expectEq(handed == &list, 1, "OK hands the list back");
  const String kept = std::any_cast<String>(list.values().at("unit"));
  expectText(kept.c_str(), "psi", "with the set it settled on");
}

void aSettingsListCancelPutsTheSetBack() {
  NoGestures gestures;
  Ux ux;
  String title("SETTINGS");
  SettingsProbe list(gestures, ux, title);
  TestRow target(gestures);
  target.hold("target", "Target", "50");
  list.set({&target});
  Scene* handed = nullptr;
  TestScene root(gestures);
  ux.reset(&root);
  ux.push(list, [&handed](Scene& scene) { handed = &scene; });

  list.press("forward:single");
  target.editor()->set(String("70"));
  ux.pop();
  const String edited = std::any_cast<String>(list.values().at("target"));
  expectText(edited.c_str(), "70", "the edit landed");
  list.press("cancel:single");
  expectEq(handed == &list, 1, "Cancel hands the list back");
  const String back = std::any_cast<String>(list.values().at("target"));
  expectText(back.c_str(), "50", "and the set as it was opened");
}

void aSettingsListDrawsItsRows() {
  NoGestures gestures;
  Ux ux;
  String title("SETTINGS");
  SettingsProbe list(gestures, ux, title);
  TestRow target(gestures);
  TestRow unit(gestures);
  target.hold("target", "Target", "50");
  unit.hold("unit", "Unit", "kPa");
  list.set({&target, &unit});
  reset();
  list.drawPage(canvas);
  expect(display.anyInk(0, 0, PageWidth, 8), "the heading is drawn");
  expect(display.anyInk(0, 8, PageWidth, 8), "the first row is drawn under it");
  expect(display.inkCount(0, 8, PageWidth, 8) > display.inkCount(0, 16, PageWidth, 8),
         "and the row showing is inverted across its width");
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
  cases.push_back({"a settings list moves and edits", aSettingsListMovesAndEdits});
  cases.push_back({"a settings list cancel puts the set back", aSettingsListCancelPutsTheSetBack});
  cases.push_back({"a settings list draws its rows", aSettingsListDrawsItsRows});
}

}  // namespace gardener::gfx::test
