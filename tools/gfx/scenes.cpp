// The scene stack: the frames, the callbacks a pop runs, and the one request a scene may hold until
// the call that asked for it returns.

#include <functional>

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

}  // namespace

void addSceneCases(std::vector<Case>& cases) {
  cases.push_back({"the root scene is placed and dropped", theRootSceneIsPlacedAndDropped});
  cases.push_back({"a pop hands the scene back", aPopHandsTheSceneBack});
  cases.push_back({"a pop with no callback reactivates below", aPopWithNoCallbackReactivatesBelow});
  cases.push_back({"the root is not popped", theRootIsNotPopped});
  cases.push_back({"a push from activate is held", aPushFromActivateIsHeld});
  cases.push_back({"a pop from a poll lands after the poll", aPopFromAPollLandsAfterThePoll});
  cases.push_back({"the stack holds eight frames", theStackHoldsEightFrames});
}

}  // namespace gardener::gfx::test
