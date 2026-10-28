// The diagnostic sink: the [I], [W] and [E] records a level set writes, and the levels it does not
// write at all. The stream is the suite's, so a case reads the exact bytes a reader on the console
// would see.

#include <doctest.h>

#include <string>

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
  logger.error("stopped");
  CHECK_EQ(serial.printed(), std::string("[I] ready\n[W] low\n[E] stopped\n"));
}

TEST_CASE("a level outside the set writes nothing") {
  TextStream serial;
  SerialLogger logger(serial);
  logger.setLevels("E");
  logger.info("ready");
  logger.warn("low");
  logger.error("stopped");
  CHECK_EQ(serial.printed(), std::string("[E] stopped\n"));
}

TEST_CASE("a set of one level leaves the others silent") {
  TextStream serial;
  SerialLogger logger(serial);
  logger.setLevels("W");
  logger.info("ready");
  logger.warn("low");
  CHECK_EQ(serial.printed(), std::string("[W] low\n"));
  CHECK_EQ(std::string(logger.levels().c_str()), std::string("W"));
}

TEST_CASE("an empty set writes nothing at all") {
  TextStream serial;
  SerialLogger logger(serial);
  logger.setLevels("-");
  logger.info("ready");
  logger.warn("low");
  logger.error("stopped");
  CHECK_EQ(serial.printed(), std::string(""));
}

TEST_CASE("a record with no text is not written") {
  TextStream serial;
  SerialLogger logger(serial);
  logger.info(nullptr);
  CHECK_EQ(serial.printed(), std::string(""));
}
