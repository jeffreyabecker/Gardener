// The device's own settings list as a model: the three rows it shows, the set it reads from the
// document, and the set it writes back. The store is the suite's in-memory Library, so a case owns
// the document and counts the writes.

#include <doctest.h>

#include <Arduino.h>

#include <any>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/SystemState.h"
#include "fakes/MemoryLibrary.h"
#include "fakes/Parts.h"
#include "machine/Machine.h"
#include "panel/GestureQueue.h"
#include "panel/page/Ux.h"
#include "panel/parts/BuzzerPlayer.h"
#include "panel/settings/SettingsRows.h"
#include "panel/settings/SettingsScene.h"
#include "panel/settings/SystemSettings.h"
#include "storage/DeviceSettings.h"
#include "storage/JingleLibrary.h"
#include "suite.h"

namespace {

using gardener::BuzzerPlayer;
using gardener::DeviceSettings;
using gardener::GestureQueue;
using gardener::JingleLibrary;
using gardener::Scene;
using gardener::SettingConfig;
using gardener::SettingsScene;
using gardener::SettingValues;
using gardener::SystemSettings;
using gardener::Ux;
using gardener::test::FakeBuzzer;
using gardener::test::MemoryLibrary;
using gardener::test::RecordingLogger;

constexpr const char* SettingsPath = "/cfg/settings.ini";
constexpr const char* JinglesPath = "/cfg/jingles.ini";

constexpr const char* SettingsDocument = "[device]\n"
                                         "unit=psi\n"
                                         "jingle=forelle\n"
                                         "max_duration=600\n";

constexpr const char* JinglesDocument = "[poacher]\n"
                                        "short_title=Poacher\n"
                                        "notes=E5:200 G5:200\n"
                                        "\n"
                                        "[forelle]\n"
                                        "short_title=Forelle\n"
                                        "notes=D5:200\n";

String textOf(const SettingValues& values, const char* key) {
  const auto held = values.find(key);
  if (held == values.end() || !held->second.has_value()) return String();
  return std::any_cast<String>(held->second);
}

int32_t secondsOf(const SettingValues& values, const char* key) {
  const auto held = values.find(key);
  if (held == values.end() || !held->second.has_value()) return -1;
  return std::any_cast<int32_t>(held->second);
}

struct Bench {
  explicit Bench(const char* settingsDocument = SettingsDocument,
                 const char* jinglesDocument = JinglesDocument) {
    if (settingsDocument != nullptr) library.put(SettingsPath, settingsDocument);
    if (jinglesDocument != nullptr) library.put(JinglesPath, jinglesDocument);
    settings.begin();
    jingles.begin();
  }

  MemoryLibrary library;
  RecordingLogger logger;
  gardener::SystemState state;
  DeviceSettings settings{&library, state};
  JingleLibrary jingles{&library, logger};
  FakeBuzzer buzzerPart;
  BuzzerPlayer buzzer{buzzerPart};
  GestureQueue gestures;
  Ux ux;
  SystemSettings system{gestures, ux, settings, jingles, buzzer};
};

// The device's list with the hooks it answers opened up, so a case can press the keys and draw it.
class SettingsProbe : public gardener::SettingsScene {
 public:
  using SettingsScene::drawPage;
  using SettingsScene::SettingsScene;

  bool press(const char* gesture) { return handle(gesture); }
};

// What the list is opened over: the list pops back to it, which is what the stack's root rule needs.
class RootScene : public Scene {
 public:
  using Scene::Scene;
};

}  // namespace

TEST_CASE("the system list shows the unit, the jingle and the maximum in that order") {
  Bench bench;
  const std::vector<SettingConfig*>& rows = bench.system.rows();
  REQUIRE_EQ(rows.size(), 3u);

  CHECK_EQ(std::string(rows[0]->key().c_str()), std::string("unit"));
  CHECK_EQ(std::string(rows[0]->label().c_str()), std::string("UNIT"));

  CHECK_EQ(std::string(rows[1]->key().c_str()), std::string("jingle"));
  CHECK_EQ(std::string(rows[1]->label().c_str()), std::string("JINGLE"));

  CHECK_EQ(std::string(rows[2]->key().c_str()), std::string("max_duration"));
  CHECK_EQ(std::string(rows[2]->label().c_str()), std::string("MAX"));
}

TEST_CASE("the maximum row cannot offer more than the machine grants") {
  Bench bench;
  const auto* maximum = static_cast<const gardener::NumericSettingCfg*>(bench.system.rows()[2]);
  CHECK_EQ(maximum->max(), static_cast<float>(gardener::CONTROL_MAX_RUN_MS / 1000));
}

TEST_CASE("the system list reads the document it edits") {
  Bench bench;
  const SettingValues values = bench.system.values();
  CHECK_EQ(std::string(textOf(values, "unit").c_str()), std::string("psi"));
  CHECK_EQ(std::string(textOf(values, "jingle").c_str()), std::string("forelle"));
  CHECK_EQ(secondsOf(values, "max_duration"), 600);
}

TEST_CASE("a stored jingle the library no longer holds reads as the first entry") {
  Bench bench("[device]\nunit=kpa\njingle=missing\n", JinglesDocument);
  CHECK_EQ(std::string(textOf(bench.system.values(), "jingle").c_str()), std::string("poacher"));
}

TEST_CASE("the settled set is written back through the document") {
  Bench bench;
  SettingValues values = bench.system.values();
  values["unit"] = std::any(String("inhg"));
  values["jingle"] = std::any(String("poacher"));
  values["max_duration"] = std::any(int32_t(1800));

  CHECK(bench.system.apply(values));
  CHECK_EQ(bench.library.writes(), 1);

  DeviceSettings again{&bench.library, bench.state};
  CHECK(again.begin());
  CHECK(again.unit() == gardener::DisplayUnit::InHg);
  CHECK_EQ(std::string(again.jingle().c_str()), std::string("poacher"));
  CHECK_EQ(again.maxDurationSeconds(), 1800);

  const std::string text = bench.library.textAt(SettingsPath);
  CHECK(text.find("unit=inhg") != std::string::npos);
  CHECK(text.find("jingle=poacher") != std::string::npos);
  CHECK(text.find("max_duration=1800") != std::string::npos);
}

TEST_CASE("a unit or jingle the device does not name is left alone") {
  Bench bench;
  SettingValues values = bench.system.values();
  values["unit"] = std::any(String("furlongs"));
  values["jingle"] = std::any(String("missing"));

  CHECK(bench.system.apply(values));
  CHECK_EQ(bench.library.writes(), 0);
  CHECK(bench.settings.unit() == gardener::DisplayUnit::Psi);
  CHECK_EQ(std::string(bench.settings.jingle().c_str()), std::string("forelle"));
}

TEST_CASE("applying the set the list opened on writes nothing") {
  Bench bench;
  CHECK(bench.system.apply(bench.system.values()));
  CHECK_EQ(bench.library.writes(), 0);
}

TEST_CASE("the device's own list edits a row and hands the set back") {
  Bench bench;
  SettingValues handed;
  bench.ux.reset(std::make_unique<RootScene>(bench.gestures));
  auto list = std::make_unique<SettingsProbe>(bench.gestures, bench.ux, String("SYSTEM"),
                                              bench.system.rows(), bench.system.values());
  SettingsProbe* scene = list.get();
  bench.ux.push(std::move(list), [&handed](Scene& popped) {
    handed = static_cast<SettingsScene&>(popped).values();
  });

  // The unit row is the one the keys open on. Forward opens its choice and Up steps it to the next
  // unit the device offers.
  CHECK_EQ(scene->selected(), 0);
  gardener::gfx::test::reset();
  scene->drawPage(gardener::gfx::test::canvas);
  const int before = gardener::gfx::test::display.inkCount(0, 8, gardener::gfx::test::PageWidth, 8);

  scene->press("forward:single");
  REQUIRE_EQ(bench.ux.depth(), 3);
  bench.gestures.push("up:single");
  bench.ux.poll(gardener::gfx::test::canvas, 1000);
  bench.gestures.push("ok:single");
  bench.ux.poll(gardener::gfx::test::canvas, 1010);
  CHECK_EQ(bench.ux.depth(), 2);

  // The row the keys are on kept the highlight through the rebuild, and the set the list holds
  // carries the new unit.
  CHECK_EQ(scene->selected(), 0);
  CHECK_EQ(std::string(textOf(scene->values(), "unit").c_str()), std::string("inhg"));

  // A row prints its label and not the value it holds, so the edited cell inks what it inked before
  // the edit; the list is rebuilt all the same, and a list built from the set the scene holds draws
  // the same picture.
  gardener::gfx::test::reset();
  scene->drawPage(gardener::gfx::test::canvas);
  const int rebuilt =
      gardener::gfx::test::display.inkCount(0, 8, gardener::gfx::test::PageWidth, 8);
  CHECK(rebuilt > 0);
  CHECK_EQ(rebuilt, before);
  SettingsProbe fresh(bench.gestures, bench.ux, String("SYSTEM"), bench.system.rows(),
                      scene->values());
  gardener::gfx::test::reset();
  fresh.drawPage(gardener::gfx::test::canvas);
  CHECK_EQ(gardener::gfx::test::display.inkCount(0, 8, gardener::gfx::test::PageWidth, 8), rebuilt);

  // OK pops the list, which hands the set to the device, and the device writes it back.
  bench.gestures.push("ok:single");
  bench.ux.poll(gardener::gfx::test::canvas, 1020);
  CHECK_EQ(bench.ux.depth(), 1);
  CHECK_EQ(std::string(textOf(handed, "unit").c_str()), std::string("inhg"));
  CHECK(bench.system.apply(handed));
  CHECK_EQ(bench.library.writes(), 1);
  CHECK(bench.settings.unit() == gardener::DisplayUnit::InHg);
  CHECK(bench.library.textAt(SettingsPath).find("unit=inhg") != std::string::npos);
}
