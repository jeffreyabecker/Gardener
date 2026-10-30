// The value editors: a number's stepping and its band, the choice scene's ring and the rows it
// draws, and the page every kind is drawn in.

#include <math.h>
#include <stdio.h>

#include <utility>

#include <doctest.h>

#include <string>

#include "panel/page/BoolScene.h"
#include "panel/page/ChoiceScene.h"
#include "panel/page/DurationScene.h"
#include "panel/page/EditorScene.h"
#include "panel/page/NumberScene.h"
#include "panel/page/Ux.h"
#include "suite.h"

namespace gardener::gfx::test {
namespace {

using namespace gardener::panel::page;
using gardener::panel::GestureQueue;

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

TEST_CASE("a number steps and holds") {
  NoGestures gestures;
  Ux ux;
  String label("Target");
  NumberProbe number(gestures, ux, label, 5, 1, 0, 10, false);
  CHECK_EQ(std::string(number.text().c_str()), std::string("5"));
  number.nudge(1);
  CHECK_EQ(std::string(number.text().c_str()), std::string("6"));
  for (int i = 0; i < 20; i++) number.nudge(1);
  CHECK_EQ(std::string(number.text().c_str()), std::string("10"));
  for (int i = 0; i < 20; i++) number.nudge(-1);
  CHECK_EQ(std::string(number.text().c_str()), std::string("0"));
  CHECK_EQ(number.isNull(), 0);
}

TEST_CASE("a real number steps by its increment") {
  NoGestures gestures;
  Ux ux;
  String label("Target");
  NumberProbe number(gestures, ux, label, 0.5f, 0.1f, 0, 1, false);
  number.nudge(1);
  CHECK_EQ(std::string(number.text().c_str()), std::string("0.6"));
}

TEST_CASE("a number prints through its formatter") {
  NoGestures gestures;
  Ux ux;
  String label("Target");
  NumberProbe number(gestures, ux, label, 5, 1, 0, 10, false, [](float value) {
    char printed[16];
    snprintf(printed, sizeof(printed), "%.1f kPa", static_cast<double>(value));
    return String(printed);
  });
  CHECK_EQ(std::string(number.text().c_str()), std::string("5.0 kPa"));
  number.nudge(1);
  CHECK_EQ(std::string(number.text().c_str()), std::string("6.0 kPa"));
}

TEST_CASE("a nullable number holds nothing") {
  NoGestures gestures;
  Ux ux;
  String label("Target");
  NumberProbe number(gestures, ux, label, NAN, 0.5f, 2, 8, true);
  CHECK_EQ(std::string(number.text().c_str()), std::string("--"));
  CHECK_EQ(number.isNull(), 1);
  number.nudge(-1);
  CHECK_EQ(number.isNull(), 1);
  number.nudge(1);
  CHECK_EQ(std::any_cast<float>(number.value()) == 2.0f, 1);
  number.nudge(-1);
  CHECK_EQ(number.isNull(), 1);
}

TEST_CASE("a number with no bounds steps freely") {
  NoGestures gestures;
  Ux ux;
  String label("Target");
  NumberProbe number(gestures, ux, label, 1, 1, -INFINITY, INFINITY, false);
  for (int i = 0; i < 5; i++) number.nudge(-1);
  CHECK_EQ(std::string(number.text().c_str()), std::string("-4"));
}

// The number is the band: its ink sits centred in the contents band, with paper above, below and
// beside it rather than a bar under it.
TEST_CASE("a number band centres the value") {
  NoGestures gestures;
  Ux ux;
  String label("Target");
  NumberProbe number(gestures, ux, label, 5, 1, 0, 10, false);
  View band = canvas.child(Rect(0, 0, 40, 24));
  reset();
  number.pageContents()(band);
  CHECK(display.anyInk(0, 0, 40, 24));
  CHECK_EQ(display.inkCount(0, 0, 40, 4), 0);
  CHECK_EQ(display.inkCount(0, 20, 40, 4), 0);
  CHECK_EQ(display.inkCount(0, 0, 14, 24), 0);
  CHECK_EQ(display.inkCount(26, 0, 14, 24), 0);
}

// The check a row's declaration gave is asked on OK: a value it refuses keeps the editor open with
// the text it answered printed in the hint band, and a step clears that text.
TEST_CASE("a refused value keeps the editor open") {
  NoGestures gestures;
  Ux ux;
  String label("Total time");
  NumberProbe number(gestures, ux, label, 30, 10, 0, 100, false);
  number.setValidator([](const std::any& proposed) {
    const bool holds = std::any_cast<float>(proposed) >= 60;
    return holds ? std::make_pair(true, String())
                 : std::make_pair(false, String("TOTAL < HOLDS"));
  });

  CHECK_EQ(number.handle("ok:single"), 1);
  CHECK_EQ(std::string(number.hint()), std::string("TOTAL < HOLDS"));
  CHECK_EQ(std::string(number.text().c_str()), std::string("30"));

  CHECK_EQ(number.handle("up:single"), 1);
  CHECK_EQ(std::string(number.hint()), std::string("\x1E/\x1F  OK=SAVE"));
  CHECK_EQ(std::string(number.text().c_str()), std::string("40"));
  number.handle("up:single");
  number.handle("up:single");
  CHECK_EQ(std::string(number.text().c_str()), std::string("60"));

  CHECK_EQ(number.handle("ok:single"), 1);
  CHECK_EQ(number.handle("up:single"), 0);
}

// --- the choice -------------------------------------------------------------

std::vector<ChoiceItem> threeEntries() {
  return {{"a", "Alpha"}, {"b", "Beta"}, {"c", "Gamma"}};
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

TEST_CASE("a choice steps and wraps") {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  ChoiceProbe choice(gestures, ux, label, "b", threeEntries(), false);
  CHECK_EQ(std::string(keyOf(choice).c_str()), std::string("b"));
  CHECK_EQ(choice.nullable(), 0);
  CHECK_EQ(choice.isNull(), 0);
  CHECK_EQ(std::string(choice.text().c_str()), std::string("Beta"));
  choice.nudge(1);
  CHECK_EQ(std::string(keyOf(choice).c_str()), std::string("c"));
  choice.nudge(1);
  CHECK_EQ(std::string(keyOf(choice).c_str()), std::string("a"));
  choice.nudge(-1);
  CHECK_EQ(std::string(keyOf(choice).c_str()), std::string("c"));
}

TEST_CASE("a key no entry names shows the first") {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  ChoiceProbe choice(gestures, ux, label, "z", threeEntries(), false);
  choice.nudge(1);
  CHECK_EQ(std::string(keyOf(choice).c_str()), std::string("b"));
}

TEST_CASE("a nullable choice clears below the first") {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  ChoiceProbe choice(gestures, ux, label, "", threeEntries(), true);
  CHECK_EQ(choice.nullable(), 1);
  CHECK_EQ(choice.isNull(), 1);
  CHECK_EQ(std::string(keyOf(choice).c_str()), std::string(""));
  CHECK_EQ(std::string(choice.text().c_str()), std::string("--"));
  choice.nudge(-1);
  CHECK_EQ(std::string(keyOf(choice).c_str()), std::string(""));
  choice.nudge(1);
  CHECK_EQ(std::string(keyOf(choice).c_str()), std::string("a"));
  choice.nudge(1);
  CHECK_EQ(std::string(keyOf(choice).c_str()), std::string("b"));
  choice.nudge(-1);
  choice.nudge(-1);
  CHECK_EQ(std::string(keyOf(choice).c_str()), std::string(""));
}

TEST_CASE("a choice with no entries steps nowhere") {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  ChoiceProbe choice(gestures, ux, label, "", std::vector<ChoiceItem>(), false);
  choice.nudge(1);
  CHECK_EQ(std::string(keyOf(choice).c_str()), std::string(""));
}

TEST_CASE("a choice closes on the key showing") {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  ChoiceProbe choice(gestures, ux, label, "b", threeEntries(), false);
  choice.nudge(1);
  CHECK_EQ(choice.press("ok:single"), 1);
  CHECK_EQ(choice.isClosed(), 1);
  CHECK_EQ(std::string(keyOf(choice).c_str()), std::string("c"));
}

TEST_CASE("a choice cancel puts back the key it opened on") {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  ChoiceProbe choice(gestures, ux, label, "b", threeEntries(), false);
  choice.nudge(1);
  CHECK_EQ(choice.press("cancel:single"), 1);
  CHECK_EQ(std::string(keyOf(choice).c_str()), std::string("b"));
}

TEST_CASE("a choice forward is the pages work") {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  String heard;
  ChoiceProbe choice(gestures, ux, label, "b", threeEntries(), false,
                     [&heard](String& current) { heard = current; });
  CHECK_EQ(choice.press("forward:single"), 1);
  CHECK_EQ(std::string(heard.c_str()), std::string("b"));
  choice.nudge(1);
  choice.press("forward:single");
  CHECK_EQ(std::string(heard.c_str()), std::string("c"));

  ChoiceProbe none(gestures, ux, label, "b", threeEntries(), false);
  CHECK_EQ(none.press("forward:single"), 0);

  bool ran = false;
  ChoiceProbe empty(gestures, ux, label, "", threeEntries(), true, [&ran](String&) { ran = true; });
  CHECK_EQ(empty.press("forward:single"), 0);
  CHECK_EQ(ran, 0);
}

TEST_CASE("a choice band inverts the entry showing") {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  ChoiceProbe choice(gestures, ux, label, "b", threeEntries(), false);
  reset();
  View band = canvas.child(Rect(0, 0, PageWidth, 36));
  choice.pageContents()(band);
  CHECK(display.inkCount(0, 14, PageWidth, 8) > PageWidth * 8 / 2);
  CHECK(display.inkCount(0, 6, PageWidth, 8) < PageWidth * 8 / 2);
}

TEST_CASE("a choice band prints every title") {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  ChoiceProbe choice(gestures, ux, label, "a", threeEntries(), false);
  reset();
  View band = canvas.child(Rect(0, 0, PageWidth, 36));
  choice.pageContents()(band);
  CHECK(display.anyInk(0, 6, PageWidth, 8));
  CHECK(display.anyInk(0, 14, PageWidth, 8));
  CHECK(display.anyInk(0, 22, PageWidth, 8));
}

TEST_CASE("a choice band prints dashes for nothing") {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  ChoiceProbe choice(gestures, ux, label, "", threeEntries(), true);
  reset();
  View band = canvas.child(Rect(0, 0, PageWidth, 36));
  choice.pageContents()(band);
  CHECK(display.anyInk(0, 10, PageWidth, 16));
  CHECK_EQ(display.inkCount(0, 6, PageWidth, 4), 0);
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

TEST_CASE("a bool toggles and reaches nothing") {
  NoGestures gestures;
  Ux ux;
  String label("Power");
  BoolProbe toggle(gestures, ux, label, "false", false);
  CHECK_EQ(std::string(toggle.text().c_str()), std::string("OFF"));
  CHECK_EQ(std::any_cast<bool>(toggle.value()), 0);
  toggle.nudge(1);
  CHECK_EQ(std::string(toggle.text().c_str()), std::string("ON"));
  CHECK_EQ(std::any_cast<bool>(toggle.value()), 1);
  toggle.nudge(1);
  CHECK_EQ(std::string(toggle.text().c_str()), std::string("OFF"));
  toggle.nudge(-1);
  CHECK_EQ(std::string(toggle.text().c_str()), std::string("ON"));

  BoolProbe nullable(gestures, ux, label, "", true);
  CHECK_EQ(std::string(nullable.text().c_str()), std::string("--"));
  CHECK_EQ(nullable.isNull(), 1);
  nullable.nudge(-1);
  CHECK_EQ(nullable.isNull(), 1);
  nullable.nudge(1);
  CHECK_EQ(std::string(nullable.text().c_str()), std::string("OFF"));
  nullable.nudge(-1);
  CHECK_EQ(nullable.isNull(), 1);
}

TEST_CASE("a bool switch inverts the state showing") {
  NoGestures gestures;
  Ux ux;
  String label("Power");
  BoolProbe toggle(gestures, ux, label, "false", false);
  reset();
  View band = canvas.child(Rect(0, 0, PageWidth, 36));
  toggle.pageContents()(band);
  CHECK(display.inkCount(14, 10, 36, 16) > display.inkCount(84, 10, 24, 16));
  CHECK(display.anyInk(84, 10, 24, 16));
}

TEST_CASE("a bool switch shows the nothing first") {
  NoGestures gestures;
  Ux ux;
  String label("Power");
  BoolProbe nullable(gestures, ux, label, "", true);
  reset();
  View band = canvas.child(Rect(0, 0, PageWidth, 36));
  nullable.pageContents()(band);
  CHECK(display.inkCount(9, 10, 24, 16) > display.inkCount(46, 10, 36, 16));
  CHECK(display.anyInk(95, 10, 24, 16));
}

TEST_CASE("a choice bar shows where the window is") {
  NoGestures gestures;
  Ux ux;
  String label("Choice");
  std::vector<ChoiceItem> six;
  for (int i = 0; i < 6; i++) six.push_back({String(i), "Row"});
  ChoiceProbe many(gestures, ux, label, String(5), six, false);
  reset();
  View band = canvas.child(Rect(0, 0, PageWidth, 36));
  many.pageContents()(band);
  CHECK(display.anyInk(126, 2, 2, 8));
  CHECK_EQ(display.inkCount(126, 15, 2, 6), 0);

  ChoiceProbe few(gestures, ux, label, "b", threeEntries(), false);
  reset();
  few.pageContents()(band);
  CHECK_EQ(display.inkCount(126, 2, 2, 4), 0);
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

TEST_CASE("a duration steps minutes then seconds") {
  NoGestures gestures;
  Ux ux;
  String label("Hold");
  DurationProbe duration(gestures, ux, label, 90, 5, 0, 3600, false);
  CHECK_EQ(std::string(duration.text().c_str()), std::string("1m30s"));
  duration.nudge(1);
  CHECK_EQ(std::string(duration.text().c_str()), std::string("2m30s"));
  duration.press("forward:single");
  duration.nudge(1);
  CHECK_EQ(std::string(duration.text().c_str()), std::string("2m35s"));
  duration.nudge(-1);
  CHECK_EQ(std::string(duration.text().c_str()), std::string("2m30s"));
  duration.press("back:single");
  duration.nudge(-1);
  CHECK_EQ(std::string(duration.text().c_str()), std::string("1m30s"));
}

TEST_CASE("a duration holds its bounds") {
  NoGestures gestures;
  Ux ux;
  String label("Hold");
  DurationProbe duration(gestures, ux, label, 30, 60, 120, 180, false);
  CHECK_EQ(std::string(duration.text().c_str()), std::string("30s"));
  duration.nudge(1);
  CHECK_EQ(std::string(duration.text().c_str()), std::string("2m"));
  duration.nudge(1);
  CHECK_EQ(std::string(duration.text().c_str()), std::string("3m"));
  duration.nudge(1);
  CHECK_EQ(std::string(duration.text().c_str()), std::string("3m"));
  CHECK_EQ(std::string(duration.caption()), std::string("\x1E/\x1F MIN  \x1A=NEXT"));
  duration.press("forward:single");
  CHECK_EQ(std::string(duration.caption()), std::string("\x1E/\x1F SEC  OK=SAVE"));
  duration.setValidator(
      [](const std::any&) { return std::make_pair(false, String("TOTAL < HOLDS")); });
  duration.press("ok:single");
  CHECK_EQ(std::string(duration.caption()), std::string("TOTAL < HOLDS"));
}

TEST_CASE("a nullable duration holds nothing") {
  NoGestures gestures;
  Ux ux;
  String label("Hold");
  DurationProbe duration(gestures, ux, label, NAN, 5, 10, 3600, true);
  CHECK_EQ(std::string(duration.text().c_str()), std::string("--"));
  CHECK_EQ(duration.isNull(), 1);
  duration.nudge(-1);
  CHECK_EQ(duration.isNull(), 1);
  duration.nudge(1);
  CHECK_EQ(std::string(duration.text().c_str()), std::string("10s"));
  duration.nudge(-1);
  CHECK_EQ(duration.isNull(), 1);
}

TEST_CASE("a duration band inverts the stage") {
  NoGestures gestures;
  Ux ux;
  String label("Hold");
  DurationProbe duration(gestures, ux, label, 90, 5, 0, 3600, false);
  reset();
  View band = canvas.child(Rect(0, 0, PageWidth, 36));
  duration.pageContents()(band);
  CHECK(display.inkCount(20, 10, 24, 16) > display.inkCount(78, 10, 36, 16));
  CHECK(display.anyInk(78, 10, 36, 16));

  duration.press("forward:single");
  reset();
  duration.pageContents()(band);
  CHECK(display.inkCount(78, 10, 36, 16) > display.inkCount(20, 10, 24, 16));
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

TEST_CASE("an editor page is three bands") {
  reset();
  NoGestures gestures;
  Ux ux;
  String label("Number");
  PageProbe page(gestures, ux, label);
  View band = canvas.child(Rect(0, 0, PageWidth, 52));
  page.pageComponents()(band);
  CHECK(display.anyInk(0, 0, PageWidth, 8));
  CHECK_EQ(display.inkCount(0, 8, PageWidth, 36), 0);
  CHECK(display.anyInk(0, 44, PageWidth, 8));
}

}  // namespace

}  // namespace gardener::gfx::test
