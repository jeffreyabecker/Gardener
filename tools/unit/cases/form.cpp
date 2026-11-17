// The form codec the HTTP API speaks: what a browser posts and what the page reads back. It is the
// sketch's, but it names nothing of the board, so the suite compiles the same header the firmware
// does.

#include <doctest.h>

#include <Arduino.h>

#include <string>

#include "Form.h"

using gardener::runtime::formDecoded;
using gardener::runtime::formEncoded;
using gardener::runtime::formField;

TEST_CASE("a form field is found by name") {
  CHECK_EQ(std::string(formField("target=40&keep_ap=1", "target").c_str()), std::string("40"));
  CHECK_EQ(std::string(formField("target=40&keep_ap=1", "keep_ap").c_str()), std::string("1"));
}

TEST_CASE("a form field is decoded") {
  CHECK_EQ(std::string(formField("ssid=My+Net%21&pass=a%26b", "ssid").c_str()),
           std::string("My Net!"));
  CHECK_EQ(std::string(formField("ssid=My+Net%21&pass=a%26b", "pass").c_str()), std::string("a&b"));
}

TEST_CASE("a field the body does not hold is empty") {
  CHECK_EQ(std::string(formField("target=40", "ssid").c_str()), std::string(""));
  CHECK_EQ(std::string(formField("", "target").c_str()), std::string(""));
}

TEST_CASE("an encoded value carries its separators as escapes") {
  CHECK_EQ(std::string(formEncoded("My Net!&=+").c_str()), std::string("My%20Net%21%26%3D%2B"));
}

TEST_CASE("a value survives the encode and decode round trip") {
  const char* awkward = "a b&c=d%20+e/\\?#";
  CHECK_EQ(std::string(formDecoded(formEncoded(awkward)).c_str()), std::string(awkward));
}

TEST_CASE("a malformed escape is left as it was typed") {
  CHECK_EQ(std::string(formDecoded("100%&a%2").c_str()), std::string("100%&a%2"));
}
