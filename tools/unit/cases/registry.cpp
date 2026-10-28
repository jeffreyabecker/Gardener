// The routines a panel offers and the documents behind them: the registry's records, their
// alternates and the order they keep, then the settings store reading a routine's document into the
// values a list opens on and writing a settled set back. The image is the suite's in-memory Library,
// so a case owns the documents and counts the writes.

#include <doctest.h>

#include <Arduino.h>

#include <any>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <Gfx.h>

#include "fakes/MemoryLibrary.h"
#include "fakes/Parts.h"
#include "routines/Registration.h"
#include "routines/RoutineRegistry.h"
#include "storage/RoutineSettingStore.h"
#include "ui/SettingsRows.h"

namespace {

using gardener::Registration;
using gardener::RoutineRegistry;
using gardener::RoutineSettingStore;
using gardener::test::MemoryLibrary;
using gardener::test::RecordingLogger;
using gardener::gfx::ui::BoolRow;
using gardener::gfx::ui::ChoiceItem;
using gardener::gfx::ui::ChoiceRow;
using gardener::gfx::ui::DurationRow;
using gardener::gfx::ui::GestureQueue;
using gardener::gfx::ui::NumberRow;
using gardener::gfx::ui::SettingValues;
using gardener::gfx::ui::SettingsRow;
using gardener::gfx::ui::Ux;

// The queue no case presses: nothing here is drawn, so nothing reads it.
class NoPresses : public GestureQueue {
 public:
  const char* take() override { return nullptr; }
};

constexpr const char* StoreDocument = "/cfg/routines/store_test.ini";
constexpr const char* StoreFixture =
    "initial_pressure=5.0\n"
    "target_pressure=20.0\n"
    "total_duration=450\n"
    "resume_behavior=immediately\n"
    "enabled=on\n"
    "empty_key=\n"
    "\n"
    "[cold]\n"
    "routine_title=Cold\n"
    "target_pressure=15.0\n";

constexpr const char* RampedDocument = "/cfg/routines/ramped_static.ini";
constexpr const char* RampedFixture =
    "initial_pressure=9.0\n"
    "target_pressure=20.0\n"
    "total_duration=400\n"
    "\n"
    "[cold]\n"
    "routine_title=Cold\n"
    "target_pressure=15.0\n"
    "\n"
    "[warm]\n"
    "target_pressure=30.0\n";

std::vector<ChoiceItem> resumeChoices() {
  return {{"hold", "Hold"}, {"immediately", "Immediately"}};
}

// The rows the store cases declare: one of every kind, a key written as nothing and a key the
// document lacks.
struct StoreRows {
  NoPresses gestures;
  Ux ux;
  NumberRow target{gestures, ux, "target_pressure", "Target", NAN, 1, 0, 100, true};
  DurationRow total{gestures, ux, "total_duration", "Total", 300, 5, 0, 3600, false};
  ChoiceRow resume{gestures, ux, "resume_behavior", "Resume", "hold", resumeChoices(), false};
  BoolRow enabled{gestures, ux, "enabled", "Enabled", false};
  NumberRow empty{gestures, ux, "empty_key", "Empty", 3.0f, 1, 0, 10, false};
  NumberRow missing{gestures, ux, "missing_key", "Missing", 7.0f, 1, 0, 10, false};
  std::vector<SettingsRow*> held{&target, &total, &resume, &enabled, &empty, &missing};
};

// The declaration a registry record owns, taken fresh for each registration.
std::vector<std::unique_ptr<SettingsRow>> rampedDeclaration(NoPresses& gestures, Ux& ux) {
  std::vector<std::unique_ptr<SettingsRow>> out;
  out.push_back(std::make_unique<NumberRow>(gestures, ux, "initial_pressure", "Start", 5.0f, 1, 0,
                                            100, false));
  out.push_back(std::make_unique<NumberRow>(gestures, ux, "target_pressure", "Target", 20.0f, 1, 0,
                                            100, false));
  out.push_back(std::make_unique<DurationRow>(gestures, ux, "total_duration", "Total", 300, 1, 1,
                                              7200, false));
  return out;
}

int32_t secondsOf(const SettingValues& values, const char* key) {
  const auto held = values.find(key);
  if (held == values.end() || !held->second.has_value()) return 0;
  return std::any_cast<int32_t>(held->second);
}

float kpaOf(const SettingValues& values, const char* key) {
  const auto held = values.find(key);
  if (held == values.end() || !held->second.has_value()) return 0.0f;
  return std::any_cast<float>(held->second);
}

}  // namespace

TEST_CASE("a routine set reads its document") {
  MemoryLibrary library;
  library.put(StoreDocument, StoreFixture);
  StoreRows rows;
  RoutineSettingStore store(library);
  const SettingValues values = store.load("store_test", "store_test", rows.held);

  CHECK_EQ(kpaOf(values, "target_pressure"), 20.0f);
  CHECK_EQ(secondsOf(values, "total_duration"), 450);
  CHECK_EQ(std::string(std::any_cast<String>(values.at("resume_behavior")).c_str()),
           std::string("immediately"));
  CHECK(std::any_cast<bool>(values.at("enabled")));
}

TEST_CASE("a key written as nothing and a key the document lacks answer the default") {
  MemoryLibrary library;
  library.put(StoreDocument, StoreFixture);
  StoreRows rows;
  RoutineSettingStore store(library);
  const SettingValues values = store.load("store_test", "store_test", rows.held);

  CHECK_EQ(kpaOf(values, "empty_key"), 3.0f);
  CHECK_EQ(kpaOf(values, "missing_key"), 7.0f);
}

TEST_CASE("an alias reads its own section") {
  MemoryLibrary library;
  library.put(StoreDocument, StoreFixture);
  StoreRows rows;
  RoutineSettingStore store(library);
  const SettingValues values = store.load("store_test", "cold", rows.held);

  CHECK_EQ(kpaOf(values, "target_pressure"), 15.0f);
  CHECK_EQ(secondsOf(values, "total_duration"), 300);
  CHECK_FALSE(std::any_cast<bool>(values.at("enabled")));
}

TEST_CASE("a save writes once and leaves what it does not declare") {
  MemoryLibrary library;
  library.put(StoreDocument, StoreFixture);
  StoreRows rows;
  RoutineSettingStore store(library);
  const int before = library.writes();

  SettingValues values = store.load("store_test", "store_test", rows.held);
  values["total_duration"] = static_cast<int32_t>(480);
  values["target_pressure"] = 42.0f;
  REQUIRE(store.save("store_test", "store_test", rows.held, values));

  CHECK_EQ(library.writes(), before + 1);
  const std::string written = library.textAt(StoreDocument);
  CHECK(written.find("total_duration=480") != std::string::npos);
  CHECK(written.find("routine_title=Cold") != std::string::npos);
  CHECK(written.find("[cold]") != std::string::npos);

  const SettingValues reread = store.load("store_test", "store_test", rows.held);
  CHECK_EQ(kpaOf(reread, "target_pressure"), 42.0f);
  const SettingValues alias = store.load("store_test", "cold", rows.held);
  CHECK_EQ(kpaOf(alias, "target_pressure"), 15.0f);
}

TEST_CASE("a set that changed nothing is not written") {
  MemoryLibrary library;
  library.put(StoreDocument, StoreFixture);
  StoreRows rows;
  RoutineSettingStore store(library);
  const SettingValues values = store.load("store_test", "store_test", rows.held);
  REQUIRE(store.save("store_test", "store_test", rows.held, values));
  const int settled = library.writes();
  REQUIRE(store.save("store_test", "store_test", rows.held, values));
  CHECK_EQ(library.writes(), settled);
}

TEST_CASE("a value written as nothing reads back as nothing") {
  MemoryLibrary library;
  library.put(StoreDocument, StoreFixture);
  StoreRows rows;
  RoutineSettingStore store(library);

  SettingValues values = store.load("store_test", "store_test", rows.held);
  values["target_pressure"] = std::any();
  REQUIRE(store.save("store_test", "store_test", rows.held, values));

  const SettingValues reread = store.load("store_test", "store_test", rows.held);
  CHECK_FALSE(reread.at("target_pressure").has_value());
}

TEST_CASE("the registry keeps the order its records registered") {
  MemoryLibrary library;
  library.put(RampedDocument, RampedFixture);
  RecordingLogger logger;
  RoutineRegistry registry(library, logger);
  NoPresses gestures;
  Ux ux;

  REQUIRE(registry.add(Registration("ramped_static", "Ramped", rampedDeclaration(gestures, ux))));
  REQUIRE(registry.add(Registration("other", "Other", rampedDeclaration(gestures, ux))));

  const std::vector<String> keys = registry.keys();
  REQUIRE(keys.size() >= 2);
  CHECK_EQ(std::string(keys[0].c_str()), std::string("ramped_static"));
}

TEST_CASE("an alternate follows its record and resolves to it") {
  MemoryLibrary library;
  library.put(RampedDocument, RampedFixture);
  RecordingLogger logger;
  RoutineRegistry registry(library, logger);
  NoPresses gestures;
  Ux ux;
  REQUIRE(registry.add(Registration("ramped_static", "Ramped", rampedDeclaration(gestures, ux))));

  const std::vector<gardener::RoutineRow> rows = registry.rows();
  REQUIRE(rows.size() >= 3);
  CHECK_EQ(std::string(rows[0].routineKey.c_str()), std::string("ramped_static"));
  CHECK_EQ(std::string(rows[1].routineKey.c_str()), std::string("cold"));

  const Registration* alternate = registry.entry("cold");
  REQUIRE(alternate != nullptr);
  CHECK_EQ(std::string(alternate->routineKey().c_str()), std::string("ramped_static"));
}

TEST_CASE("a key that is not a plain word is refused and changes nothing") {
  MemoryLibrary library;
  RecordingLogger logger;
  RoutineRegistry registry(library, logger);
  NoPresses gestures;
  Ux ux;

  CHECK_FALSE(registry.add(Registration("bad/key", "Bad", rampedDeclaration(gestures, ux))));
  CHECK(registry.keys().empty());
}

TEST_CASE("a key registered twice keeps its place") {
  MemoryLibrary library;
  RecordingLogger logger;
  RoutineRegistry registry(library, logger);
  NoPresses gestures;
  Ux ux;

  REQUIRE(registry.add(Registration("first", "First", rampedDeclaration(gestures, ux))));
  REQUIRE(registry.add(Registration("second", "Second", rampedDeclaration(gestures, ux))));
  REQUIRE(registry.add(Registration("first", "Replaced", rampedDeclaration(gestures, ux))));

  const std::vector<String> keys = registry.keys();
  REQUIRE(keys.size() == 2);
  CHECK_EQ(std::string(keys[0].c_str()), std::string("first"));
  const Registration* first = registry.registration("first");
  REQUIRE(first != nullptr);
  CHECK_EQ(std::string(first->title().c_str()), std::string("Replaced"));
}
