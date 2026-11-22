// The diagnostic sink: the [I] and [W] records it writes. The stream is the suite's, so a case reads
// the exact bytes a reader on the console would see.

#include <doctest.h>

#include <string>
#include <utility>

#include "TextStream.h"
#include "core/SerialLogger.h"

namespace {

using gardener::SerialLogger;
using gardener::host::TextStream;

}  // namespace

TEST_CASE("every level is written from boot") {
  TextStream serial;
  SerialLogger logger(serial);
  logger.info("ready");
  logger.warn("low");
  CHECK_EQ(serial.printed(), std::string("[I] ready\n[W] low\n"));
}

TEST_CASE("the level is the first argument of log") {
  TextStream serial;
  SerialLogger logger(serial);
  logger.log(gardener::LogLevel::Warning, "low");
  logger.log(gardener::LogLevel::Info, "ready");
  CHECK_EQ(serial.printed(), std::string("[W] low\n[I] ready\n"));
}

TEST_CASE("the levels are the syslog numbers, so the smaller is the more severe") {
  CHECK_EQ(static_cast<int>(gardener::LogLevel::Emergency), 0);
  CHECK_EQ(static_cast<int>(gardener::LogLevel::Warning), 4);
  CHECK_EQ(static_cast<int>(gardener::LogLevel::Info), 6);
  CHECK_EQ(static_cast<int>(gardener::LogLevel::Debug), 7);
  CHECK(gardener::LogLevel::Error < gardener::LogLevel::Warning);
  CHECK(gardener::LogLevel::Critical < gardener::LogLevel::Error);
}

TEST_CASE("every level has its own frame letter") {
  TextStream serial;
  SerialLogger logger(serial);
  logger.log(gardener::LogLevel::Emergency, "a");
  logger.log(gardener::LogLevel::Alert, "b");
  logger.log(gardener::LogLevel::Critical, "c");
  logger.log(gardener::LogLevel::Error, "d");
  logger.log(gardener::LogLevel::Warning, "e");
  logger.log(gardener::LogLevel::Notice, "f");
  logger.log(gardener::LogLevel::Info, "g");
  logger.log(gardener::LogLevel::Debug, "h");
  CHECK_EQ(serial.printed(),
           std::string("[E] a\n[A] b\n[C] c\n[R] d\n[W] e\n[N] f\n[I] g\n[D] h\n"));
}

TEST_CASE("a record's parts are formatted the way print writes them") {
  TextStream serial;
  SerialLogger logger(serial);
  logger.info("target=", std::make_pair(5.0f, 2), " state=", "PULL");
  logger.warn("fault code ", 3);
  logger.log(gardener::LogLevel::Error, "pressure=", std::make_pair(1.5f, 1));
  CHECK_EQ(serial.printed(), std::string("[I] target=5.00 state=PULL\n"
                                         "[W] fault code 3\n"
                                         "[R] pressure=1.5\n"));
}

TEST_CASE("a logger starts at Debug and keeps the level it is given") {
  TextStream serial;
  SerialLogger logger(serial);
  CHECK(logger.level() == gardener::LogLevel::Debug);
  logger.setLevel(gardener::LogLevel::Info);
  CHECK(logger.level() == gardener::LogLevel::Info);
}

TEST_CASE("a record above the current level is not written") {
  TextStream serial;
  SerialLogger logger(serial);
  logger.setLevel(gardener::LogLevel::Warning);
  logger.info("quiet");
  logger.log(gardener::LogLevel::Debug, "quieter");
  logger.warn("loud");
  logger.log(gardener::LogLevel::Error, "loudest");
  CHECK_EQ(serial.printed(), std::string("[W] loud\n[R] loudest\n"));
}

TEST_CASE("a record with no text is not written") {
  TextStream serial;
  SerialLogger logger(serial);
  logger.info(nullptr);
  CHECK_EQ(serial.printed(), std::string(""));
}
