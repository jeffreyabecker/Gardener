// The scene stack and the settings list: the frames and the request a scene may hold until the call
// that asked for it returns, and the list that walks a row, opens the row's editor and hands the
// set back.

#include <functional>
#include <math.h>
#include <memory>
#include <utility>
#include <vector>

#include <doctest.h>

#include <string>

#include "Clock.h"
#include "panel/page/Overlay.h"
#include "panel/page/Scene.h"
#include "panel/page/Ux.h"
#include "panel/settings/BoolScene.h"
#include "panel/settings/DurationScene.h"
#include "panel/settings/SettingsRows.h"
#include "suite.h"

namespace gardener::gfx::test {
namespace {

using namespace gardener;
using gardener::GestureQueue;

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

TEST_CASE("the root scene is placed and dropped") {
  NoGestures gestures;
  Watched watched;
  Ux ux;
  CHECK_EQ(ux.reset(aScene(gestures, watched)), 1);
  CHECK_EQ(ux.depth(), 1);
  CHECK_EQ(ux.top() != nullptr, 1);
  CHECK_EQ(watched.activations, 1);
  CHECK_EQ(ux.reset(nullptr), 0);
  CHECK_EQ(ux.depth(), 0);
  CHECK_EQ(watched.leaves, 1);
}

TEST_CASE("a pop hands the scene back") {
  NoGestures gestures;
  Watched rootWatch;
  Watched listWatch;
  Ux ux;
  ux.reset(aScene(gestures, rootWatch));
  Scene* popped = nullptr;
  CHECK_EQ(ux.push(aScene(gestures, listWatch), [&popped](Scene& scene) { popped = &scene; }), 1);
  Scene* showing = ux.top();
  CHECK_EQ(listWatch.activations, 1);
  CHECK_EQ(ux.pop(), 1);
  CHECK_EQ(popped == showing, 1);
  CHECK_EQ(listWatch.leaves, 1);
}

TEST_CASE("a pop with no callback reactivates below") {
  NoGestures gestures;
  Watched rootWatch;
  Watched listWatch;
  Ux ux;
  ux.reset(aScene(gestures, rootWatch));
  ux.push(aScene(gestures, listWatch));
  ux.pop();
  CHECK_EQ(rootWatch.activations, 2);
}

TEST_CASE("the root is not popped") {
  NoGestures gestures;
  Watched rootWatch;
  Watched listWatch;
  Ux ux;
  ux.reset(aScene(gestures, rootWatch));
  CHECK_EQ(ux.pop(), 0);
  ux.push(aScene(gestures, listWatch));
  CHECK_EQ(ux.popToRoot(), 1);
  CHECK_EQ(ux.depth(), 1);
  CHECK_EQ(listWatch.leaves, 1);
}

TEST_CASE("pop to root with nothing showing answers false") {
  Ux ux;
  CHECK_EQ(ux.popToRoot(), 0);
}

TEST_CASE("a push from activate is held") {
  NoGestures gestures;
  Watched rootWatch;
  Watched pusherWatch;
  Watched heldWatch;
  Ux ux;
  ux.reset(aScene(gestures, rootWatch));
  auto pusher = aScene(gestures, pusherWatch);
  pusher->onActivate = [&ux, &gestures, &heldWatch] { ux.push(aScene(gestures, heldWatch)); };
  CHECK_EQ(ux.push(std::move(pusher)), 1);
  CHECK_EQ(ux.depth(), 2);
  CHECK_EQ(heldWatch.activations, 0);
  ux.enter(canvas, 400);
  CHECK_EQ(ux.depth(), 3);
  CHECK_EQ(heldWatch.activations, 1);
}

TEST_CASE("a pop from a poll lands after the poll") {
  NoGestures gestures;
  Watched rootWatch;
  Watched editorWatch;
  Ux ux;
  ux.reset(aScene(gestures, rootWatch));
  auto editor = aScene(gestures, editorWatch);
  editor->onTick = [&ux] { ux.pop(); };
  ux.push(std::move(editor));
  CHECK_EQ(ux.poll(canvas, 1000), 1);
  CHECK_EQ(ux.depth(), 1);
  CHECK_EQ(editorWatch.leaves, 1);
}

TEST_CASE("the stack holds eight frames") {
  NoGestures gestures;
  Watched watched;
  Ux ux;
  ux.reset(aScene(gestures, watched));
  while (ux.depth() < Ux::MaxFrames)
    ux.push(aScene(gestures, watched));
  CHECK_EQ(ux.depth(), Ux::MaxFrames);
  CHECK_EQ(ux.push(aScene(gestures, watched)), 0);
  CHECK_EQ(ux.depth(), Ux::MaxFrames);
}

// A page that inks the whole canvas it is given, so a case can see the overlay drawn over its
// bottom rows.
class InkedScene : public Scene {
 public:
  explicit InkedScene(GestureQueue& gestures) : Scene(gestures) {}

 protected:
  void drawPage(Canvas& own) override {
    own.draw(gfx::Rectangle(0, 0, own.width(), own.height(), Ink, true));
  }
};

// Where the overlay starts in these cases, and what it covers: the panel's own strip across the
// bottom of the glass.
constexpr int16_t OverlayTop = PageHeight - 12;
constexpr int16_t OverlayRows = PageHeight - OverlayTop;

// A paint that fills the canvas it is handed, so a case reads the overlay from the ink it leaves.
PaintNodeRef filledCell() {
  return callable([](Canvas& cell) {
    cell.draw(gfx::Rectangle(0, 0, cell.width(), cell.height(), Ink, true));
  });
}

TEST_CASE("the overlay is handed the rows from where it starts") {
  NoGestures gestures;
  Watched watched;
  Ux ux;
  reset();
  ux.setOverlay(OverlayTop, filledCell());
  ux.reset(aScene(gestures, watched));
  ux.poll(canvas, 300);
  CHECK_EQ(display.inkCount(0, 0, PageWidth, OverlayTop), 0);
  CHECK_EQ(display.inkCount(0, OverlayTop, PageWidth, OverlayRows), PageWidth * OverlayRows);
}

TEST_CASE("the page stops where the overlay starts") {
  NoGestures gestures;
  Ux ux;
  reset();
  ux.setOverlay(OverlayTop, callable([](Canvas&) {}));
  ux.reset(std::make_unique<InkedScene>(gestures));
  ux.poll(canvas, 300);
  CHECK_EQ(display.inkCount(0, 0, PageWidth, OverlayTop), PageWidth * OverlayTop);
  CHECK_EQ(display.inkCount(0, OverlayTop, PageWidth, OverlayRows), 0);
}

TEST_CASE("an empty paint is no overlay") {
  NoGestures gestures;
  Ux ux;
  reset();
  ux.setOverlay(OverlayTop, PaintNodeRef());
  CHECK_EQ(ux.overlayStart(), 0);
  ux.reset(std::make_unique<InkedScene>(gestures));
  ux.poll(canvas, 300);
  CHECK_EQ(display.inkCount(0, 0, PageWidth, PageHeight), PageWidth * PageHeight);
}

TEST_CASE("a stack showing nothing draws no overlay") {
  NoGestures gestures;
  Ux ux;
  reset();
  ux.setOverlay(OverlayTop, filledCell());
  CHECK_EQ(ux.poll(canvas, 300), 0);
  CHECK_EQ(display.inkCount(0, 0, PageWidth, PageHeight), 0);
}

// The strip is the node itself: five columns across the rows it is handed, the unit's suffix in the
// first and the gauge in the last.
TEST_CASE("the status strip draws its own cells") {
  SystemState state;
  state.unit = DisplayUnit::Kpa;
  state.batteryPresent = true;
  state.batteryPercent = 80;
  state.bluetoothConnected = true;
  Overlay overlay(state);
  reset();
  View band = canvas.child(Rect(0, 0, PageWidth, OverlayRows));
  overlay.draw(band);
  CHECK(display.anyInk(0, 0, PageWidth / 4, OverlayRows));
  // The rune is a bitmap at the top of the links' second column, an eighth of the strip in.
  CHECK(display.anyInk(static_cast<int16_t>(PageWidth / 4 + PageWidth / 8), 0, 16, 9));
  CHECK(display.anyInk(static_cast<int16_t>(PageWidth * 3 / 4), 0, PageWidth / 4, OverlayRows));

  // The marks name the Adafruit face themselves, so they survive a design whose codes start at
  // 0x20: under ttyp0 the idle circle would be nothing at all, while the unit's suffix takes the
  // default and is drawn in whatever the composition chose.
  setDefaultFamily(findFamily("ttyp0"));
  reset();
  overlay.draw(band);
  setDefaultFamily(nullptr);
  CHECK(display.anyInk(0, 0, PageWidth / 4, OverlayRows));
  CHECK(display.anyInk(static_cast<int16_t>(PageWidth / 2), 0, PageWidth / 4, OverlayRows));
}

// The gauge is the battery face's mark turned a quarter, so a full pack inks the bars an empty one
// leaves as the outline's paper.
TEST_CASE("the gauge inks a full pack more than an empty one") {
  const int16_t left = static_cast<int16_t>(PageWidth * 3 / 4);
  SystemState full;
  full.batteryPresent = true;
  full.batteryPercent = 100;
  Overlay filled(full);
  reset();
  View band = canvas.child(Rect(0, 0, PageWidth, OverlayRows));
  filled.draw(band);
  const int packed = display.inkCount(left, 0, PageWidth / 4, OverlayRows);

  SystemState flat;
  flat.batteryPresent = true;
  flat.batteryPercent = 0;
  Overlay empty(flat);
  reset();
  empty.draw(band);
  CHECK(packed > display.inkCount(left, 0, PageWidth / 4, OverlayRows));
}

// A chamber that is moving shows the face's own triangle: up for a pull, down for a vent, lit for the
// first 650 ms of each second and gone for the rest of it.
TEST_CASE("a moving chamber shows its triangle") {
  const int16_t stateLeft = static_cast<int16_t>(PageWidth / 2);
  const int16_t markRows = 3;
  const int16_t markWidth = 5;
  SystemState pulling;
  pulling.runState = RunState::Pulling;
  Overlay strip(pulling);
  host::reset();
  reset();
  View band = canvas.child(Rect(0, 0, PageWidth, OverlayRows));
  strip.draw(band);
  // Pointing up, so the ink is one pixel at the apex and five along the base.
  CHECK(display.inkCount(stateLeft, markRows, markWidth, 1) <
        display.inkCount(stateLeft, static_cast<int16_t>(markRows + 4), markWidth, 1));

  SystemState venting;
  venting.runState = RunState::Venting;
  Overlay down(venting);
  reset();
  down.draw(band);
  CHECK(display.inkCount(stateLeft, markRows, markWidth, 1) >
        display.inkCount(stateLeft, static_cast<int16_t>(markRows + 4), markWidth, 1));

  // Past the lit part of the second the mark is gone.
  host::advance(700);
  reset();
  down.draw(band);
  CHECK_EQ(display.inkCount(stateLeft, 0, PageWidth / 4, OverlayRows), 0);
  host::reset();
}

TEST_CASE("a scene paints as soon as it is pushed") {
  NoGestures gestures;
  Watched rootWatch;
  Ux ux;
  reset();
  auto root = aScene(gestures, rootWatch);
  root->onTick = [&ux, &gestures] { ux.push(std::make_unique<InkedScene>(gestures)); };
  ux.reset(std::move(root));
  ux.poll(canvas, 300);
  CHECK(display.anyInk(0, 0, PageWidth, PageHeight));
}

TEST_CASE("a page paints again when the one above it pops") {
  NoGestures gestures;
  Watched childWatch;
  Ux ux;
  reset();
  ux.reset(std::make_unique<InkedScene>(gestures));
  ux.poll(canvas, 300);
  CHECK(display.anyInk(0, 0, PageWidth, PageHeight));
  auto child = aScene(gestures, childWatch);
  child->onTick = [&ux] { ux.pop(); };
  ux.push(std::move(child));
  ux.poll(canvas, 320);
  CHECK(display.anyInk(0, 0, PageWidth, PageHeight));
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
class TestRow : public SettingConfig {
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

  String stored(const std::any& value) const override {
    return value.has_value() ? std::any_cast<String>(value) : String();
  }

  std::any parse(const String& stored) const override {
    return stored.isEmpty() ? defaultValue() : std::any(stored);
  }

  bool accept(std::any& value) const override {
    return !value.has_value() || std::any_cast<String>(&value) != nullptr;
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
  using SettingsScene::drawPage;
  using SettingsScene::SettingsScene;

  bool press(const char* gesture) { return handle(gesture); }
};

// One gesture through the stack: pushed, then taken by whatever is on top on this beat. A scene
// that closes itself pops on this call rather than inside its own handle, which is how the stack is
// driven.
void press(Ux& ux, GestureQueue& gestures, const char* gesture, uint32_t nowMs) {
  gestures.push(gesture);
  ux.poll(canvas, nowMs);
}

TEST_CASE("a settings list moves and edits") {
  GestureQueue gestures;
  Ux ux;
  String title("SETTINGS");
  TestRow target(gestures, ux);
  TestRow unit(gestures, ux);
  target.hold("target", "Target", "50");
  unit.hold("unit", "Unit", "kPa");
  std::vector<SettingConfig*> rows{&target, &unit};
  auto list = std::make_unique<SettingsProbe>(gestures, ux, title, rows, SettingValues());
  SettingsProbe* settings = list.get();
  CHECK_EQ(settings->selected(), 0);
  settings->press("down:single");
  CHECK_EQ(settings->selected(), 1);
  settings->press("down:single");
  CHECK_EQ(settings->selected(), 0);
  settings->press("up:single");
  CHECK_EQ(settings->selected(), 1);

  SettingValues handed;
  Scene* popped = nullptr;
  Watched rootWatch;
  ux.reset(aScene(gestures, rootWatch));
  ux.push(std::move(list), [&handed, &popped](Scene& scene) {
    popped = &scene;
    handed = static_cast<SettingsScene&>(scene).values();
  });

  press(ux, gestures, "forward:single", 1000);
  CHECK_EQ(ux.depth(), 3);
  auto* editor = static_cast<TestEditor*>(ux.top());
  editor->set(String("psi"));
  ux.pop();
  CHECK_EQ(ux.depth(), 2);

  press(ux, gestures, "ok:single", 1010);
  CHECK_EQ(ux.depth(), 1);
  CHECK_EQ(popped == settings, 1);
  const String edited = std::any_cast<String>(handed.at("unit"));
  CHECK_EQ(std::string(edited.c_str()), std::string("psi"));
  const String untouched = std::any_cast<String>(handed.at("target"));
  CHECK_EQ(std::string(untouched.c_str()), std::string("50"));
}

TEST_CASE("a settings list cancel puts the set back") {
  GestureQueue gestures;
  Ux ux;
  String title("SETTINGS");
  TestRow target(gestures, ux);
  target.hold("target", "Target", "50");
  std::vector<SettingConfig*> rows{&target};
  auto list = std::make_unique<SettingsProbe>(gestures, ux, title, rows, SettingValues());
  SettingsProbe* settings = list.get();
  SettingValues handed;
  Watched rootWatch;
  ux.reset(aScene(gestures, rootWatch));
  ux.push(std::move(list),
          [&handed](Scene& scene) { handed = static_cast<SettingsScene&>(scene).values(); });

  settings->press("forward:single");
  static_cast<TestEditor*>(ux.top())->set(String("70"));
  ux.pop();
  press(ux, gestures, "cancel:single", 1000);
  const String back = std::any_cast<String>(handed.at("target"));
  CHECK_EQ(std::string(back.c_str()), std::string("50"));
}

TEST_CASE("a settings list draws its rows") {
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
  CHECK(display.anyInk(0, 0, PageWidth, 8));
  CHECK(display.anyInk(0, 8, PageWidth, 8));
  CHECK(display.inkCount(0, 8, PageWidth, 8) > display.inkCount(0, 16, PageWidth, 8));
}

TEST_CASE("a settings list rebuilds the row an editor changed") {
  GestureQueue gestures;
  Ux ux;
  String title("SETTINGS");
  TestRow target(gestures, ux);
  target.hold("target", "Target", "50");
  std::vector<SettingConfig*> rows{&target};
  auto list = std::make_unique<SettingsProbe>(gestures, ux, title, rows, SettingValues());
  SettingsProbe* settings = list.get();
  Watched rootWatch;
  ux.reset(aScene(gestures, rootWatch));
  ux.push(std::move(list));

  // The row the keys are on, as its cell stands before the edit.
  reset();
  settings->drawPage(canvas);
  const int before = display.inkCount(0, 8, PageWidth, 8);

  press(ux, gestures, "forward:single", 1000);
  static_cast<TestEditor*>(ux.top())->set(String("WIDER"));
  ux.pop();
  CHECK_EQ(ux.depth(), 2);
  CHECK_EQ(settings->selected(), 0);
  const String edited = std::any_cast<String>(settings->values().at("target"));
  CHECK_EQ(std::string(edited.c_str()), std::string("WIDER"));

  // A row prints its label and not the value it holds, so the edited cell inks what it inked
  // before the edit; the list is rebuilt all the same, and a list built from the set the scene
  // hands back draws the same picture.
  reset();
  settings->drawPage(canvas);
  const int rebuilt = display.inkCount(0, 8, PageWidth, 8);
  CHECK(rebuilt > 0);
  CHECK_EQ(rebuilt, before);
  SettingsProbe fresh(gestures, ux, title, rows, settings->values());
  reset();
  fresh.drawPage(canvas);
  CHECK_EQ(display.inkCount(0, 8, PageWidth, 8), rebuilt);
}

// The two entries the choice row offers, and the keys they store.
std::vector<gfx::RowList::Row> choices() {
  return {gfx::RowList::textRow("lo", "Low"), gfx::RowList::textRow("hi", "High")};
}

TEST_CASE("a choice row prints its mark before its label") {
  NoGestures gestures;
  Ux ux;
  String title("SETTINGS");
  // Two marks, each one ink pixel in its top-left corner so where it lands is the whole question:
  // an eight-row one that fills its row, and a four-row one that has to be centred in it. The first
  // row carries none and is the one the keys are on.
  ChoiceSettingCfg plain(gestures, ux, "plain", "Plain", "lo", choices(), false);
  ChoiceSettingCfg tall(gestures, ux, "tall", "Tall", "lo", choices(), false,
                        "080800800000000000000000");
  ChoiceSettingCfg shortMark(gestures, ux, "short", "Short", "lo", choices(), false,
                             "08040080000000");
  SettingsProbe list(gestures, ux, title, {&plain, &tall, &shortMark}, {});
  reset();
  list.drawPage(canvas);

  // The heading takes the first eight rows, so the rows start at 8, 16 and 24.
  CHECK(display.anyInk(0, 8, 4, 4));           // the row the keys are on, inverted across its cell
  CHECK_EQ(display.inkCount(0, 16, 8, 8), 1);  // the eight-row mark, at the top of its row
  CHECK(display.anyInk(0, 16, 1, 1));
  CHECK_EQ(display.inkCount(0, 24, 8, 2), 0);  // the four-row one is centred: two rows above it
  CHECK_EQ(display.inkCount(0, 24, 8, 8), 1);
  CHECK(display.anyInk(0, 26, 1, 1));

  // The mark takes its own columns and the label follows them, rather than starting over it.
  CHECK(display.anyInk(8, 16, PageWidth - 8, 8));
}

TEST_CASE("a settings list edits every kind") {
  GestureQueue gestures;
  Ux ux;
  String title("SETTINGS");
  NumericSettingCfg target(gestures, ux, "target", "Target", 5.0f, 1, 0, 10, false);
  BoolSettingCfg power(gestures, ux, "power", "Power", false);
  ChoiceSettingCfg unit(gestures, ux, "unit", "Unit", "lo", choices(), false);
  DurationSettingCfg hold(gestures, ux, "hold", "Hold", 90, 5, 0, 3600, false);
  std::vector<SettingConfig*> rows{&target, &power, &unit, &hold};
  auto list = std::make_unique<SettingsScene>(gestures, ux, title, rows, SettingValues());

  SettingValues handed;
  Watched rootWatch;
  ux.reset(aScene(gestures, rootWatch));
  ux.push(std::move(list),
          [&handed](Scene& scene) { handed = static_cast<SettingsScene&>(scene).values(); });

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
  press(ux, gestures, "down:single", now += 10);
  press(ux, gestures, "ok:single", now += 10);

  // The duration: one step in the minutes stage takes 1m30s to 2m30s.
  press(ux, gestures, "down:single", now += 10);
  press(ux, gestures, "forward:single", now += 10);
  press(ux, gestures, "up:single", now += 10);
  press(ux, gestures, "ok:single", now += 10);

  press(ux, gestures, "ok:single", now += 10);

  CHECK_EQ(ux.depth(), 1);
  CHECK_EQ(std::any_cast<float>(handed.at("target")) == 7.0f, 1);
  CHECK_EQ(std::any_cast<bool>(handed.at("power")), 1);
  const String chosen = std::any_cast<String>(handed.at("unit"));
  CHECK_EQ(std::string(chosen.c_str()), std::string("hi"));
  CHECK_EQ(std::any_cast<int32_t>(handed.at("hold")), 150);
}

// Each kind's row opens an editor over the value it holds, and that editor hands the same kind of
// value back: the row and its scene agree about what the setting holds.
TEST_CASE("a row opens an editor over the value it holds") {
  NoGestures gestures;
  Ux ux;
  NumericSettingCfg number(gestures, ux, "target", "Target", 5.0f, 1, 0, 10, false);
  BoolSettingCfg power(gestures, ux, "power", "Power", true);
  ChoiceSettingCfg unit(gestures, ux, "unit", "Unit", "hi", choices(), false);
  DurationSettingCfg hold(gestures, ux, "hold", "Hold", 90, 5, 0, 3600, false);
  NumericSettingCfg unset(gestures, ux, "ceiling", "Ceiling", NAN, 1, 0, 10, true);

  CHECK_EQ(std::any_cast<float>(number.scene(number.defaultValue())->value()) == 5.0f, 1);
  CHECK_EQ(std::any_cast<bool>(power.scene(power.defaultValue())->value()), 1);
  CHECK_EQ(std::string(std::any_cast<String>(unit.scene(unit.defaultValue())->value()).c_str()),
           std::string("hi"));
  CHECK_EQ(std::any_cast<int32_t>(hold.scene(hold.defaultValue())->value()), 90);

  CHECK_EQ(unset.defaultValue().has_value(), 0);
  CHECK_EQ(unset.scene(unset.defaultValue())->isNull(), 1);
}

// The unit a pressure row steps by is the device's, read when the list or the editor asks rather
// than kept from when the row was declared: a unit changed in the device's own list reaches the
// pressures a routine declared when the image loaded.
TEST_CASE("a pressure row follows the unit the device shows") {
  NoGestures gestures;
  Ux ux;
  SystemState state;
  state.unit = DisplayUnit::Kpa;
  PressureSettingCfg target(gestures, ux, "target", "Target", state, 20.0f, 1.0f);

  CHECK_EQ(target.step() == 1.0f, 1);

  state.unit = DisplayUnit::InHg;
  CHECK_EQ(fabsf(target.step() - 1.0f / 0.29529991f) < 0.001f, 1);
}

// The step, the bounds and the entries each row was declared with, which are the rules its editor
// moves within. What a record names them — the kind word and the nil a kind with none of them
// answers — is the binding's to say, so that is not read here.
TEST_CASE("a row answers the step, bounds and entries it was built from") {
  NoGestures gestures;
  Ux ux;
  NumericSettingCfg target(gestures, ux, "target", "Target", 5.0f, 2, 1, 9, false);
  ChoiceSettingCfg unit(gestures, ux, "unit", "Unit", "hi", choices(), false);
  DurationSettingCfg hold(gestures, ux, "hold", "Hold", 90, 5, 30, 3600, false);

  CHECK_EQ(target.step() == 2.0f, 1);
  CHECK_EQ(target.min() == 1.0f, 1);
  CHECK_EQ(target.max() == 9.0f, 1);

  // A choice's entries are the list's own rows, each keyed by the value it stores.
  const std::vector<gfx::RowList::Row>& entries = unit.entries();
  CHECK_EQ(entries.size(), choices().size());
  CHECK_EQ(std::string(entries.front().value.c_str()), std::string("lo"));

  CHECK_EQ(hold.step() == 5.0f, 1);
  CHECK_EQ(hold.min() == 30.0f, 1);
  CHECK_EQ(hold.max() == 3600.0f, 1);
}

// The list is seeded from the set the document holds: what the set says for a key it carries, the
// row's own default for a key it does not, and nothing for a key the set holds nothing for. What
// goes in is what Back and Cancel put back, so the list opened on it.
TEST_CASE("a settings list seeds from the document") {
  GestureQueue gestures;
  Ux ux;
  String title("SETTINGS");
  BoolSettingCfg power(gestures, ux, "power", "Power", true);
  NumericSettingCfg target(gestures, ux, "target", "Target", 5.0f, 1, 0, 10, false);
  NumericSettingCfg ceiling(gestures, ux, "ceiling", "Ceiling", 9.0f, 1, 0, 10, true);
  SettingValues held;
  held["power"] = false;
  held["ceiling"] = std::any();
  std::vector<SettingConfig*> rows{&power, &target, &ceiling};
  auto list = std::make_unique<SettingsProbe>(gestures, ux, title, rows, held);
  SettingsProbe* settings = list.get();

  CHECK_EQ(std::any_cast<bool>(settings->values().at("power")), 0);
  CHECK_EQ(std::any_cast<float>(settings->values().at("target")) == 5.0f, 1);
  CHECK_EQ(settings->values().at("ceiling").has_value(), 0);

  Watched rootWatch;
  ux.reset(aScene(gestures, rootWatch));
  ux.push(std::move(list), nullptr);

  settings->press("forward:single");
  CHECK_EQ(std::string(static_cast<BoolScene*>(ux.top())->text().c_str()), std::string("OFF"));
  ux.pop();
  settings->press("down:single");
  settings->press("down:single");
  settings->press("forward:single");
  CHECK_EQ(ux.top()->isNull(), 1);
  ux.pop();
  CHECK_EQ(std::any_cast<bool>(settings->values().at("power")), 0);
}

// Each kind's row writes the document's form of a value, which is not the form the list prints, and
// reads that text back as the value it wrote.
TEST_CASE("a row stores its document form") {
  NoGestures gestures;
  Ux ux;
  NumericSettingCfg number(gestures, ux, "target", "Target", 5.0f, 1, 0, 10, false);
  BoolSettingCfg power(gestures, ux, "power", "Power", true);
  ChoiceSettingCfg unit(gestures, ux, "unit", "Unit", "hi", choices(), false);
  DurationSettingCfg hold(gestures, ux, "hold", "Hold", 90, 5, 0, 3600, false);
  NumericSettingCfg unset(gestures, ux, "ceiling", "Ceiling", NAN, 1, 0, 10, true);

  CHECK_EQ(std::string(number.stored(std::any(7.5f)).c_str()), std::string("7.50"));
  CHECK_EQ(std::string(power.stored(std::any(true)).c_str()), std::string("on"));
  CHECK_EQ(std::string(unit.stored(std::any(String("lo"))).c_str()), std::string("lo"));
  CHECK_EQ(std::string(hold.stored(std::any(static_cast<int32_t>(150))).c_str()),
           std::string("150"));

  CHECK_EQ(std::any_cast<float>(number.parse(String("7.50"))) == 7.5f, 1);
  CHECK_EQ(std::any_cast<bool>(power.parse(String("off"))), 0);
  CHECK_EQ(std::any_cast<bool>(power.parse(String("YES"))), 1);
  CHECK_EQ(std::string(std::any_cast<String>(unit.parse(String("lo"))).c_str()), std::string("lo"));
  CHECK_EQ(std::any_cast<int32_t>(hold.parse(String("150"))), 150);

  CHECK_EQ(std::string(number.stored(std::any()).c_str()), std::string(""));
  CHECK_EQ(std::string(unset.stored(std::any(NAN)).c_str()), std::string(""));
  CHECK_EQ(std::any_cast<float>(number.parse(String())) == 5.0f, 1);
  CHECK_EQ(unset.parse(String()).has_value(), 0);
  CHECK_EQ(std::any_cast<bool>(power.parse(String("maybe"))), 1);
}

// The check 5_ramped_static's declaration gives its total: the profile is the two holds plus a
// ramp, so the total covers the holds. This is the declaration's form of a check — the declaration,
// the set in force and the value proposed — which is what a row's constructor is handed.
std::pair<bool, String> totalCoversHolds(const std::vector<SettingConfig*>&,
                                         const SettingValues& values, const std::any& proposed) {
  const auto initial = values.find("initial_duration");
  const auto target = values.find("target_duration");
  const int32_t holds = (initial == values.end() ? 0 : std::any_cast<int32_t>(initial->second)) +
                        (target == values.end() ? 0 : std::any_cast<int32_t>(target->second));
  const int32_t total = std::any_cast<int32_t>(proposed);
  return total < holds ? std::make_pair(false, String("TOTAL < HOLDS"))
                       : std::make_pair(true, String());
}

// The check 2_ramped_interval's declaration gives its final pressure: a ramp ends no lower than it
// starts.
std::pair<bool, String> finalUnderStart(const std::vector<SettingConfig*>&,
                                        const SettingValues& values, const std::any& proposed) {
  const auto initial = values.find("initial_pressure");
  const float start = initial == values.end() ? 0.0f : std::any_cast<float>(initial->second);
  const float end = std::any_cast<float>(proposed);
  return end < start ? std::make_pair(false, String("FINAL < START"))
                     : std::make_pair(true, String());
}

// A row's check curried over the declaration and the set the list holds refuses the values a
// routine cannot run and accepts the ones it can, and a row the declaration gave none answers none.
TEST_CASE("a rows check refuses what the set cannot use") {
  NoGestures gestures;
  Ux ux;
  DurationSettingCfg initial(gestures, ux, "initial_duration", "Start time", 60, 1, 0, 7200, false);
  DurationSettingCfg target(gestures, ux, "target_duration", "Target time", 60, 1, 0, 7200, false);
  DurationSettingCfg total(gestures, ux, "total_duration", "Total time", 300, 1, 1, 7200, false,
                           totalCoversHolds);
  NumericSettingCfg start(gestures, ux, "initial_pressure", "Start", 5.0f, 1, 0, 100, false);
  NumericSettingCfg last(gestures, ux, "final_pressure", "Final", 5.0f, 1, 0, 100, false, nullptr,
                         2, finalUnderStart);
  std::vector<SettingConfig*> rows{&initial, &target, &total, &start, &last};

  SettingValues values;
  values["initial_duration"] = static_cast<int32_t>(60);
  values["target_duration"] = static_cast<int32_t>(60);
  values["total_duration"] = static_cast<int32_t>(300);
  values["initial_pressure"] = 5.0f;
  values["final_pressure"] = 5.0f;

  const CurriedValidator holds = total.check(rows, values);
  CHECK_EQ(holds != nullptr, 1);
  if (!holds) return;
  std::pair<bool, String> refused = holds(std::any(static_cast<int32_t>(30)));
  CHECK_EQ(refused.first, 0);
  CHECK_EQ(std::string(refused.second.c_str()), std::string("TOTAL < HOLDS"));
  CHECK_EQ(holds(std::any(static_cast<int32_t>(400))).first, 1);

  const CurriedValidator pressure = last.check(rows, values);
  CHECK_EQ(pressure != nullptr, 1);
  if (!pressure) return;
  refused = pressure(std::any(3.0f));
  CHECK_EQ(refused.first, 0);
  CHECK_EQ(std::string(refused.second.c_str()), std::string("FINAL < START"));
  CHECK_EQ(pressure(std::any(5.0f)).first, 1);

  CHECK_EQ(initial.check(rows, values) == nullptr, 1);
}

// The list hands a row's check to the editor it opens, so a refused value keeps that editor on the
// stack and never reaches the set: the list closes on the value it was opened with.
TEST_CASE("a settings list keeps a refused value out") {
  GestureQueue gestures;
  Ux ux;
  String title("SETTINGS");
  NumericSettingCfg start(gestures, ux, "initial_pressure", "Start", 5.0f, 1, 0, 100, false);
  NumericSettingCfg last(gestures, ux, "final_pressure", "Final", 5.0f, 1, 0, 100, false, nullptr,
                         2, finalUnderStart);
  std::vector<SettingConfig*> rows{&start, &last};
  auto list = std::make_unique<SettingsProbe>(gestures, ux, title, rows, SettingValues());
  SettingsProbe* settings = list.get();

  SettingValues handed;
  Watched rootWatch;
  ux.reset(aScene(gestures, rootWatch));
  ux.push(std::move(list),
          [&handed](Scene& scene) { handed = static_cast<SettingsScene&>(scene).values(); });

  settings->press("down:single");
  settings->press("forward:single");
  CHECK_EQ(ux.depth(), 3);
  press(ux, gestures, "down:single", 1000);
  press(ux, gestures, "down:single", 1010);
  press(ux, gestures, "ok:single", 1020);
  CHECK_EQ(ux.depth(), 3);
  auto* editor = static_cast<NumberScene*>(ux.top());
  CHECK_EQ(std::any_cast<float>(editor->value()) == 3.0f, 1);

  press(ux, gestures, "cancel:single", 1030);
  CHECK_EQ(ux.depth(), 2);
  settings->press("ok:single");
  CHECK_EQ(ux.depth(), 1);
  CHECK_EQ(std::any_cast<float>(handed.at("final_pressure")) == 5.0f, 1);
}

// What a document holds is held to its row on the way in: a duration or a number outside its bounds
// answers the bound, and a choice naming no entry answers its default.
TEST_CASE("a parsed value is held to its row") {
  NoGestures gestures;
  Ux ux;
  DurationSettingCfg hold(gestures, ux, "hold", "Hold", 90, 5, 0, 7200, false);
  NumericSettingCfg target(gestures, ux, "initial_pressure", "Start", 5.0f, 1, 0, 100, false);
  ChoiceSettingCfg unit(gestures, ux, "unit", "Unit", "hi", choices(), false);

  CHECK_EQ(std::any_cast<int32_t>(hold.parse(String("99999"))), 7200);
  CHECK_EQ(std::any_cast<int32_t>(hold.parse(String("-5"))), 0);
  CHECK_EQ(std::any_cast<float>(target.parse(String("400"))) == 100.0f, 1);
  CHECK_EQ(std::string(std::any_cast<String>(unit.parse(String("middle"))).c_str()),
           std::string("hi"));
}

}  // namespace

}  // namespace gardener::gfx::test
