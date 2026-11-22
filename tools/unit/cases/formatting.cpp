// The wire's line writer: the order of the parts, the LF that ends the line, a pair's decimals, a
// bare number's default, and the quoting a value needs to read back as one token.

#include <doctest.h>

#include <Arduino.h>

#include <string>
#include <utility>

#include "TextStream.h"
#include "core/Formatting.h"

namespace {

using gardener::print;
using gardener::println;
using gardener::quote;
using gardener::host::TextStream;

// A part that goes through Print's Printable overload rather than a String.
class Stamp : public Printable {
 public:
  size_t printTo(Print& io) const override { return io.print("stamp"); }
};

}  // namespace

TEST_CASE("print writes every part in order and leaves the line open") {
  TextStream io;
  print(io, "ok count=", 3);
  CHECK_EQ(io.printed(), std::string("ok count=3"));
  println(io, " done");
  CHECK_EQ(io.printed(), std::string("ok count=3 done\n"));
}

TEST_CASE("println writes every part in order and ends the line") {
  TextStream io;
  println(io, "ok target=", 5, " state=", "PULL");
  CHECK_EQ(io.printed(), std::string("ok target=5 state=PULL\n"));
}

TEST_CASE("a line of no parts is just the newline") {
  TextStream io;
  println(io);
  CHECK_EQ(io.printed(), std::string("\n"));
}

TEST_CASE("a pair writes its value to the decimals it names") {
  TextStream io;
  println(io, std::make_pair(5.0f, 2), ' ', std::make_pair(60.0f, 1), ' ', std::make_pair(5.0f, 0));
  CHECK_EQ(io.printed(), std::string("5.00 60.0 5\n"));
}

TEST_CASE("a bare number takes Print's own format") {
  TextStream io;
  println(io, 5.0f, ' ', 7u, ' ', 2.5);
  CHECK_EQ(io.printed(), std::string("5.00 7 2.50\n"));
}

TEST_CASE("quote writes a token bare and quotes what a token cannot hold") {
  TextStream io;
  println(io, quote("Home"), ' ', quote("My Net"), ' ', quote("Bob\"s"), ' ', quote(""));
  CHECK_EQ(io.printed(), std::string("Home \"My Net\" \"Bob\\\"s\" \"\"\n"));
}

TEST_CASE("quote escapes a backslash inside the quotes") {
  TextStream io;
  println(io, quote("a\\b"));
  CHECK_EQ(io.printed(), std::string("\"a\\\\b\"\n"));
}

TEST_CASE("a printable part is written through printTo") {
  TextStream io;
  println(io, "v=", Stamp());
  CHECK_EQ(io.printed(), std::string("v=stamp\n"));
}

TEST_CASE("format writes the parts into one string") {
  const String line = gardener::format("ok ", 5, " ", std::make_pair(2.0f, 1));
  CHECK_EQ(std::string(line.c_str()), std::string("ok 5 2.0"));
}
