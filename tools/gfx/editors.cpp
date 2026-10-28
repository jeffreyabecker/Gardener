// The value editors: a number's stepping and its band, the choice scene's ring and the rows it
// draws, and the page every kind is drawn in.

#include <math.h>
#include <stdio.h>

#include <utility>

#include "suite.h"

namespace gardener::gfx::test {
namespace {

using namespace gardener::gfx::ui;

// The key a choice hands back, as the text a case compares; empty is the nullable nothing.
String keyOf(const ChoiceScene& scene) {
  return scene.isNull() ? String("") : std::any_cast<String>(scene.value());
}

// --- the number -------------------------------------------------------------

// A number scene with the hooks it answers opened up, so a case can step and draw it.
class NumberProbe : public NumberScene {
 public:
  using NumberScene::NumberScene;
  using EditorScene::handle;
  using EditorScene::hint;
  using EditorScene::nudge;
  using EditorScene::pageContents;
};

void aNumberStepsAndHolds() {
  NoGestures gestures;
  Ux ux;
  String label("Target");
  NumberProbe number(gestures, ux, label, 5, 1, 0, 10, false);
  expectText(number.text().c_str(), "5", "the number prints as it is");
  number.nudge(1);
  expectText(number.text().c_str(), "6", "up steps by one");
  for (int i = 0; i < 20; i++) number.nudge(1);
  expectText(number.text().c_str(), "10", "the top bound holds");
  for (int i = 0; i < 20; i++) number.nudge(-1);
  expectText(number.text().c_str(), "0", "the bottom bound holds");
  expectEq(number.isNull(), 0, "a bounded number always holds a value");
}

void aRealNumberStepsByItsIncrement() {
  NoGestures gestures;
  Ux ux;
  String label("Target");
  NumberProbe number(gestures, ux, label, 0.5f, 0.1f, 0, 1, false);
  number.nudge(1);
  expectText(number.text().c_str(), "0.6", "a tenth step lands on 0.6");
}

void aNumberPrintsThroughItsFormatter() {
  NoGestures gestures;
  Ux ux;
  String label("Target");
  NumberProbe number(gestures, ux, label, 5, 1, 0, 10, false, [](float value) {
    char printed[16];
    snprintf(printed, sizeof(printed), "%.1f kPa", static_cast<double>(value));
    return String(printed);
  });
  expectText(number.text().c_str(), "5.0 kPa", "the setting's formatter prints the number");
  number.nudge(1);
  expectText(number.text().c_str(), "6.0 kPa", "and what it prints follows the steps");
}

void aNullableNumberHoldsNothing() {
  NoGestures gestures;
  Ux ux;
  String label("Target");
  NumberProbe number(gestures, ux, label, NAN, 0.5f, 2, 8, true);
  expectText(number.text().c_str(), "--", "nothing prints dashes");
  expectEq(number.isNull(), 1, "and holds no value");
  number.nudge(-1);
  expectEq(number.isNull(), 1, "down at nothing stays nothing");
  number.nudge(1);
  expectEq(std::any_cast<float>(number.value()) == 2.0f, 1, "up from nothing enters the floor");
  number.nudge(-1);
  expectEq(number.isNull(), 1, "down at the floor clears back to nothing");
}

void aNumberWithNoBoundsStepsFreely() {
  NoGestures gestures;
  Ux ux;
  String label("Target");
  NumberProbe number(gestures, ux, label, 1, 1, -INFINITY, INFINITY, false);
  for (int i = 0; i < 5; i++) number.nudge(-1);
  expectText(number.text().c_str(), "-4", "an unbounded value steps past zero");
}

void aNumberBandDrawsTheValueAndTheBar() {
  NoGestures gestures;
  Ux ux;
  String label("Target");
  Canvas band = canvas.child(Rect(0, 0, 40, 24));
  reset();
  NumberProbe bounded(gestures, ux, label, 5, 1, 0, 10, false);
  bounded.pageContents()(band);
  expect(display.anyInk(0, 0, 40, 16), "the number is drawn in the top of the band");
  expect(display.anyInk(0, 16, 40, 8), "and the bar under it");

  reset();
  NumberProbe free(gestures, ux, label, 5, 1, -INFINITY, INFINITY, false);
  free.pageContents()(band);
  expect(display.anyInk(0, 0, 40, 24), "an unbounded number still draws");
  expectEq(display.inkCount(0, 18, 40, 6), 0, "and carries no bar");
}

// The check a row's declaration gave is asked on OK: a value it refuses keeps the editor open with
// the text it answered printed in the hint band, and a step clears that text.
void aRefusedValueKeepsTheEditorOpen() {
  NoGestures gestures;
  Ux ux;
  String label("Total time");
  NumberProbe number(gestures, ux, label, 30, 10, 0, 100, false);
  number.setValidator([](const std::any& proposed) {
    const bool holds = std::any_cast<float>(proposed) >= 60;
    return holds ? std::make_pair(true, String())
                 : std::make_pair(false, String("TOTAL < HOLDS"));
  });

  expectEq(number.handle("ok:single"), 1, "OK is answered");
  expectText(number.hint(), "TOTAL < HOLDS", "a refused value prints the text it answered");
  expectText(number.text().c_str(), "30", "and the editor stays on the value it was refused");

  expectEq(number.handle("up:single"), 1, "a step is answered");
  expectText(number.hint(), "\x1E/\x1F  OK=SAVE", "and clears the refusal");
  expectText(number.text().c_str(), "40", "the value stepped as it always does");
  number.handle("up:single");
  number.handle("up:single");
  expectText(number.text().c_str(), "60", "up to the value the check accepts");

  expectEq(number.handle("ok:single"), 1, "OK is answered again");
  expectEq(number.handle("up:single"), 0, "and an editor that closed answers nothing more");
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
  Ux ux;
  String label("Choice");
  ChoiceProbe choice(gestures, ux, label, "b", threeEntries(), false);
  expectText(keyOf(choice).c_str(), "b", "the stored key shows");
  expectEq(choice.nullable(), 0, "a setting that must hold a key says so");
  expectEq(choice.isNull(), 0, "and is holding one");
  expectText(choice.text().c_str(), "Beta", "and its title");
  choice.nudge(1);
  expectText(keyOf(choice).c_str(), "c", "up goes to the next entry");
  choice.nudge(1);
  expectText(keyOf(choice).c_str(), "a", "and wraps to the first");
  choice.nudge(-1);
  expectText(keyOf(choice).c_str(), "c", "down wraps to the last");
}

void aKeyNoEntryNamesShowsTheFirst() {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  ChoiceProbe choice(gestures, ux, label, "z", threeEntries(), false);
  choice.nudge(1);
  expectText(keyOf(choice).c_str(), "b", "an unknown key reads as the first entry");
}

void aNullableChoiceClearsBelowTheFirst() {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  ChoiceProbe choice(gestures, ux, label, "", threeEntries(), true);
  expectEq(choice.nullable(), 1, "the scene says it may hold nothing");
  expectEq(choice.isNull(), 1, "and starts holding nothing");
  expectText(keyOf(choice).c_str(), "", "nothing holds no key");
  expectText(choice.text().c_str(), "--", "and prints dashes");
  choice.nudge(-1);
  expectText(keyOf(choice).c_str(), "", "down at nothing stays nothing");
  choice.nudge(1);
  expectText(keyOf(choice).c_str(), "a", "up from nothing enters the first");
  choice.nudge(1);
  expectText(keyOf(choice).c_str(), "b", "and then steps");
  choice.nudge(-1);
  choice.nudge(-1);
  expectText(keyOf(choice).c_str(), "", "down at the first clears to nothing");
}

void aChoiceWithNoEntriesStepsNowhere() {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  ChoiceProbe choice(gestures, ux, label, "", std::vector<ChoiceItem>(), false);
  choice.nudge(1);
  expectText(keyOf(choice).c_str(), "", "an empty choice holds nothing");
}

void aChoiceClosesOnTheKeyShowing() {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  ChoiceProbe choice(gestures, ux, label, "b", threeEntries(), false);
  choice.nudge(1);
  expectEq(choice.press("ok:single"), 1, "OK is the scene's");
  expectEq(choice.isClosed(), 1, "OK closed the editor");
  expectText(keyOf(choice).c_str(), "c", "OK keeps the key showing");
}

void aChoiceCancelPutsBackTheKeyItOpenedOn() {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  ChoiceProbe choice(gestures, ux, label, "b", threeEntries(), false);
  choice.nudge(1);
  expectEq(choice.press("cancel:single"), 1, "Cancel is the scene's");
  expectText(keyOf(choice).c_str(), "b", "Cancel puts back the key it opened on");
}

void aChoiceForwardIsThePagesWork() {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  String heard;
  ChoiceProbe choice(gestures, ux, label, "b", threeEntries(), false,
                     [&heard](String& current) { heard = current; });
  expectEq(choice.press("forward:single"), 1, "a choice handed the work takes Forward");
  expectText(heard.c_str(), "b", "and runs it on the key showing");
  choice.nudge(1);
  choice.press("forward:single");
  expectText(heard.c_str(), "c", "the work follows the entry showing");

  ChoiceProbe none(gestures, ux, label, "b", threeEntries(), false);
  expectEq(none.press("forward:single"), 0, "a choice handed none drops Forward");

  bool ran = false;
  ChoiceProbe empty(gestures, ux, label, "", threeEntries(), true, [&ran](String&) { ran = true; });
  expectEq(empty.press("forward:single"), 0, "nothing showing leaves Forward unanswered");
  expectEq(ran, 0, "and runs no work");
}

void aChoiceBandInvertsTheEntryShowing() {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  ChoiceProbe choice(gestures, ux, label, "b", threeEntries(), false);
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
  Ux ux;
  String label("Choice");
  std::vector<ChoiceItem> bare = {{"a", "Alpha", nullptr}, {"b", "Beta", nullptr}};
  ChoiceProbe choice(gestures, ux, label, "a", bare, false);
  reset();
  Canvas band = canvas.child(Rect(0, 0, PageWidth, 36));
  choice.pageContents()(band);
  expect(display.anyInk(0, 6, PageWidth, 8), "a title is drawn where its entry has no paint");
}

void aChoiceBandPrintsDashesForNothing() {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  ChoiceProbe choice(gestures, ux, label, "", threeEntries(), true);
  reset();
  Canvas band = canvas.child(Rect(0, 0, PageWidth, 36));
  choice.pageContents()(band);
  expect(display.anyInk(0, 10, PageWidth, 16), "nothing prints the editor's dashes");
  expectEq(display.inkCount(0, 6, PageWidth, 4), 0, "and no row block is drawn");
}

// --- the bool ---------------------------------------------------------------

// A bool scene with the hooks it answers opened up, so a case can step and draw it.
class BoolProbe : public BoolScene {
 public:
  using BoolScene::BoolScene;
  using EditorScene::nudge;
  using EditorScene::pageContents;

  bool press(const char* gesture) { return handle(gesture); }
};

void aBoolTogglesAndReachesNothing() {
  NoGestures gestures;
  Ux ux;
  String label("Power");
  BoolProbe toggle(gestures, ux, label, "false", false);
  expectText(toggle.text().c_str(), "OFF", "a false setting prints OFF");
  expectEq(std::any_cast<bool>(toggle.value()), 0, "and is off");
  toggle.nudge(1);
  expectText(toggle.text().c_str(), "ON", "up turns it on");
  expectEq(std::any_cast<bool>(toggle.value()), 1, "and it reads on");
  toggle.nudge(1);
  expectText(toggle.text().c_str(), "OFF", "up again turns it back off");
  toggle.nudge(-1);
  expectText(toggle.text().c_str(), "ON", "down turns it on");

  BoolProbe nullable(gestures, ux, label, "", true);
  expectText(nullable.text().c_str(), "--", "a nullable setting holding nothing prints dashes");
  expectEq(nullable.isNull(), 1, "and holds no state");
  nullable.nudge(-1);
  expectEq(nullable.isNull(), 1, "down at nothing stays nothing");
  nullable.nudge(1);
  expectText(nullable.text().c_str(), "OFF", "up from nothing enters OFF");
  nullable.nudge(-1);
  expectEq(nullable.isNull(), 1, "down at OFF clears back to nothing");
}

void aBoolSwitchInvertsTheStateShowing() {
  NoGestures gestures;
  Ux ux;
  String label("Power");
  BoolProbe toggle(gestures, ux, label, "false", false);
  reset();
  Canvas band = canvas.child(Rect(0, 0, PageWidth, 36));
  toggle.pageContents()(band);
  expect(display.inkCount(14, 10, 36, 16) > display.inkCount(84, 10, 24, 16),
         "the label showing is inverted in its cell");
  expect(display.anyInk(84, 10, 24, 16), "and ON drew its label in the other cell");
}

void aBoolSwitchShowsTheNothingFirst() {
  NoGestures gestures;
  Ux ux;
  String label("Power");
  BoolProbe nullable(gestures, ux, label, "", true);
  reset();
  Canvas band = canvas.child(Rect(0, 0, PageWidth, 36));
  nullable.pageContents()(band);
  expect(display.inkCount(9, 10, 24, 16) > display.inkCount(46, 10, 36, 16),
         "nothing inverts the first of the three cells");
  expect(display.anyInk(95, 10, 24, 16), "and ON is the third");
}

void aChoiceBarShowsWhereTheWindowIs() {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  std::vector<ChoiceItem> six;
  for (int i = 0; i < 6; i++) six.push_back({String(i), "Row", nullptr});
  ChoiceProbe many(gestures, ux, label, String(5), six, false);
  reset();
  Canvas band = canvas.child(Rect(0, 0, PageWidth, 36));
  many.pageContents()(band);
  expect(display.anyInk(126, 2, 2, 8), "the scrollbar inks the track above the thumb");
  expectEq(display.inkCount(126, 15, 2, 6), 0, "and leaves the thumb bare");

  ChoiceProbe few(gestures, ux, label, "b", threeEntries(), false);
  reset();
  few.pageContents()(band);
  expectEq(display.inkCount(126, 2, 2, 4), 0, "a window of every entry carries no scrollbar");
}

// --- the duration -----------------------------------------------------------

// A duration scene with the hooks it answers opened up, so a case can step, switch stage and draw.
class DurationProbe : public DurationScene {
 public:
  using DurationScene::DurationScene;
  using EditorScene::nudge;
  using EditorScene::pageContents;

  bool press(const char* gesture) { return handle(gesture); }
  const char* caption() { return hint(); }
};

void aDurationStepsMinutesThenSeconds() {
  NoGestures gestures;
  Ux ux;
  String label("Hold");
  DurationProbe duration(gestures, ux, label, 90, 5, 0, 3600, false);
  expectText(duration.text().c_str(), "1m30s", "the seconds print compact");
  duration.nudge(1);
  expectText(duration.text().c_str(), "2m30s", "the minutes stage steps whole minutes");
  duration.press("forward:single");
  duration.nudge(1);
  expectText(duration.text().c_str(), "2m35s", "the seconds stage steps the setting's step");
  duration.nudge(-1);
  expectText(duration.text().c_str(), "2m30s", "and down takes it back");
  duration.press("back:single");
  duration.nudge(-1);
  expectText(duration.text().c_str(), "1m30s", "back returns to the minutes stage");
}

void aDurationHoldsItsBounds() {
  NoGestures gestures;
  Ux ux;
  String label("Hold");
  DurationProbe duration(gestures, ux, label, 30, 60, 120, 180, false);
  expectText(duration.text().c_str(), "30s", "the value opens as the setting gave it");
  duration.nudge(1);
  expectText(duration.text().c_str(), "2m", "a step from below the floor lands on it");
  duration.nudge(1);
  expectText(duration.text().c_str(), "3m", "and a step inside the bounds lands on its own");
  duration.nudge(1);
  expectText(duration.text().c_str(), "3m", "a step past the ceiling is held to it");
  expectText(duration.caption(), "\x1E/\x1F MIN  \x1A=NEXT", "the minutes stage says Forward next");
  duration.press("forward:single");
  expectText(duration.caption(), "\x1E/\x1F SEC  OK=SAVE", "the seconds stage says OK saves");
  duration.setValidator(
      [](const std::any&) { return std::make_pair(false, String("TOTAL < HOLDS")); });
  duration.press("ok:single");
  expectText(duration.caption(), "TOTAL < HOLDS", "a refusal prints over the stage's hint");
}

void aNullableDurationHoldsNothing() {
  NoGestures gestures;
  Ux ux;
  String label("Hold");
  DurationProbe duration(gestures, ux, label, NAN, 5, 10, 3600, true);
  expectText(duration.text().c_str(), "--", "nothing prints dashes");
  expectEq(duration.isNull(), 1, "and holds no value");
  duration.nudge(-1);
  expectEq(duration.isNull(), 1, "down at nothing stays nothing");
  duration.nudge(1);
  expectText(duration.text().c_str(), "10s", "up from nothing enters the floor");
  duration.nudge(-1);
  expectEq(duration.isNull(), 1, "down at the floor clears back to nothing");
}

void aDurationBandInvertsTheStage() {
  NoGestures gestures;
  Ux ux;
  String label("Hold");
  DurationProbe duration(gestures, ux, label, 90, 5, 0, 3600, false);
  reset();
  Canvas band = canvas.child(Rect(0, 0, PageWidth, 36));
  duration.pageContents()(band);
  expect(display.inkCount(20, 10, 24, 16) > display.inkCount(78, 10, 36, 16),
         "the minutes field is inverted while it is stepped");
  expect(display.anyInk(78, 10, 36, 16), "and the seconds field drew its title");

  duration.press("forward:single");
  reset();
  duration.pageContents()(band);
  expect(display.inkCount(78, 10, 36, 16) > display.inkCount(20, 10, 24, 16),
         "the inversion follows the stage to the seconds");
}

// --- the editor page --------------------------------------------------------

// The editor page itself, with a kind that draws nothing in the middle band.
class PageProbe : public EditorScene {
 public:
  PageProbe(GestureQueue& gestures, Ux& ux, String& label) : EditorScene(gestures, ux, label) {}
  using EditorScene::pageComponents;

 protected:
  void nudge(int8_t) override {}
};

void anEditorPageIsThreeBands() {
  reset();
  NoGestures gestures;
  Ux ux;
  String label("Number");
  PageProbe page(gestures, ux, label);
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
  cases.push_back({"a number prints through its formatter", aNumberPrintsThroughItsFormatter});
  cases.push_back({"a nullable number holds nothing", aNullableNumberHoldsNothing});
  cases.push_back({"a number with no bounds steps freely", aNumberWithNoBoundsStepsFreely});
  cases.push_back({"a number band draws the value and the bar", aNumberBandDrawsTheValueAndTheBar});
  cases.push_back({"a refused value keeps the editor open", aRefusedValueKeepsTheEditorOpen});
  cases.push_back({"a choice steps and wraps", aChoiceStepsAndWraps});
  cases.push_back({"a key no entry names shows the first", aKeyNoEntryNamesShowsTheFirst});
  cases.push_back({"a nullable choice clears below the first", aNullableChoiceClearsBelowTheFirst});
  cases.push_back({"a choice with no entries steps nowhere", aChoiceWithNoEntriesStepsNowhere});
  cases.push_back({"a choice closes on the key showing", aChoiceClosesOnTheKeyShowing});
  cases.push_back(
      {"a choice cancel puts back the key it opened on", aChoiceCancelPutsBackTheKeyItOpenedOn});
  cases.push_back({"a choice forward is the page's work", aChoiceForwardIsThePagesWork});
  cases.push_back({"a choice band inverts the entry showing", aChoiceBandInvertsTheEntryShowing});
  cases.push_back(
      {"a choice band prints the title without a paint", aChoiceBandPrintsTheTitleWithoutAPaint});
  cases.push_back({"a choice band prints dashes for nothing", aChoiceBandPrintsDashesForNothing});
  cases.push_back({"a choice bar shows where the window is", aChoiceBarShowsWhereTheWindowIs});
  cases.push_back({"a duration steps minutes then seconds", aDurationStepsMinutesThenSeconds});
  cases.push_back({"a duration holds its bounds", aDurationHoldsItsBounds});
  cases.push_back({"a nullable duration holds nothing", aNullableDurationHoldsNothing});
  cases.push_back({"a duration band inverts the stage", aDurationBandInvertsTheStage});
  cases.push_back({"a bool toggles and reaches nothing", aBoolTogglesAndReachesNothing});
  cases.push_back({"a bool switch inverts the state showing", aBoolSwitchInvertsTheStateShowing});
  cases.push_back({"a bool switch shows the nothing first", aBoolSwitchShowsTheNothingFirst});
  cases.push_back({"an editor page is three bands", anEditorPageIsThreeBands});
}

}  // namespace gardener::gfx::test
