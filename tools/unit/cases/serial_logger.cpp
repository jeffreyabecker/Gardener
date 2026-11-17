// The diagnostic sink: the [I] and [W] records it writes. The stream is the suite's, so a case reads
// the exact bytes a reader on the console would see.

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
  CHECK_EQ(serial.printed(), std::string("[I] ready\n[W] low\n"));
}

TEST_CASE("a record with no text is not written") {
  TextStream serial;
  SerialLogger logger(serial);
  logger.info(nullptr);
  CHECK_EQ(serial.printed(), std::string(""));
}
