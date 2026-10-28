// RoutineSettingStore's cases: a routine's document read into the values a list opens on, and the
// settled set written back to it. They run over the in-memory image the check loads, with a fixture
// document written into it, so what a save leaves in the file can be read back.

#include "RoutineSettingStoreCheck.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <any>
#include <vector>

#include <Gfx.h>

#include "MemoryFiles.h"
#include "storage/RoutineSettingStore.h"

namespace gardener {
namespace {

using gfx::ui::BoolRow;
using gfx::ui::ChoiceItem;
using gfx::ui::ChoiceRow;
using gfx::ui::DurationRow;
using gfx::ui::GestureQueue;
using gfx::ui::NumberRow;
using gfx::ui::SettingValues;
using gfx::ui::SettingsRow;

// A fixture routine's document: a value of every kind the list has a row for, a key written as
// nothing, an alias section with a title, and the parent set an alias must not disturb.
constexpr const char* Document = "/cfg/routines/store_test.ini";
constexpr const char* Fixture =
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

// The queue a store case never presses: the rows build no scene here, so nothing reads it.
class NoPresses : public GestureQueue {
 public:
  const char* take() override { return nullptr; }
};

// The entries the fixture's resume_behavior key picks between.
std::vector<ChoiceItem> resumeChoices() {
  return {{"hold", "Hold", nullptr}, {"immediately", "Immediately", nullptr}};
}

bool expect(bool condition, const char* what) {
  if (condition) return true;
  printf("[E] store: %s\n", what);
  return false;
}

// The rows a case declares, and the stack parts they hold. Nothing here is drawn, so the rows exist
// only to state their kinds.
struct Rows {
  NoPresses gestures;
  gfx::ui::Ux ux;
  NumberRow target{gestures, ux, "target_pressure", "Target", NAN, 1, 0, 100, true};
  DurationRow total{gestures, ux, "total_duration", "Total", 300, 5, 0, 3600, false};
  ChoiceRow resume{gestures, ux, "resume_behavior", "Resume", "hold", resumeChoices(), false};
  BoolRow enabled{gestures, ux, "enabled", "Enabled", false};
  NumberRow empty{gestures, ux, "empty_key", "Empty", 3.0f, 1, 0, 10, false};
  NumberRow missing{gestures, ux, "missing_key", "Missing", 7.0f, 1, 0, 10, false};
  std::vector<SettingsRow*> held{&target, &total, &resume, &enabled, &empty, &missing};
};

bool seed(MemoryFiles& files, const char* dataDir) {
  if (!files.load(dataDir)) {
    printf("[E] store: the image is not there\n");
    return false;
  }
  return files.write(Document, Fixture);
}

bool aRoutineSetReadsItsDocument(const char* dataDir) {
  MemoryFiles files;
  if (!seed(files, dataDir)) return false;
  Rows rows;
  RoutineSettingStore store(files);
  const SettingValues values = store.load("store_test", "store_test", rows.held);

  bool passed = expect(std::any_cast<float>(values.at("target_pressure")) == 20.0f,
                       "the document's number is not read");
  passed = expect(std::any_cast<int32_t>(values.at("total_duration")) == 450,
                  "the document's seconds are not read") &&
           passed;
  passed = expect(std::any_cast<String>(values.at("resume_behavior")) == String("immediately"),
                  "the document's choice key is not read") &&
           passed;
  passed = expect(std::any_cast<bool>(values.at("enabled")), "the document's switch is not read") &&
           passed;
  passed = expect(std::any_cast<float>(values.at("empty_key")) == 3.0f,
                  "a key written as nothing does not answer the default") &&
           passed;
  passed = expect(std::any_cast<float>(values.at("missing_key")) == 7.0f,
                  "a key the document lacks does not answer the default") &&
           passed;
  return passed;
}

bool anAliasReadsItsOwnSection(const char* dataDir) {
  MemoryFiles files;
  if (!seed(files, dataDir)) return false;
  Rows rows;
  RoutineSettingStore store(files);
  const SettingValues values = store.load("store_test", "cold", rows.held);

  bool passed = expect(std::any_cast<float>(values.at("target_pressure")) == 15.0f,
                       "an alias does not read its section");
  passed = expect(std::any_cast<int32_t>(values.at("total_duration")) == 300,
                  "an alias key its section lacks does not answer the default") &&
           passed;
  passed = expect(!std::any_cast<bool>(values.at("enabled")),
                  "an alias inherited the parent's set") &&
           passed;
  return passed;
}

bool aSaveWritesOnceAndLeavesTheRest(const char* dataDir) {
  MemoryFiles files;
  if (!seed(files, dataDir)) return false;
  Rows rows;
  RoutineSettingStore store(files);
  const int before = files.writes();

  SettingValues values = store.load("store_test", "store_test", rows.held);
  values["total_duration"] = static_cast<int32_t>(480);
  values["target_pressure"] = 42.0f;
  if (!expect(store.save("store_test", "store_test", rows.held, values), "the save was refused")) {
    return false;
  }

  bool passed = expect(files.writes() == before + 1, "a settled set did not cost one write");

  String written;
  files.read(Document, written);
  passed = expect(written.indexOf('=') > 0, "the document was not readable after the save") && passed;
  passed = expect(strstr(written.c_str(), "total_duration=480") != nullptr,
                  "the changed seconds are not in the document") &&
           passed;
  passed = expect(strstr(written.c_str(), "routine_title=Cold") != nullptr,
                  "a key the rows do not declare was lost") &&
           passed;
  passed = expect(strstr(written.c_str(), "[cold]") != nullptr, "the alias section was lost") &&
           passed;

  const SettingValues reread = store.load("store_test", "store_test", rows.held);
  passed = expect(std::any_cast<float>(reread.at("target_pressure")) == 42.0f,
                  "the settled value does not read back") &&
           passed;
  const SettingValues alias = store.load("store_test", "cold", rows.held);
  passed = expect(std::any_cast<float>(alias.at("target_pressure")) == 15.0f,
                  "a save of the parent set disturbed its alias") &&
           passed;

  // A set that changes nothing costs no write at all.
  const int settled = files.writes();
  store.save("store_test", "store_test", rows.held, reread);
  passed = expect(files.writes() == settled, "a set that changed nothing was written") && passed;
  return passed;
}

bool nothingReadsBackAsTheDefault(const char* dataDir) {
  MemoryFiles files;
  if (!seed(files, dataDir)) return false;
  Rows rows;
  RoutineSettingStore store(files);

  SettingValues values = store.load("store_test", "store_test", rows.held);
  values["target_pressure"] = std::any();
  if (!expect(store.save("store_test", "store_test", rows.held, values),
              "the save of a set holding nothing was refused")) {
    return false;
  }

  const SettingValues reread = store.load("store_test", "store_test", rows.held);
  return expect(!reread.at("target_pressure").has_value(),
                "a value written as nothing does not read back as nothing");
}

using Case = bool (*)(const char* dataDir);

void run(const char* name, Case item, const char* dataDir, bool& passed) {
  if (!item(dataDir)) {
    passed = false;
    return;
  }
  printf("[x] %s\n", name);
}

}  // namespace

bool runRoutineSettingStoreCases(const char* dataDir) {
  bool passed = true;
  run("a routine set reads its document", aRoutineSetReadsItsDocument, dataDir, passed);
  run("an alias reads its own section", anAliasReadsItsOwnSection, dataDir, passed);
  run("a save writes once and leaves the rest", aSaveWritesOnceAndLeavesTheRest, dataDir, passed);
  run("nothing reads back as the default", nothingReadsBackAsTheDefault, dataDir, passed);
  return passed;
}

}  // namespace gardener
