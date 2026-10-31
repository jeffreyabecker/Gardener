// The jingle library as its document leaves it: the entries it holds, the key lookup a page makes,
// and the section it reports and leaves out. The store is the suite's in-memory Library, so a case
// owns the document.

#include <doctest.h>

#include <Arduino.h>

#include <string>

#include "fakes/MemoryLibrary.h"
#include "fakes/Parts.h"
#include "storage/JingleLibrary.h"

namespace {

using gardener::Jingle;
using gardener::JingleLibrary;
using gardener::test::MemoryLibrary;
using gardener::test::RecordingLogger;

constexpr const char* Path = "/cfg/jingles.ini";
constexpr const char* Fixture = "[poacher]\n"
                                "full_title=Lincolnshire Poacher\n"
                                "short_title=Poacher\n"
                                "notes=E5:200 G5:200\n"
                                "\n"
                                "[broken]\n"
                                "short_title=Broken\n"
                                "\n"
                                "[forelle]\n"
                                "short_title=Forelle\n"
                                "notes=D5:200\n";

struct Bench {
  explicit Bench(const char* document) {
    if (document != nullptr) library.put(Path, document);
    read = jingles.begin();
  }

  MemoryLibrary library;
  RecordingLogger logger;
  JingleLibrary jingles{&library, logger};
  bool read = false;
};

}  // namespace

TEST_CASE("the library holds the sections its document completes") {
  Bench bench(Fixture);
  CHECK(bench.read);
  REQUIRE_EQ(bench.jingles.entries().size(), 2u);
  CHECK_EQ(std::string(bench.jingles.entries()[0].key.c_str()), std::string("poacher"));
  CHECK_EQ(std::string(bench.jingles.entries()[0].shortTitle.c_str()), std::string("Poacher"));
  CHECK_EQ(std::string(bench.jingles.entries()[0].notes.c_str()), std::string("E5:200 G5:200"));
  CHECK_EQ(std::string(bench.jingles.entries()[1].key.c_str()), std::string("forelle"));
}

TEST_CASE("a section missing a key is left out and reported") {
  Bench bench(Fixture);
  CHECK(bench.jingles.find("broken") == nullptr);
  CHECK(bench.logger.saw("jingles: broken is incomplete"));
}

TEST_CASE("a key names the entry it holds, or nothing") {
  Bench bench(Fixture);
  const Jingle* found = bench.jingles.find("forelle");
  REQUIRE(found != nullptr);
  CHECK_EQ(std::string(found->shortTitle.c_str()), std::string("Forelle"));
  CHECK(bench.jingles.find("nobody") == nullptr);
}

TEST_CASE("a build with no store has no jingles") {
  RecordingLogger logger;
  JingleLibrary jingles{nullptr, logger};
  CHECK_FALSE(jingles.begin());
  CHECK(jingles.entries().empty());
}
