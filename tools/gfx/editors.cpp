// The value editors: a number's stepping and its band, the choice scene's ring and the rows it
// draws, and the page every kind is drawn in.

#include <math.h>

#include "suite.h"

namespace gardener::gfx::test {
namespace {

using namespace gardener::gfx::ui;

// --- the number -------------------------------------------------------------

void aNumberStepsAndHolds() {
  NumberEditorComponent number(5, 1, 0, 10, false);
  expectText(number.text().c_str(), "5", "the number prints as it is");
  number.nudge(1);
  expectText(number.text().c_str(), "6", "up steps by one");
  for (int i = 0; i < 20; i++) number.nudge(1);
  expectText(number.text().c_str(), "10", "the top bound holds");
  for (int i = 0; i < 20; i++) number.nudge(-1);
  expectText(number.text().c_str(), "0", "the bottom bound holds");
  expectEq(number.hasValue(), 1, "a bounded number always holds a value");
}

void aRealNumberStepsByItsIncrement() {
  NumberEditorComponent number(0.5f, 0.1f, 0, 1, false);
  number.nudge(1);
  expectText(number.text().c_str(), "0.6", "a tenth step lands on 0.6");
}

void aNullableNumberHoldsNothing() {
  NumberEditorComponent number(NAN, 0.5f, 2, 8, true);
  expectText(number.text().c_str(), "--", "nothing prints dashes");
  expectEq(number.hasValue(), 0, "and holds no value");
  number.nudge(-1);
  expectEq(number.hasValue(), 0, "down at nothing stays nothing");
  number.nudge(1);
  expectEq(number.value() == 2.0f, 1, "up from nothing enters the floor");
  number.nudge(-1);
  expectEq(number.hasValue(), 0, "down at the floor clears back to nothing");
}

void aNumberWithNoBoundsStepsFreely() {
  NumberEditorComponent number(1, 1, -INFINITY, INFINITY, false);
  for (int i = 0; i < 5; i++) number.nudge(-1);
  expectText(number.text().c_str(), "-4", "an unbounded value steps past zero");
}

void aNumberBandDrawsTheValueAndTheBar() {
  reset();
  Canvas band = canvas.child(Rect(0, 0, 40, 24));
  NumberEditorComponent bounded(5, 1, 0, 10, false);
  bounded.draw()(band);
  expect(display.anyInk(0, 0, 40, 16), "the number is drawn in the top of the band");
  expect(display.anyInk(0, 16, 40, 8), "and the bar under it");

  reset();
  NumberEditorComponent free(5, 1, -INFINITY, INFINITY, false);
  free.draw()(band);
  expect(display.anyInk(0, 0, 40, 24), "an unbounded number still draws");
  expectEq(display.inkCount(0, 18, 40, 6), 0, "and carries no bar");
}

// --- the choice -------------------------------------------------------------

// A choice entry that inks a bar across the left of the row it is given.
Paint entry() {
  return [](Canvas& row) { row.draw(shapes::FillRect(0, 2, 10, 4, Ink)); };
}

std::vector<ChoiceItem> threeEntries() {
  return {{"a", "Alpha", entry()}, {"b", "Beta", entry()}, {"c", "Gamma", entry()}};
}

// A choice scene with the hooks it answers opened up, so a case can step and draw it.
class ChoiceProbe : public ChoiceScene {
 public:
  using ChoiceScene::ChoiceScene;
  using EditorScene::nudge;
  using EditorScene::pageContents;

  bool press(const char* gesture) { return handle(gesture); }
  bool isClosed() const { return closed_; }
};

void aChoiceStepsAndWraps() {
  NoGestures gestures;
  String label("Choice");
  ChoiceProbe choice(gestures, label, "b", threeEntries(), false);
  expectText(choice.value().c_str(), "b", "the stored key shows");
  expectText(choice.text().c_str(), "Beta", "and its title");
  choice.nudge(1);
  expectText(choice.value().c_str(), "c", "up goes to the next entry");
  choice.nudge(1);
  expectText(choice.value().c_str(), "a", "and wraps to the first");
  choice.nudge(-1);
  expectText(choice.value().c_str(), "c", "down wraps to the last");
}

void aKeyNoEntryNamesShowsTheFirst() {
  NoGestures gestures;
  String label("Choice");
  ChoiceProbe choice(gestures, label, "z", threeEntries(), false);
  choice.nudge(1);
  expectText(choice.value().c_str(), "b", "an unknown key reads as the first entry");
}

void aNullableChoiceClearsBelowTheFirst() {
  NoGestures gestures;
  String label("Choice");
  ChoiceProbe choice(gestures, label, "", threeEntries(), true);
  expectText(choice.value().c_str(), "", "nothing holds no key");
  expectText(choice.text().c_str(), "--", "and prints dashes");
  choice.nudge(-1);
  expectText(choice.value().c_str(), "", "down at nothing stays nothing");
  choice.nudge(1);
  expectText(choice.value().c_str(), "a", "up from nothing enters the first");
  choice.nudge(1);
  expectText(choice.value().c_str(), "b", "and then steps");
  choice.nudge(-1);
  choice.nudge(-1);
  expectText(choice.value().c_str(), "", "down at the first clears to nothing");
}

void aChoiceWithNoEntriesStepsNowhere() {
  NoGestures gestures;
  String label("Choice");
  ChoiceProbe choice(gestures, label, "", std::vector<ChoiceItem>(), false);
  choice.nudge(1);
  expectText(choice.value().c_str(), "", "an empty choice holds nothing");
}

void aChoiceClosesOnTheKeyShowing() {
  NoGestures gestures;
  String label("Choice");
  ChoiceProbe choice(gestures, label, "b", threeEntries(), false);
  choice.nudge(1);
  expectEq(choice.press("ok:single"), 1, "OK is the scene's");
  expectEq(choice.isClosed(), 1, "OK closed the editor");
  expectText(choice.value().c_str(), "c", "OK keeps the key showing");
}

void aChoiceCancelPutsBackTheKeyItOpenedOn() {
  NoGestures gestures;
  String label("Choice");
  ChoiceProbe choice(gestures, label, "b", threeEntries(), false);
  choice.nudge(1);
  expectEq(choice.press("cancel:single"), 1, "Cancel is the scene's");
  expectText(choice.value().c_str(), "b", "Cancel puts back the key it opened on");
}

void aChoiceBandInvertsTheEntryShowing() {
  NoGestures gestures;
  String label("Choice");
  ChoiceProbe choice(gestures, label, "b", threeEntries(), false);
  reset();
  Canvas band = canvas.child(Rect(0, 0, PageWidth, 36));
  choice.pageContents()(band);
  expectEq(display.inkCount(0, 14, PageWidth, 8), PageWidth * 8 - 10 * 4,
           "the entry showing inverts the whole of its row");
  expectEq(display.inkCount(0, 6, PageWidth, 8), 10 * 4,
           "an entry beside it is the paint alone");
}

void aChoiceBandPrintsTheTitleWithoutAPaint() {
  NoGestures gestures;
  String label("Choice");
  std::vector<ChoiceItem> bare = {{"a", "Alpha", nullptr}, {"b", "Beta", nullptr}};
  ChoiceProbe choice(gestures, label, "a", bare, false);
  reset();
  Canvas band = canvas.child(Rect(0, 0, PageWidth, 36));
  choice.pageContents()(band);
  expect(display.anyInk(0, 6, PageWidth, 8), "a title is drawn where its entry has no paint");
}

void aChoiceBandPrintsDashesForNothing() {
  NoGestures gestures;
  String label("Choice");
  ChoiceProbe choice(gestures, label, "", threeEntries(), true);
  reset();
  Canvas band = canvas.child(Rect(0, 0, PageWidth, 36));
  choice.pageContents()(band);
  expect(display.anyInk(0, 10, PageWidth, 16), "nothing prints the editor's dashes");
  expectEq(display.inkCount(0, 6, PageWidth, 4), 0, "and no row block is drawn");
}

// --- the editor page --------------------------------------------------------

// The editor page itself, with a kind that draws nothing in the middle band.
class PageProbe : public EditorScene {
 public:
  PageProbe(GestureQueue& gestures, String& label) : EditorScene(gestures, label) {}
  using EditorScene::pageComponents;

 protected:
  void nudge(int8_t) override {}
  String value() override { return ""; }
};

void anEditorPageIsThreeBands() {
  reset();
  NoGestures gestures;
  String label("Number");
  PageProbe page(gestures, label);
  Canvas band = canvas.child(Rect(0, 0, PageWidth, 52));
  page.pageComponents()(band);
  expect(display.anyInk(0, 0, PageWidth, 8), "the title is drawn in the top band");
  expectEq(display.inkCount(0, 8, PageWidth, 36), 0, "the contents band is left to the kind");
  expect(display.anyInk(0, 44, PageWidth, 8), "the hint is drawn in the bottom band");
}

}  // namespace

void addEditorCases(std::vector<Case>& cases) {
  cases.push_back({"a number steps and holds", aNumberStepsAndHolds});
  cases.push_back({"a real number steps by its increment", aRealNumberStepsByItsIncrement});
  cases.push_back({"a nullable number holds nothing", aNullableNumberHoldsNothing});
  cases.push_back({"a number with no bounds steps freely", aNumberWithNoBoundsStepsFreely});
  cases.push_back({"a number band draws the value and the bar", aNumberBandDrawsTheValueAndTheBar});
  cases.push_back({"a choice steps and wraps", aChoiceStepsAndWraps});
  cases.push_back({"a key no entry names shows the first", aKeyNoEntryNamesShowsTheFirst});
  cases.push_back({"a nullable choice clears below the first", aNullableChoiceClearsBelowTheFirst});
  cases.push_back({"a choice with no entries steps nowhere", aChoiceWithNoEntriesStepsNowhere});
  cases.push_back({"a choice closes on the key showing", aChoiceClosesOnTheKeyShowing});
  cases.push_back(
      {"a choice cancel puts back the key it opened on", aChoiceCancelPutsBackTheKeyItOpenedOn});
  cases.push_back({"a choice band inverts the entry showing", aChoiceBandInvertsTheEntryShowing});
  cases.push_back(
      {"a choice band prints the title without a paint", aChoiceBandPrintsTheTitleWithoutAPaint});
  cases.push_back({"a choice band prints dashes for nothing", aChoiceBandPrintsDashesForNothing});
  cases.push_back({"an editor page is three bands", anEditorPageIsThreeBands});
}

}  // namespace gardener::gfx::test
