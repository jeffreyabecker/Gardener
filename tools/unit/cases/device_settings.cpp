// The device's document as its own object: the two values it answers in, the fallbacks a document
// without them keeps, and the keys it does not own surviving a write. The store is the suite's
// in-memory Library, so a case owns every byte and can count the writes.

#include <doctest.h>

#include <Arduino.h>

#include <string>

#include "fakes/MemoryLibrary.h"
#include "storage/DeviceSettings.h"

namespace {

using gardener::DeviceSettings;
using gardener::DisplayUnit;
using gardener::test::MemoryLibrary;

constexpr const char* Path = "/cfg/settings.ini";

struct Bench {
  explicit Bench(const char* document) {
    if (document != nullptr) library.put(Path, document);
    read = settings.begin();
  }

  MemoryLibrary library;
  DeviceSettings settings{&library};
  bool read = false;
};

}  // namespace

TEST_CASE("a document with no device section reads kPa and no target") {
  Bench bench("[jingle]\nname=poacher\n");
  CHECK(bench.read);
  CHECK(bench.settings.unit() == DisplayUnit::Kpa);
  CHECK_EQ(bench.settings.targetKpa(), 0.0f);
}

TEST_CASE("a unit name this firmware does not hold reads kPa") {
  Bench bench("[device]\nunit=furlongs\n");
  CHECK(bench.settings.unit() == DisplayUnit::Kpa);
}

TEST_CASE("a stored target at ambient reads as none") {
  Bench bench("[device]\ntarget=0.0\n");
  CHECK_EQ(bench.settings.targetKpa(), 0.0f);
}

TEST_CASE("the unit and the target are read as what they mean") {
  Bench bench("[device]\nunit=psi\ntarget=15.5\n");
  CHECK(bench.settings.unit() == DisplayUnit::Psi);
  CHECK_EQ(bench.settings.targetKpa(), 15.5f);
}

TEST_CASE("a set and a save write once and read back, with a key it does not own kept") {
  Bench bench("[device]\nunit=kpa\njingle=poacher\n");
  bench.settings.setUnit(DisplayUnit::InHg);
  bench.settings.setTargetKpa(22.0f);
  CHECK(bench.settings.save());
  CHECK_EQ(bench.library.writes(), 1);

  DeviceSettings again{&bench.library};
  CHECK(again.begin());
  CHECK(again.unit() == DisplayUnit::InHg);
  CHECK_EQ(again.targetKpa(), 22.0f);

  const std::string text = bench.library.textAt(Path);
  CHECK(text.find("jingle=poacher") != std::string::npos);
  CHECK(text.find("unit=inhg") != std::string::npos);
}

TEST_CASE("a save with nothing changed writes nothing") {
  Bench bench("[device]\nunit=kpa\n");
  CHECK(bench.settings.save());
  CHECK_EQ(bench.library.writes(), 0);
}

TEST_CASE("a build with no store behind it reads its defaults") {
  DeviceSettings settings{nullptr};
  CHECK_FALSE(settings.begin());
  CHECK(settings.unit() == DisplayUnit::Kpa);
  CHECK_EQ(settings.targetKpa(), 0.0f);

  // There is nowhere to write a change to, which is the one case a save answers false on.
  settings.setUnit(DisplayUnit::Psi);
  CHECK_FALSE(settings.save());
}
