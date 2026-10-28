// RoutineRegistry's cases: the records a routine's declaration registers, the alternates its
// document names, the rule that repairs a set, the document it is read from and written to, and the
// page a launch builds. They run over the in-memory image the check loads, with fixture documents
// written into it, and reach no interpreter.

#include "RoutineRegistryCheck.h"

#include <stdio.h>
#include <string.h>

#include <any>
#include <memory>
#include <vector>

#include <Gfx.h>

#include "Logger.h"
#include "MemoryFiles.h"
#include "routines/Registration.h"
#include "routines/RoutineRegistry.h"

namespace gardener {
namespace {

using gfx::ui::DurationRow;
using gfx::ui::GestureQueue;
using gfx::ui::NumberRow;
using gfx::ui::Scene;
using gfx::ui::SettingValues;
using gfx::ui::SettingsRow;
using gfx::ui::Ux;

// A document for `ramped_static`: a root set whose start is 9 kPa, so a value the record's default
// gives and a value the root set gives are told apart, and two alternate sections, one titled.
constexpr const char* Ramped = "/cfg/routines/ramped_static.ini";
constexpr const char* RampedText =
    "initial_pressure=9.0\n"
    "target_pressure=20.0\n"
    "initial_duration=60\n"
    "target_duration=60\n"
    "total_duration=400\n"
    "\n"
    "[cold]\n"
    "routine_title=Cold\n"
    "target_pressure=15.0\n"
    "\n"
    "[warm]\n"
    "target_pressure=30.0\n";

// The queue no case presses: nothing here is drawn, so nothing reads it.
class NoPresses : public GestureQueue {
 public:
  const char* take() override { return nullptr; }
};

// The lines a record and the registry write, kept so a case can read them back.
class RecordingLog : public Logger {
 public:
  String levels() const override { return "IWE"; }
  void setLevels(const String&) override {}
  void info(const char* text) override { lines_.push_back(text); }
  void warn(const char* text) override { lines_.push_back(text); }
  void error(const char* text) override { lines_.push_back(text); }

  bool holds(const char* text) const {
    for (const String& line : lines_) {
      if (strstr(line.c_str(), text) != nullptr) return true;
    }
    return false;
  }
  bool endsWith(const char* text) const {
    return !lines_.empty() && lines_.back().endsWith(text);
  }
  size_t count() const { return lines_.size(); }
  void clear() { lines_.clear(); }

 private:
  std::vector<String> lines_;
};

bool expect(bool condition, const char* what) {
  if (condition) return true;
  printf("[E] registry: %s\n", what);
  return false;
}

// The rows a record declares, taken afresh for each registration that owns them. The kinds are the
// settings list's, which is all a declaration is.
struct Rows {
  NoPresses gestures;
  Ux ux;

  std::vector<std::unique_ptr<SettingsRow>> declaration() {
    std::vector<std::unique_ptr<SettingsRow>> out;
    out.push_back(std::make_unique<NumberRow>(gestures, ux, "initial_pressure", "Start", 5.0f, 1, 0,
                                              100, false));
    out.push_back(std::make_unique<NumberRow>(gestures, ux, "target_pressure", "Target", 20.0f, 1,
                                              0, 100, false));
    out.push_back(std::make_unique<DurationRow>(gestures, ux, "initial_duration", "Start time", 60,
                                                1, 0, 7200, false));
    out.push_back(std::make_unique<DurationRow>(gestures, ux, "target_duration", "Target time", 60,
                                                1, 0, 7200, false));
    out.push_back(std::make_unique<DurationRow>(gestures, ux, "total_duration", "Total time", 300,
                                                1, 1, 7200, false));
    return out;
  }
};

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

String titleOf(const SettingValues& values) {
  const auto held = values.find("routine_title");
  return held == values.end() ? String() : std::any_cast<String>(held->second);
}

// `ramped_static`'s rule: the profile is the two holds plus a ramp, so the total covers the holds.
const char* totalCoversHolds(const SettingValues& values) {
  const int32_t holds = secondsOf(values, "initial_duration") + secondsOf(values, "target_duration");
  return secondsOf(values, "total_duration") < holds ? "TOTAL < HOLDS" : nullptr;
}

// A rule no set satisfies, so a repair runs out of settings to put back.
const char* nothingHolds(const SettingValues&) { return "NEVER FINE"; }

bool image(MemoryFiles& files, const char* dataDir) {
  if (files.load(dataDir)) return true;
  printf("[E] registry: the image is not there\n");
  return false;
}

bool rampedImage(MemoryFiles& files, const char* dataDir) {
  return image(files, dataDir) && files.write(Ramped, RampedText);
}

bool aDeclarationKeepsItsOrder(const char*) {
  Rows rows;
  const Registration::Glyph glyph = [](gfx::Rect) -> gfx::ui::Paint {
    return [](gfx::Canvas&) {};
  };
  Registration registration("key", "", rows.declaration(), nullptr, nullptr, glyph);

  bool passed = expect(registration.settings().size() == 5, "the declaration lost a setting");
  passed = expect(registration.settings().front()->key() == "initial_pressure",
                  "the declaration is not in the order it was written") &&
           passed;
  passed = expect(registration.title() == "key", "a record with no title is not known by its key") &&
           passed;
  passed = expect(!registration.factory(), "a record that names no page was given one") && passed;
  passed = expect(static_cast<bool>(registration.glyph()), "the record lost its glyph") && passed;
  return passed;
}

bool aRefusedSetIsRepaired(const char* dataDir) {
  MemoryFiles files;
  if (!rampedImage(files, dataDir)) return false;
  if (!files.write(Ramped,
                   "initial_pressure=9.0\ntarget_pressure=20.0\ninitial_duration=60\n"
                   "target_duration=60\ntotal_duration=30\n"
                   "\n[cold]\nroutine_title=Cold\ntotal_duration=30\n")) {
    return false;
  }

  Rows rows;
  RecordingLog log;
  RoutineRegistry registry(files, log);
  if (!expect(registry.add(Registration("ramped_static", "Ramped Static", rows.declaration(),
                                        totalCoversHolds)),
              "the record was refused")) {
    return false;
  }

  const SettingValues refused = registry.loadSettings("ramped_static");
  bool passed = expect(secondsOf(refused, "total_duration") == 300,
                       "a refused set was not put back to its default");
  passed = expect(log.holds("settings: ramped_static: TOTAL < HOLDS"),
                  "the repair did not log its reason") &&
           passed;

  // A repair in an alias's section is logged under the section, so the line names the set it is
  // about rather than the record it belongs to.
  log.clear();
  registry.loadSettings("cold");
  passed = expect(log.holds("settings: cold: TOTAL < HOLDS"),
                  "a repair in an alias's section is not logged under the section") &&
           passed;

  // The same document with a total that covers the holds is left alone, and says so by silence.
  if (!files.write(Ramped, RampedText)) return false;
  log.clear();
  const SettingValues accepted = registry.loadSettings("ramped_static");
  passed = expect(secondsOf(accepted, "total_duration") == 400,
                  "a set a run accepts was repaired") &&
           passed;
  passed = expect(log.count() == 0, "a set a run accepts logged a repair") && passed;
  return passed;
}

bool theDefaultsAreReportedWhenTheyDoNotHelp(const char* dataDir) {
  MemoryFiles files;
  if (!rampedImage(files, dataDir)) return false;

  Rows rows;
  RecordingLog log;
  RoutineRegistry registry(files, log);
  registry.add(Registration("ramped_static", "Ramped Static", rows.declaration(), nothingHolds));

  const SettingValues values = registry.loadSettings("ramped_static");
  bool passed =
      expect(secondsOf(values, "total_duration") == 300, "the defaults were not put back");
  passed = expect(log.count() == 2, "a repair that ran out of settings did not log twice") && passed;
  passed = expect(log.endsWith("even at the defaults"),
                  "the repair that ran out of settings did not say so") &&
           passed;
  return passed;
}

bool anAliasReadsItsOwnSection(const char* dataDir) {
  MemoryFiles files;
  if (!rampedImage(files, dataDir)) return false;

  Rows rows;
  RecordingLog log;
  RoutineRegistry registry(files, log);
  registry.add(Registration("ramped_static", "Ramped Static", rows.declaration(), totalCoversHolds));

  const SettingValues root = registry.loadSettings("ramped_static");
  const SettingValues cold = registry.loadSettings("cold");

  bool passed =
      expect(kpaOf(root, "initial_pressure") == 9.0f, "the root set does not read the global section");
  passed = expect(kpaOf(cold, "target_pressure") == 15.0f, "an alias does not read its section") &&
           passed;
  passed = expect(kpaOf(cold, "initial_pressure") == 5.0f,
                  "an alias read a key its section lacks from the root set") &&
           passed;
  passed = expect(titleOf(cold) == "Cold", "an alias does not carry its routine_title") && passed;
  passed = expect(titleOf(root).isEmpty(), "the root set carries an alias's routine_title") && passed;
  return passed;
}

bool aSaveWritesOnceAndLeavesTheRest(const char* dataDir) {
  MemoryFiles files;
  if (!rampedImage(files, dataDir)) return false;

  Rows rows;
  RecordingLog log;
  RoutineRegistry registry(files, log);
  registry.add(Registration("ramped_static", "Ramped Static", rows.declaration(), totalCoversHolds));

  const int before = files.writes();
  SettingValues values = registry.loadSettings("cold");
  values["target_pressure"] = 42.0f;
  if (!expect(registry.saveSettings("cold", values), "the save was refused")) return false;

  bool passed = expect(files.writes() == before + 1, "a settled set did not cost one write");
  String written;
  files.read(Ramped, written);
  passed = expect(strstr(written.c_str(), "routine_title=Cold") != nullptr,
                  "a key the rows do not declare was lost") &&
           passed;
  passed = expect(strstr(written.c_str(), "target_pressure=42") != nullptr,
                  "the changed value is not in the document") &&
           passed;

  const SettingValues root = registry.loadSettings("ramped_static");
  passed = expect(kpaOf(root, "initial_pressure") == 9.0f,
                  "an alias's save disturbed the root set") &&
           passed;
  return passed;
}

bool everyAliasIsListedAfterItsRecord(const char* dataDir) {
  MemoryFiles files;
  if (!rampedImage(files, dataDir)) return false;

  Rows rows;
  RecordingLog log;
  RoutineRegistry registry(files, log);
  const Registration::Glyph glyph = [](gfx::Rect) -> gfx::ui::Paint {
    return [](gfx::Canvas&) {};
  };
  registry.add(
      Registration("ramped_static", "Ramped Static", rows.declaration(), totalCoversHolds, nullptr,
                   glyph));
  registry.add(Registration("interval", "Interval", rows.declaration()));

  const std::vector<String> keys = registry.keys();
  bool passed = expect(keys.size() == 4, "the registry did not list four keys");
  passed = expect(keys[0] == "ramped_static" && keys[1] == "cold" && keys[2] == "warm" &&
                      keys[3] == "interval",
                  "the keys are not in registration order with each alias after its record") &&
           passed;

  const std::vector<RoutineRow> listed = registry.rows();
  passed = expect(listed.size() == 4, "the browser's rows do not match the keys") && passed;
  passed = expect(listed[1].title == "Cold" && listed[1].routineKey == "cold",
                  "an alias's row does not carry its title and key") &&
           passed;
  passed = expect(listed[2].title == "warm", "an alias without a title does not fall back to its key") &&
           passed;
  passed = expect(static_cast<bool>(listed[1].glyph), "an alias's row does not share its glyph") &&
           passed;

  passed = expect(registry.title("cold") == "Cold", "an alias does not show its title") && passed;
  passed = expect(registry.title("ramped_static") == "Ramped Static",
                  "a record does not show its title") &&
           passed;
  passed = expect(registry.title("nope").isEmpty(), "an unregistered key has a title") && passed;

  passed = expect(registry.entry("cold") == registry.registration("ramped_static"),
                  "an alias does not resolve to its record") &&
           passed;
  passed = expect(registry.section("cold") == "cold", "an alias does not read its own section") &&
           passed;
  passed = expect(registry.section("ramped_static").isEmpty(),
                  "a record does not read the global section") &&
           passed;
  passed = expect(registry.indexOf("ramped_static") == 0 && registry.indexOf("interval") == 1,
                  "a record's place is not registration order") &&
           passed;
  passed = expect(registry.indexOf("cold") < 0, "an alias answered a record's place") && passed;

  const auto* declared = registry.settingsRows("cold");
  passed = expect(declared != nullptr && declared->size() == 5,
                  "an alias does not answer its record's declaration") &&
           passed;
  passed = expect(registry.settingsRows("nope") == nullptr,
                  "an unregistered key answers a declaration") &&
           passed;
  passed = expect(registry.icon("cold") != nullptr, "an alias has no glyph") && passed;
  passed = expect(registry.icon("interval") == nullptr,
                  "a record that names no glyph answered one") &&
           passed;
  return passed;
}

bool reRegisteringKeepsThePlaceAndDropsAnAlias(const char* dataDir) {
  MemoryFiles files;
  if (!rampedImage(files, dataDir)) return false;

  Rows rows;
  RecordingLog log;
  RoutineRegistry registry(files, log);
  registry.add(Registration("ramped_static", "Ramped Static", rows.declaration(), totalCoversHolds));
  registry.add(Registration("interval", "Interval", rows.declaration()));

  if (!files.write(Ramped,
                   "initial_pressure=9.0\n[cold]\nroutine_title=Cold\n")) {
    return false;
  }
  registry.add(Registration("ramped_static", "Ramped Again", rows.declaration(), totalCoversHolds));

  const std::vector<String> keys = registry.keys();
  bool passed = expect(keys.size() == 3, "a re-registration left an alias behind");
  passed = expect(keys[0] == "ramped_static" && keys[1] == "cold" && keys[2] == "interval",
                  "a re-registration moved its record") &&
           passed;
  passed = expect(registry.title("warm").isEmpty(), "a re-registration kept a dropped alias") &&
           passed;
  passed = expect(registry.title("ramped_static") == "Ramped Again",
                  "a re-registration kept the old record") &&
           passed;
  return passed;
}

bool aTakenAliasNameIsSkipped(const char* dataDir) {
  MemoryFiles files;
  if (!rampedImage(files, dataDir)) return false;
  if (!files.write("/cfg/routines/interval.ini", "[cold]\nroutine_title=Interval Cold\n")) {
    return false;
  }

  Rows rows;
  RecordingLog log;
  RoutineRegistry registry(files, log);
  registry.add(Registration("ramped_static", "Ramped Static", rows.declaration(), totalCoversHolds));
  registry.add(Registration("interval", "Interval", rows.declaration()));

  const std::vector<String> keys = registry.keys();
  bool passed = expect(keys.size() == 4, "a taken alias name was listed twice");
  passed = expect(registry.title("cold") == "Cold", "a taken alias name replaced the first") && passed;
  passed = expect(log.holds("registry: interval alias cold already taken"),
                  "a taken alias name was not reported") &&
           passed;
  return passed;
}

bool aKeyThatIsNotAPlainWordIsRefused(const char* dataDir) {
  MemoryFiles files;
  if (!rampedImage(files, dataDir)) return false;

  Rows rows;
  RecordingLog log;
  RoutineRegistry registry(files, log);

  bool passed = expect(!registry.add(Registration("a/b", "Bad", rows.declaration())),
                       "a key with a slash was accepted");
  passed = expect(!registry.add(Registration("", "Bad", rows.declaration())),
                  "an empty key was accepted") &&
           passed;
  passed = expect(log.holds("registry: routine key must be a plain word"),
                  "a refused key was not reported") &&
           passed;
  passed = expect(registry.keys().empty(), "a refused record was registered") && passed;
  return passed;
}

class Probe : public Scene {
 public:
  Probe(GestureQueue& gestures, const RoutineActivation& activation)
      : Scene(gestures), routineKey(activation.routineKey), values(activation.values) {}

  String routineKey;
  SettingValues values;
};

bool aLaunchBuildsTheRecordFromItsDocument(const char* dataDir) {
  MemoryFiles files;
  if (!rampedImage(files, dataDir)) return false;

  Rows rows;
  RecordingLog log;
  RoutineRegistry registry(files, log);
  const Registration::Factory factory = [&rows](const RoutineActivation& activation) {
    return std::make_unique<Probe>(rows.gestures, activation);
  };
  registry.add(Registration("ramped_static", "Ramped Static", rows.declaration(), totalCoversHolds,
                            factory));
  registry.add(Registration("interval", "Interval", rows.declaration()));

  Ux ux;
  if (!expect(registry.push("ramped_static", ux), "the launch was refused")) return false;

  bool passed = expect(ux.depth() == 1, "the launch pushed no page");
  const auto* probe = static_cast<const Probe*>(ux.top());
  passed = expect(probe != nullptr && probe->routineKey == "ramped_static",
                  "the page was not launched under its key") &&
           passed;
  passed = expect(probe != nullptr && secondsOf(probe->values, "total_duration") == 400,
                  "the page was not launched on the document's set") &&
           passed;

  passed = expect(!registry.push("interval", ux), "a record with no page was launched") && passed;
  passed = expect(!registry.push("nope", ux), "an unregistered key was launched") && passed;
  return passed;
}

bool aKeyItDoesNotHoldAnswersNothing(const char* dataDir) {
  MemoryFiles files;
  if (!rampedImage(files, dataDir)) return false;

  Rows rows;
  RecordingLog log;
  RoutineRegistry registry(files, log);
  registry.add(Registration("ramped_static", "Ramped Static", rows.declaration(), totalCoversHolds));

  SettingValues values;
  bool passed = expect(registry.entry("nope") == nullptr, "an unregistered key has a record");
  passed = expect(registry.registration("cold") == nullptr,
                  "an alias answered as a record") &&
           passed;
  passed = expect(registry.loadSettings("nope").empty(), "an unregistered key has a set") && passed;
  passed = expect(!registry.saveSettings("nope", values), "an unregistered key's set was written") &&
           passed;
  return passed;
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

bool runRoutineRegistryCases(const char* dataDir) {
  bool passed = true;
  run("a declaration keeps its order", aDeclarationKeepsItsOrder, dataDir, passed);
  run("a refused set is repaired", aRefusedSetIsRepaired, dataDir, passed);
  run("the defaults are reported when they do not help", theDefaultsAreReportedWhenTheyDoNotHelp,
      dataDir, passed);
  run("an alias reads its own section", anAliasReadsItsOwnSection, dataDir, passed);
  run("a save writes once and leaves the rest", aSaveWritesOnceAndLeavesTheRest, dataDir, passed);
  run("every alias is listed after its record", everyAliasIsListedAfterItsRecord, dataDir, passed);
  run("re-registering keeps the place and drops an alias", reRegisteringKeepsThePlaceAndDropsAnAlias,
      dataDir, passed);
  run("a taken alias name is skipped", aTakenAliasNameIsSkipped, dataDir, passed);
  run("a key that is not a plain word is refused", aKeyThatIsNotAPlainWordIsRefused, dataDir, passed);
  run("a launch builds the record from its document", aLaunchBuildsTheRecordFromItsDocument, dataDir,
      passed);
  run("a key it does not hold answers nothing", aKeyItDoesNotHoldAnswersNothing, dataDir, passed);
  return passed;
}

}  // namespace gardener
