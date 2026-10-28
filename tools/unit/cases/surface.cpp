#include <doctest.h>

#include <Arduino.h>

#include <string>

#include "Clock.h"
#include "TextStream.h"

using gardener::host::TextStream;

TEST_CASE("a float prints the decimals it is asked for") {
  CHECK(String(1.5f, 1) == "1.5");
  CHECK(String(2.25f, 2) == "2.25");
}

TEST_CASE("a text grown past its first allocation keeps its bytes") {
  String text = "short";
  for (int i = 0; i < 64; i++) text += "0123456789";
  CHECK(text.length() == 5 + 64 * 10);
  CHECK(text.endsWith("0123456789"));
}

TEST_CASE("a stream hands a domain the text typed and holds what it printed") {
  TextStream io;
  io.type("vacuum get\n");
  CHECK(io.available() == 11);
  CHECK(io.read() == 'v');
  CHECK(io.peek() == 'a');
  io.clearPrinted();
  io.print("ok state=IDLE\n");
  CHECK(io.printed() == std::string("ok state=IDLE\n"));
}

TEST_CASE("the clock a case advances is the millis the firmware reads") {
  gardener::host::reset();
  CHECK(millis() == 0);
  gardener::host::advance(250);
  CHECK(millis() == 250);
  delay(100);
  CHECK(millis() == 350);
}
