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
#include "machine/Machine.h"
#include "panel/page/Registration.h"
#include "panel/page/RoutineRegistry.h"
#include "panel/settings/SettingsRows.h"
#include "storage/RoutineSettingStore.h"

namespace {

using gardener::BoolSettingCfg;
using gardener::ChoiceItem;
using gardener::ChoiceSettingCfg;
using gardener::DisplayUnit;
using gardener::DurationSettingCfg;
using gardener::GestureQueue;
using gardener::NumericSettingCfg;
using gardener::PressureSettingCfg;
using gardener::Registration;
using gardener::RoutineRegistry;
using gardener::RoutineSettingStore;
using gardener::Scene;
using gardener::SettingConfig;
using gardener::SettingValues;
using gardener::SystemState;
using gardener::Ux;
using gardener::test::MemoryLibrary;
using gardener::test::RecordingLogger;

// The queue no case presses: nothing here is drawn, so nothing reads it.
using NoPresses = GestureQueue;

// The page a launch builds: what a case reads a push through is the activation the factory was
// handed, so the page itself draws nothing.
class StubPage : public Scene {
 public:
  explicit StubPage(GestureQueue& gestures) : Scene(gestures) {}
};

constexpr const char* StoreDocument = "/cfg/routines/store_test.ini";
constexpr const char* StoreFixture = "initial_pressure=5.0\n"
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
constexpr const char* RampedFixture = "initial_pressure=9.0\n"
                                      "target_pressure=20.0\n"
                                      "total_duration=400\n"
                                      "\n"
                                      "[cold]\n"
                                      "routine_title=Cold\n"
                                      "target_pressure=15.0\n"
                                      "\n"
                                      "[warm]\n"
                                      "target_pressure=30.0\n";

// A document whose section hides one setting, pins another and names a glyph of its own.
constexpr const char* PinnedDocument = "/cfg/routines/pinned.ini";
constexpr const char* PinnedFixture = "initial_pressure=9.0\n"
                                      "target_pressure=20.0\n"
                                      "total_duration=400\n"
                                      "\n"
                                      "[cold]\n"
                                      "routine_title=Cold\n"
                                      "routine_icon=080800800000000000000000\n"
                                      "target_pressure:hidden=true\n"
                                      "initial_pressure:constant=7.0\n"
                                      "\n"
                                      "[warm]\n"
                                      "total_duration=600\n";

// The glyph a record registers with, which a section that names none keeps.
constexpr const char* RecordGlyph = "08010080000000";

// A document whose sections state the range their settings move within: one narrows, one widens, and
// one states a pair a duration cannot take.
constexpr const char* BoundedDocument = "/cfg/routines/bounded.ini";
constexpr const char* BoundedFixture = "initial_pressure=9.0\n"
                                       "target_pressure=20.0\n"
                                       "total_duration=400\n"
                                       "\n"
                                       "[narrow]\n"
                                       "initial_pressure=50.0\n"
                                       "initial_pressure:min=10\n"
                                       "initial_pressure:max=12\n"
                                       "total_duration=400\n"
                                       "total_duration:min=600\n"
                                       "total_duration:max=300\n"
                                       "\n"
                                       "[wide]\n"
                                       "initial_pressure=9.0\n"
                                       "initial_pressure:min=-10\n"
                                       "initial_pressure:max=200\n"
                                       "total_duration:min=120\n"
                                       "total_duration:max=240\n";

// A document whose pressure section reaches past what the machine allows.
constexpr const char* PressureDocument = "/cfg/routines/pressured.ini";
constexpr const char* PressureFixture = "\n"
                                        "[outside]\n"
                                        "target_pressure=20.0\n"
                                        "target_pressure:min=2\n"
                                        "target_pressure:max=90\n";

std::vector<ChoiceItem> resumeChoices() {
  return {{"hold", "Hold"}, {"immediately", "Immediately"}};
}

// A row is the document's half and the page's, and the store takes the first.
std::vector<const gardener::SettingKind*> kindsOf(const std::vector<SettingConfig*>& rows) {
  return std::vector<const gardener::SettingKind*>(rows.begin(), rows.end());
}

// The rows the store cases declare: one of every kind, a key written as nothing and a key the
// document lacks.
struct StoreRows {
  NoPresses gestures;
  Ux ux;
  NumericSettingCfg target{gestures, ux, "target_pressure", "Target", NAN, 1, 0, 100, true};
  DurationSettingCfg total{gestures, ux, "total_duration", "Total", 300, 5, 0, 3600, false};
  ChoiceSettingCfg resume{gestures,        ux,   "resume_behavior", "Resume", "hold",
                          resumeChoices(), false};
  BoolSettingCfg enabled{gestures, ux, "enabled", "Enabled", false};
  NumericSettingCfg empty{gestures, ux, "empty_key", "Empty", 3.0f, 1, 0, 10, false};
  NumericSettingCfg missing{gestures, ux, "missing_key", "Missing", 7.0f, 1, 0, 10, false};
  std::vector<SettingConfig*> held{&target, &total, &resume, &enabled, &empty, &missing};
};

// The declaration a registry record owns, taken fresh for each registration.
std::vector<std::unique_ptr<SettingConfig>> rampedDeclaration(NoPresses& gestures, Ux& ux) {
  std::vector<std::unique_ptr<SettingConfig>> out;
  out.push_back(std::make_unique<NumericSettingCfg>(gestures, ux, "initial_pressure", "Start", 5.0f,
                                                    1, 0, 100, false));
  out.push_back(std::make_unique<NumericSettingCfg>(gestures, ux, "target_pressure", "Target",
                                                    20.0f, 1, 0, 100, false));
  out.push_back(std::make_unique<DurationSettingCfg>(gestures, ux, "total_duration", "Total", 300,
                                                     1, 1, 7200, false));
  return out;
}

// The declaration a pressure record owns: the machine's own bounds until a section states others.
std::vector<std::unique_ptr<SettingConfig>> pressureDeclaration(NoPresses& gestures, Ux& ux,
                                                                SystemState& state) {
  std::vector<std::unique_ptr<SettingConfig>> out;
  out.push_back(std::make_unique<PressureSettingCfg>(gestures, ux, "target_pressure", "Target",
                                                     state, 20.0f, 1.0f));
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
  const SettingValues values = store.load("store_test", "store_test", kindsOf(rows.held));

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
  const SettingValues values = store.load("store_test", "store_test", kindsOf(rows.held));

  CHECK_EQ(kpaOf(values, "empty_key"), 3.0f);
  CHECK_EQ(kpaOf(values, "missing_key"), 7.0f);
}

TEST_CASE("an alias reads its own section") {
  MemoryLibrary library;
  library.put(StoreDocument, StoreFixture);
  StoreRows rows;
  RoutineSettingStore store(library);
  const SettingValues values = store.load("store_test", "cold", kindsOf(rows.held));

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

  SettingValues values = store.load("store_test", "store_test", kindsOf(rows.held));
  values["total_duration"] = static_cast<int32_t>(480);
  values["target_pressure"] = 42.0f;
  REQUIRE(store.save("store_test", "store_test", kindsOf(rows.held), values));

  CHECK_EQ(library.writes(), before + 1);
  const std::string written = library.textAt(StoreDocument);
  CHECK(written.find("total_duration=480") != std::string::npos);
  CHECK(written.find("routine_title=Cold") != std::string::npos);
  CHECK(written.find("[cold]") != std::string::npos);

  const SettingValues reread = store.load("store_test", "store_test", kindsOf(rows.held));
  CHECK_EQ(kpaOf(reread, "target_pressure"), 42.0f);
  const SettingValues alias = store.load("store_test", "cold", kindsOf(rows.held));
  CHECK_EQ(kpaOf(alias, "target_pressure"), 15.0f);
}

TEST_CASE("a set that changed nothing is not written") {
  MemoryLibrary library;
  library.put(StoreDocument, StoreFixture);
  StoreRows rows;
  RoutineSettingStore store(library);
  const SettingValues values = store.load("store_test", "store_test", kindsOf(rows.held));
  REQUIRE(store.save("store_test", "store_test", kindsOf(rows.held), values));
  const int settled = library.writes();
  REQUIRE(store.save("store_test", "store_test", kindsOf(rows.held), values));
  CHECK_EQ(library.writes(), settled);
}

TEST_CASE("a value written as nothing reads back as nothing") {
  MemoryLibrary library;
  library.put(StoreDocument, StoreFixture);
  StoreRows rows;
  RoutineSettingStore store(library);

  SettingValues values = store.load("store_test", "store_test", kindsOf(rows.held));
  values["target_pressure"] = std::any();
  REQUIRE(store.save("store_test", "store_test", kindsOf(rows.held), values));

  const SettingValues reread = store.load("store_test", "store_test", kindsOf(rows.held));
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

TEST_CASE("an alternate is a copy of its record") {
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

  // The copy answers to the section's key, reads the record's document, and takes the section's
  // title where it names one and its own key where it does not.
  const Registration* alternate = registry.registration("cold");
  REQUIRE(alternate != nullptr);
  CHECK_EQ(std::string(alternate->routineKey().c_str()), std::string("cold"));
  CHECK_EQ(std::string(alternate->documentKey().c_str()), std::string("ramped_static"));
  CHECK_EQ(std::string(alternate->title().c_str()), std::string("Cold"));
  CHECK_EQ(std::string(rows[2].routineKey.c_str()), std::string("warm"));
  CHECK_EQ(std::string(rows[2].title.c_str()), std::string("warm"));

  // The declaration is the record's, made again: the copy owns rows of its own.
  const Registration* record = registry.registration("ramped_static");
  REQUIRE(record != nullptr);
  REQUIRE(alternate->settings().size() == record->settings().size());
  for (size_t i = 0; i < record->settings().size(); i++) {
    CHECK(alternate->settings()[i] != record->settings()[i]);
    CHECK_EQ(std::string(alternate->settings()[i]->key().c_str()),
             std::string(record->settings()[i]->key().c_str()));
  }
}

TEST_CASE("an alternate reads and writes its own section") {
  MemoryLibrary library;
  library.put(RampedDocument, RampedFixture);
  RecordingLogger logger;
  RoutineRegistry registry(library, logger);
  NoPresses gestures;
  Ux ux;
  REQUIRE(registry.add(Registration("ramped_static", "Ramped", rampedDeclaration(gestures, ux))));

  // The section carries one key; the rest begin at the declaration's default, not at the record's
  // global set.
  SettingValues alternate = registry.loadSettings("cold");
  CHECK_EQ(kpaOf(alternate, "target_pressure"), 15.0f);
  CHECK_EQ(kpaOf(alternate, "initial_pressure"), 5.0f);
  CHECK_EQ(secondsOf(alternate, "total_duration"), 300);

  alternate["target_pressure"] = 12.0f;
  REQUIRE(registry.saveSettings("cold", alternate));

  const std::string written = library.textAt(RampedDocument);
  CHECK(written.find("[cold]") != std::string::npos);
  CHECK(written.find("target_pressure=12.00") != std::string::npos);
  CHECK_EQ(kpaOf(registry.loadSettings("cold"), "target_pressure"), 12.0f);

  // The record's own set is the document's global section, which the copy's write left alone.
  CHECK_EQ(kpaOf(registry.loadSettings("ramped_static"), "target_pressure"), 20.0f);
}

TEST_CASE("a section hides and pins settings and names its own glyph") {
  MemoryLibrary library;
  library.put(PinnedDocument, PinnedFixture);
  RecordingLogger logger;
  RoutineRegistry registry(library, logger);
  NoPresses gestures;
  Ux ux;
  REQUIRE(registry.add(
      Registration("pinned", "Pinned", rampedDeclaration(gestures, ux), nullptr, RecordGlyph)));

  // Both marked rows keep their place in the declaration — only the list leaves them out — and the
  // record's own rows are untouched.
  const std::vector<SettingConfig*>& cold = *registry.settingsRows("cold");
  REQUIRE(cold.size() == 3);
  CHECK(cold[0]->hidden());
  CHECK(cold[1]->hidden());
  CHECK_FALSE(cold[2]->hidden());
  const std::vector<SettingConfig*>& own = *registry.settingsRows("pinned");
  REQUIRE(own.size() == 3);
  CHECK_FALSE(own[0]->hidden());
  CHECK_FALSE(own[1]->hidden());

  // The pinned setting reads as the value the section fixes; the hidden one still reads its own.
  CHECK_EQ(kpaOf(registry.loadSettings("cold"), "initial_pressure"), 7.0f);
  CHECK_EQ(kpaOf(registry.loadSettings("cold"), "target_pressure"), 20.0f);

  // The section's glyph is the alternate's, and one that names none keeps the record's.
  REQUIRE(registry.icon("cold") != nullptr);
  CHECK_EQ(std::string(registry.icon("cold")->c_str()), std::string("080800800000000000000000"));
  REQUIRE(registry.icon("warm") != nullptr);
  CHECK_EQ(std::string(registry.icon("warm")->c_str()), std::string(RecordGlyph));
  const std::vector<gardener::RoutineRow> rows = registry.rows();
  REQUIRE(rows.size() >= 2);
  CHECK_EQ(std::string(rows[1].glyph.c_str()), std::string("080800800000000000000000"));
}

TEST_CASE("a launch cannot move a setting a section pinned") {
  MemoryLibrary library;
  library.put(PinnedDocument, PinnedFixture);
  RecordingLogger logger;
  RoutineRegistry registry(library, logger);
  NoPresses gestures;
  Ux ux;

  gardener::RoutineActivation launched;
  REQUIRE(registry.add(
      Registration("pinned", "Pinned", rampedDeclaration(gestures, ux),
                   [&launched, &gestures](const gardener::RoutineActivation& activation) {
                     launched = activation;
                     return std::make_unique<StubPage>(gestures);
                   })));

  // The caller states another value for the pinned setting, which the section's value wins over; the
  // hidden setting is the caller's to state, since hiding only keeps it off the list.
  SettingValues values;
  values["initial_pressure"] = 30.0f;
  values["target_pressure"] = 15.0f;
  REQUIRE(registry.activate("cold", values) != nullptr);
  CHECK_EQ(std::string(launched.routineKey.c_str()), std::string("cold"));
  CHECK_EQ(kpaOf(launched.values, "initial_pressure"), 7.0f);
  CHECK_EQ(kpaOf(launched.values, "target_pressure"), 15.0f);

  // A settled set cannot move it either: the save writes the pinned value.
  SettingValues settled = registry.loadSettings("cold");
  settled["initial_pressure"] = 30.0f;
  REQUIRE(registry.saveSettings("cold", settled));
  CHECK(library.textAt(PinnedDocument).find("initial_pressure=7.00") != std::string::npos);
  CHECK_EQ(kpaOf(registry.loadSettings("cold"), "initial_pressure"), 7.0f);
}

TEST_CASE("a section states the bounds a setting moves within") {
  MemoryLibrary library;
  library.put(BoundedDocument, BoundedFixture);
  RecordingLogger logger;
  RoutineRegistry registry(library, logger);
  NoPresses gestures;
  Ux ux;
  REQUIRE(registry.add(Registration("bounded", "Bounded", rampedDeclaration(gestures, ux))));

  // The pair a section states is the declaration its alternate reports, and the value the section
  // holds is read back through that pair.
  const std::vector<SettingConfig*>& narrow = *registry.settingsRows("narrow");
  REQUIRE(narrow.size() == 3);
  const gardener::SettingDeclaration start = narrow[0]->declaration();
  CHECK_EQ(start.min, 10.0f);
  CHECK_EQ(start.max, 12.0f);
  CHECK_EQ(kpaOf(registry.loadSettings("narrow"), "initial_pressure"), 12.0f);

  // A pair that leaves no range is a typo: the duration keeps the pair it was declared with, and the
  // value the section holds reads as it is.
  const gardener::SettingDeclaration total = narrow[2]->declaration();
  CHECK_EQ(total.min, 1.0f);
  CHECK_EQ(total.max, 7200.0f);
  CHECK_EQ(secondsOf(registry.loadSettings("narrow"), "total_duration"), 400);

  // A section widens as readily as it narrows, and the record's own declaration never moves.
  const gardener::SettingDeclaration opened = registry.settingsRows("wide")->at(0)->declaration();
  CHECK_EQ(opened.min, -10.0f);
  CHECK_EQ(opened.max, 200.0f);
  // A duration is a number row, so its pair is the section's too.
  const gardener::SettingDeclaration held = registry.settingsRows("wide")->at(2)->declaration();
  CHECK_EQ(held.min, 120.0f);
  CHECK_EQ(held.max, 240.0f);
  const gardener::SettingDeclaration declared =
      registry.settingsRows("bounded")->at(0)->declaration();
  CHECK_EQ(declared.min, 0.0f);
  CHECK_EQ(declared.max, 100.0f);
}

TEST_CASE("a section's pressure pair stays inside the machine's") {
  MemoryLibrary library;
  library.put(PressureDocument, PressureFixture);
  RecordingLogger logger;
  RoutineRegistry registry(library, logger);
  NoPresses gestures;
  Ux ux;
  SystemState state;
  REQUIRE(registry.add(
      Registration("pressured", "Pressured", pressureDeclaration(gestures, ux, state))));

  // 2 and 90 lie outside what the loop accepts, so the row keeps the machine's own pair.
  const gardener::SettingDeclaration target =
      registry.settingsRows("outside")->at(0)->declaration();
  CHECK_EQ(target.min, gardener::CONTROL_MIN_TARGET_KPA);
  CHECK_EQ(target.max, CONTROL_FLOOR_KPA);
  CHECK_EQ(kpaOf(registry.loadSettings("outside"), "target_pressure"), 20.0f);
}

TEST_CASE("a record registered again re-makes its alternates once, in their place") {
  MemoryLibrary library;
  library.put(RampedDocument, RampedFixture);
  RecordingLogger logger;
  RoutineRegistry registry(library, logger);
  NoPresses gestures;
  Ux ux;

  REQUIRE(registry.add(Registration("ramped_static", "Ramped", rampedDeclaration(gestures, ux))));
  REQUIRE(registry.add(Registration("other", "Other", rampedDeclaration(gestures, ux))));
  REQUIRE(
      registry.add(Registration("ramped_static", "Ramped again", rampedDeclaration(gestures, ux))));

  const std::vector<String> keys = registry.keys();
  REQUIRE(keys.size() == 4);
  CHECK_EQ(std::string(keys[0].c_str()), std::string("ramped_static"));
  CHECK_EQ(std::string(keys[1].c_str()), std::string("cold"));
  CHECK_EQ(std::string(keys[2].c_str()), std::string("warm"));
  CHECK_EQ(std::string(keys[3].c_str()), std::string("other"));
  CHECK_EQ(std::string(registry.title("ramped_static").c_str()), std::string("Ramped again"));
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

TEST_CASE("a launch carries the set the caller stated, held to the record's declaration") {
  MemoryLibrary library;
  library.put(RampedDocument, RampedFixture);
  RecordingLogger logger;
  RoutineRegistry registry(library, logger);
  NoPresses gestures;
  Ux ux;

  gardener::RoutineActivation launched;
  REQUIRE(registry.add(
      Registration("ramped_static", "Ramped", rampedDeclaration(gestures, ux),
                   [&launched, &gestures](const gardener::RoutineActivation& activation) {
                     launched = activation;
                     return std::make_unique<StubPage>(gestures);
                   })));

  // The caller states the set it launches with; a whole number written for a pressure is the
  // pressure, and a setting the set leaves out begins at the declaration's default.
  SettingValues values;
  values["initial_pressure"] = int32_t(12);
  values["undeclared"] = String("x");
  REQUIRE(registry.activate("ramped_static", values) != nullptr);

  CHECK_EQ(std::string(launched.routineKey.c_str()), std::string("ramped_static"));
  CHECK_EQ(kpaOf(launched.values, "initial_pressure"), 12.0f);
  CHECK_EQ(kpaOf(launched.values, "target_pressure"), 20.0f);
  CHECK_EQ(secondsOf(launched.values, "total_duration"), 300);
  CHECK(launched.values.find("undeclared") == launched.values.end());
}

TEST_CASE("a key nobody registered launches nothing") {
  MemoryLibrary library;
  RecordingLogger logger;
  RoutineRegistry registry(library, logger);

  CHECK(registry.activate("nobody", SettingValues()) == nullptr);
}

TEST_CASE("a row holds a value of its own kind and refuses another") {
  NoPresses gestures;
  Ux ux;
  NumericSettingCfg pressure{gestures, ux, "target_pressure", "Target", 5.0f, 1, 0, 100, true};
  DurationSettingCfg total{gestures, ux, "total_duration", "Total", 300, 1, 0, 7200, false};
  ChoiceSettingCfg resume{gestures,        ux,   "resume_behavior", "Resume", "hold",
                          resumeChoices(), false};
  BoolSettingCfg enabled{gestures, ux, "enabled", "Enabled", false};

  std::any nothing;
  CHECK(pressure.accept(nothing));
  CHECK(total.accept(nothing));

  // A whole literal a page wrote for a pressure reads as the pressure.
  std::any whole = int32_t(15);
  REQUIRE(pressure.accept(whole));
  CHECK_EQ(std::any_cast<float>(whole), 15.0f);

  // A real written for a duration is whole seconds, which is the unit the kind holds.
  std::any real = 4.75f;
  REQUIRE(total.accept(real));
  CHECK_EQ(std::any_cast<int32_t>(real), 4);

  std::any key = String("hold");
  CHECK(resume.accept(key));
  std::any flag = true;
  CHECK(enabled.accept(flag));

  // A value of another kind is refused rather than cast to.
  CHECK_FALSE(pressure.accept(key));
  CHECK_FALSE(total.accept(flag));
  CHECK_FALSE(resume.accept(whole));
}
