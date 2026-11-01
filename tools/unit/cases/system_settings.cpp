// The device's own settings list as a model: the three rows it shows, the set it reads from the
// document, and the set it writes back. The store is the suite's in-memory Library, so a case owns
// the document and counts the writes.

#include <doctest.h>

#include <Arduino.h>

#include <any>
#include <string>

#include "core/SystemState.h"
#include "fakes/MemoryLibrary.h"
#include "fakes/Parts.h"
#include "machine/Machine.h"
#include "panel/GestureQueue.h"
#include "panel/page/SettingsRows.h"
#include "panel/page/SystemSettings.h"
#include "panel/page/Ux.h"
#include "panel/parts/BuzzerPlayer.h"
#include "storage/DeviceSettings.h"
#include "storage/JingleLibrary.h"

namespace {

using gardener::BuzzerPlayer;
using gardener::DeviceSettings;
using gardener::GestureQueue;
using gardener::JingleLibrary;
using gardener::SettingsRow;
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

}  // namespace

TEST_CASE("the system list shows the unit, the jingle and the maximum in that order") {
  Bench bench;
  const std::vector<SettingsRow*>& rows = bench.system.rows();
  REQUIRE_EQ(rows.size(), 3u);

  CHECK_EQ(std::string(rows[0]->key().c_str()), std::string("unit"));
  CHECK_EQ(std::string(rows[0]->label().c_str()), std::string("UNIT"));
  CHECK_EQ(std::string(rows[0]->text(std::any(String("psi"))).c_str()), std::string("psi"));

  CHECK_EQ(std::string(rows[1]->key().c_str()), std::string("jingle"));
  CHECK_EQ(std::string(rows[1]->label().c_str()), std::string("JINGLE"));
  CHECK_EQ(std::string(rows[1]->text(std::any(String("forelle"))).c_str()), std::string("Forelle"));

  CHECK_EQ(std::string(rows[2]->key().c_str()), std::string("max_duration"));
  CHECK_EQ(std::string(rows[2]->label().c_str()), std::string("MAX"));
  CHECK_EQ(std::string(rows[2]->text(std::any(int32_t(900))).c_str()), std::string("15m"));
}

TEST_CASE("the maximum row cannot offer more than the machine grants") {
  Bench bench;
  const gardener::SettingDeclaration declaration = bench.system.rows()[2]->declaration();
  CHECK_EQ(declaration.max, static_cast<float>(gardener::CONTROL_MAX_RUN_MS / 1000));
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
