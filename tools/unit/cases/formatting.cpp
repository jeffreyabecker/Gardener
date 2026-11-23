// The line writer: the order of the parts, a pair's decimals and a bare number's default.

#include <doctest.h>

#include <Arduino.h>

#include <string>
#include <utility>

#include "TextStream.h"
#include "core/Formatting.h"

namespace {

using gardener::print;
using gardener::host::TextStream;

// A part that goes through Print's Printable overload rather than a String.
class Stamp : public Printable {
 public:
  size_t printTo(Print& io) const override { return io.print("stamp"); }
};

}  // namespace

TEST_CASE("print writes every part in order and leaves the line open") {
  TextStream io;
  print(io, "ok count ", 3);
  CHECK_EQ(io.printed(), std::string("ok count 3"));
  print(io, " done");
  CHECK_EQ(io.printed(), std::string("ok count 3 done"));
}

TEST_CASE("a pair writes its value to the decimals it names") {
  TextStream io;
  print(io, std::make_pair(5.0f, 2), ' ', std::make_pair(60.0f, 1), ' ', std::make_pair(5.0f, 0));
  CHECK_EQ(io.printed(), std::string("5.00 60.0 5"));
}

TEST_CASE("a bare number takes Print's own format") {
  TextStream io;
  print(io, 5.0f, ' ', 7u, ' ', 2.5);
  CHECK_EQ(io.printed(), std::string("5.00 7 2.50"));
}

TEST_CASE("a printable part is written through printTo") {
  TextStream io;
  print(io, "v=", Stamp());
  CHECK_EQ(io.printed(), std::string("v=stamp"));
}

TEST_CASE("format writes the parts into one string") {
  const String line = gardener::format("ok ", 5, " ", std::make_pair(2.0f, 1));
  CHECK_EQ(std::string(line.c_str()), std::string("ok 5 2.0"));
}
