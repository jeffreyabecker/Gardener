// The settings document: the shape Config keeps as it parses, the types it reads out of that text,
// and the one write a save costs. The store is the suite's in-memory Library, so a case owns every
// byte the parser sees and can count the writes it made.

#include <doctest.h>

#include <Arduino.h>

#include <string>

#include "fakes/MemoryLibrary.h"
#include "storage/Config.h"

namespace {

using gardener::Config;
using gardener::test::MemoryLibrary;

// A document with every shape the parser has to survive: comments, a blank line, a line with no
// equals sign, a header with spaces in it, keys before the first header, a key that appears twice,
// and a value holding an equals sign.
constexpr const char* Document =
    "; a comment\n"
    "\n"
    "no equals here\n"
    "loose=1\n"
    "[ cold ]\n"
    "target=15.0\n"
    "target=16.0\n"
    "expression=a=b\n";

constexpr const char* Path = "/cfg/settings.ini";

struct Bench {
  Bench() {
    library.put(Path, Document);
    REQUIRE(config.begin());
  }

  MemoryLibrary library;
  Config config{&library, Path};
};

}  // namespace

TEST_CASE("a comment, a blank line and a line with no equals sign are ignored") {
  Bench bench;
  CHECK_EQ(std::string(bench.config.get<String>("", "no equals here", "fallback").c_str()),
           std::string("fallback"));
  CHECK_EQ(std::string(bench.config.get<String>("", "a comment", "fallback").c_str()),
           std::string("fallback"));
}

TEST_CASE("a header is trimmed to its name") {
  Bench bench;
  CHECK_EQ(std::string(bench.config.get<String>("cold", "target", "").c_str()),
           std::string("16.0"));
}

TEST_CASE("keys before the first header belong to the empty section") {
  Bench bench;
  CHECK_EQ(bench.config.get<int>("", "loose", 0), 1);
}

TEST_CASE("the first equals splits, so a value may hold one") {
  Bench bench;
  CHECK_EQ(std::string(bench.config.get<String>("cold", "expression", "").c_str()),
           std::string("a=b"));
}

TEST_CASE("a key that appears twice keeps the last value") {
  Bench bench;
  CHECK_EQ(bench.config.get<float>("cold", "target", 0.0f), 16.0f);
}

TEST_CASE("a save writes the sections in the order they first appeared") {
  Bench bench;
  bench.config.set("cold", "target", 16.0f);
  REQUIRE(bench.config.save());
  const std::string written = bench.library.textAt(Path);
  CHECK(written.find("loose=1") == 0);
  CHECK(written.find("[cold]") != std::string::npos);
  CHECK(written.find("[ cold ]") == std::string::npos);
}

TEST_CASE("a save keeps the keys it does not know") {
  Bench bench;
  bench.config.set("cold", "target", 42.0f);
  REQUIRE(bench.config.save());
  const std::string written = bench.library.textAt(Path);
  CHECK(written.find("target=42.00") != std::string::npos);
  CHECK(written.find("expression=a=b") != std::string::npos);
}

TEST_CASE("the words that mean on are on") {
  MemoryLibrary library;
  library.put(Path, "a=on\nb=TRUE\nc=1\nd=Yes\n");
  Config config(&library, Path);
  REQUIRE(config.begin());
  CHECK(config.get<bool>("", "a", false));
  CHECK(config.get<bool>("", "b", false));
  CHECK(config.get<bool>("", "c", false));
  CHECK(config.get<bool>("", "d", false));
}

TEST_CASE("the words that mean off are off") {
  MemoryLibrary library;
  library.put(Path, "a=off\nb=False\nc=0\nd=NO\n");
  Config config(&library, Path);
  REQUIRE(config.begin());
  CHECK_FALSE(config.get<bool>("", "a", true));
  CHECK_FALSE(config.get<bool>("", "b", true));
  CHECK_FALSE(config.get<bool>("", "c", true));
  CHECK_FALSE(config.get<bool>("", "d", true));
}

TEST_CASE("a value that is not a switch leaves the fallback") {
  MemoryLibrary library;
  library.put(Path, "a=maybe\n");
  Config config(&library, Path);
  REQUIRE(config.begin());
  CHECK(config.get<bool>("", "a", true));
}

TEST_CASE("a missing document leaves every default") {
  MemoryLibrary library;
  Config config(&library, Path);
  CHECK_FALSE(config.begin());
  CHECK_EQ(config.get<int>("cold", "target", 7), 7);
  CHECK_EQ(std::string(config.get<String>("cold", "name", "none").c_str()), std::string("none"));
}

TEST_CASE("five sets and one save write once") {
  Bench bench;
  bench.library.clearWrites();
  bench.config.set("cold", "target", 20.0f);
  bench.config.set("cold", "other", 1);
  bench.config.set("", "loose", 2);
  bench.config.set("warm", "target", 30.0f);
  bench.config.set("warm", "enabled", true);
  CHECK_EQ(bench.library.writes(), 0);
  REQUIRE(bench.config.save());
  CHECK_EQ(bench.library.writes(), 1);
}

TEST_CASE("a second save with nothing changed writes nothing") {
  Bench bench;
  bench.config.set("cold", "target", 20.0f);
  REQUIRE(bench.config.save());
  const int after = bench.library.writes();
  REQUIRE(bench.config.save());
  CHECK_EQ(bench.library.writes(), after);
}
