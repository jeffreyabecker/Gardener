// A frame a case drew, saved through the writer the tools share: the still and the sequence, and
// the bytes a GIF89a starts with. The frame is packed here the way a display buffer holds it, which
// is the writer's only input.

#include <doctest.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "GifWriter.h"
#include "suite.h"

namespace {

using gardener::Capture;
using gardener::GifDelayCs;
using gardener::RenderScale;
using gardener::gfx::test::display;
using gardener::gfx::test::PageHeight;
using gardener::gfx::test::PageWidth;
using gardener::gfx::test::reset;

// The frame the stub display holds, packed one bit per pixel, row-major and top-down.
std::vector<uint8_t> packedFrame() {
  const int rowBytes = (PageWidth + 7) / 8;
  std::vector<uint8_t> packed(static_cast<size_t>(rowBytes) * PageHeight, 0);
  for (int y = 0; y < PageHeight; y++) {
    for (int x = 0; x < PageWidth; x++) {
      if (display.at(x, y) == 0) continue;
      packed[static_cast<size_t>(y) * rowBytes + x / 8] |= static_cast<uint8_t>(0x80 >> (x & 7));
    }
  }
  return packed;
}

std::vector<uint8_t> readFile(const char* path) {
  std::ifstream file(path, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
}

// A rectangle of ink at the top left, which is enough to make a frame non-empty.
void drawMark() {
  reset();
  gardener::gfx::test::canvas.flush(display);
  gardener::gfx::test::canvas.setPixel(10, 10, 1);
  gardener::gfx::test::canvas.setPixel(11, 10, 1);
  gardener::gfx::test::canvas.flush(display);
}

int countFrameBlocks(const std::vector<uint8_t>& bytes) {
  int count = 0;
  for (size_t i = 0; i + 2 < bytes.size(); i++) {
    if (bytes[i] == 0x21 && bytes[i + 1] == 0xF9 && bytes[i + 2] == 0x04) count++;
  }
  return count;
}

uint16_t littleEndian(const std::vector<uint8_t>& bytes, size_t at) {
  return static_cast<uint16_t>(bytes[at] | (bytes[at + 1] << 8));
}

}  // namespace

TEST_CASE("a still a core panel scene drew is a GIF89a at twice the page") {
  drawMark();
  const std::vector<uint8_t> packed = packedFrame();
  Capture capture;
  capture.arm(PageWidth, PageHeight);
  capture.add(packed.data());
  REQUIRE(capture.save("tools/unit/build/capture_core_still.gif"));

  const std::vector<uint8_t> bytes = readFile("tools/unit/build/capture_core_still.gif");
  REQUIRE(bytes.size() > 20);
  CHECK_EQ(std::string(reinterpret_cast<const char*>(bytes.data()), 6), std::string("GIF89a"));
  CHECK_EQ(littleEndian(bytes, 6), static_cast<uint16_t>(PageWidth * RenderScale));
  CHECK_EQ(littleEndian(bytes, 8), static_cast<uint16_t>(PageHeight * RenderScale));
  std::remove("tools/unit/build/capture_core_still.gif");
}

TEST_CASE("the palette is white paper then black ink") {
  drawMark();
  const std::vector<uint8_t> packed = packedFrame();
  Capture capture;
  capture.arm(PageWidth, PageHeight);
  capture.add(packed.data());
  REQUIRE(capture.save("tools/unit/build/capture_palette.gif"));

  const std::vector<uint8_t> bytes = readFile("tools/unit/build/capture_palette.gif");
  REQUIRE(bytes.size() > 19);
  CHECK_EQ(bytes[13], 0xFF);
  CHECK_EQ(bytes[14], 0xFF);
  CHECK_EQ(bytes[15], 0xFF);
  CHECK_EQ(bytes[16], 0x00);
  CHECK_EQ(bytes[17], 0x00);
  CHECK_EQ(bytes[18], 0x00);
  std::remove("tools/unit/build/capture_palette.gif");
}

TEST_CASE("the delay a frame is held for is the writer's own") {
  drawMark();
  const std::vector<uint8_t> packed = packedFrame();
  Capture capture;
  capture.arm(PageWidth, PageHeight);
  capture.add(packed.data());
  REQUIRE(capture.save("tools/unit/build/capture_delay.gif"));

  const std::vector<uint8_t> bytes = readFile("tools/unit/build/capture_delay.gif");
  size_t at = 0;
  while (at + 5 < bytes.size() && !(bytes[at] == 0x21 && bytes[at + 1] == 0xF9)) at++;
  REQUIRE(at + 5 < bytes.size());
  CHECK_EQ(littleEndian(bytes, at + 4), GifDelayCs);
  std::remove("tools/unit/build/capture_delay.gif");
}

TEST_CASE("an armed capture keeps every frame it was handed") {
  drawMark();
  const std::vector<uint8_t> packed = packedFrame();
  Capture capture;
  capture.arm(PageWidth, PageHeight);
  capture.add(packed.data());
  capture.add(packed.data());
  capture.add(packed.data());
  CHECK_EQ(capture.frameCount(), 3);
  REQUIRE(capture.save("tools/unit/build/capture_sequence.gif"));

  const std::vector<uint8_t> bytes = readFile("tools/unit/build/capture_sequence.gif");
  CHECK_EQ(countFrameBlocks(bytes), 3);
  std::remove("tools/unit/build/capture_sequence.gif");
}

TEST_CASE("a capture with nothing collected answers false") {
  Capture capture;
  capture.arm(PageWidth, PageHeight);
  CHECK_FALSE(capture.save("tools/unit/build/capture_empty.gif"));
}

TEST_CASE("a path that cannot be opened answers false without ending the run") {
  drawMark();
  const std::vector<uint8_t> packed = packedFrame();
  Capture capture;
  capture.arm(PageWidth, PageHeight);
  capture.add(packed.data());
  CHECK_FALSE(capture.save("tools/unit/build/no such folder/capture.gif"));
}

TEST_CASE("a still a Gfx component drew is saved the same way") {
  drawMark();
  const std::vector<uint8_t> packed = packedFrame();
  Capture capture;
  capture.arm(PageWidth, PageHeight);
  capture.add(packed.data());
  REQUIRE(capture.save("tools/unit/build/capture_gfx_still.gif"));

  const std::vector<uint8_t> bytes = readFile("tools/unit/build/capture_gfx_still.gif");
  REQUIRE(bytes.size() > 20);
  CHECK_EQ(std::string(reinterpret_cast<const char*>(bytes.data()), 6), std::string("GIF89a"));
  std::remove("tools/unit/build/capture_gfx_still.gif");
}
