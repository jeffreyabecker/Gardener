// A frame a case drew, saved through the writer the tools share: the still and the sequence, and
// the bytes a GIF89a starts with. The frame is packed by hand where the writer is what is under
// test; a page driven through the suite's own display leaves the packing, the frame it is placed in
// and the components drawn over it to the capture. A capture of a page carries the gesture the case
// named as a strip under the glass, and the overlay rows a case did not draw.

#include <doctest.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "GestureStrip.h"
#include "GifWriter.h"
#include "panel/page/Scene.h"
#include "panel/page/Ux.h"
#include "suite.h"

namespace {

using gardener::Capture;
using gardener::CaptureDisplay;
using gardener::GestureStrip;
using gardener::GifDelayCs;
using gardener::RenderScale;
using gardener::gfx::View;
using gardener::gfx::Canvas;
using gardener::gfx::callable;
using gardener::gfx::Rect;
using gardener::panel::GestureQueue;
using gardener::panel::page::Scene;
using gardener::panel::page::Ux;
using gardener::gfx::test::display;
using gardener::gfx::test::PageHeight;
using gardener::gfx::test::PageWidth;
using gardener::gfx::test::reset;

// Whether one bit of a frame packed one bit per pixel, row-major and top-down, is ink.
bool packedAt(const std::vector<uint8_t>& frame, int16_t width, int x, int y) {
  const int rowBytes = (width + 7) / 8;
  return (frame[static_cast<size_t>(y) * rowBytes + x / 8] & (0x80 >> (x & 7))) != 0;
}

bool packedInk(const std::vector<uint8_t>& frame, int16_t width, int x, int y, int w, int h) {
  for (int row = y; row < y + h; row++) {
    for (int column = x; column < x + w; column++) {
      if (packedAt(frame, width, column, row)) return true;
    }
  }
  return false;
}

// The frame the display holds, packed one bit per pixel, row-major and top-down.
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
  gardener::gfx::test::display.flush();
  gardener::gfx::test::canvas.setPixel(10, 10, 1);
  gardener::gfx::test::canvas.setPixel(11, 10, 1);
  gardener::gfx::test::display.flush();
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

// Arms the shared display with the picture a harness capture writes: the glass at the top, a
// strip's rows under it, and the strip in them.
void beginStripCapture(GestureStrip& strip) {
  const Rect stripBox(0, PageHeight, PageWidth, GestureStrip::Rows);
  display.beginCapture(CaptureDisplay::Frame{PageWidth,
                                             static_cast<int16_t>(PageHeight + GestureStrip::Rows),
                                             Rect(0, 0, PageWidth, PageHeight)},
                       strip.component(stripBox));
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

TEST_CASE("a recording display keeps one frame per flush that drew") {
  reset();
  display.beginCapture();
  gardener::gfx::test::canvas.setPixel(4, 20, 255);
  gardener::gfx::test::display.flush();
  // Nothing drawn since: still a flush, but nothing sent and so nothing kept.
  gardener::gfx::test::display.flush();
  CHECK_EQ(display.flushCount(), 2);
  CHECK_EQ(display.frameCount(), 1);
  display.endCapture();
}

TEST_CASE("a beat saved through the display is one GIF frame per send") {
  reset();
  display.beginCapture();
  gardener::gfx::test::canvas.setPixel(4, 20, 255);
  gardener::gfx::test::display.flush();
  gardener::gfx::test::canvas.setPixel(6, 20, 255);
  gardener::gfx::test::display.flush();
  REQUIRE(display.save("tools/unit/build/capture_display_beat.gif"));
  display.endCapture();

  const std::vector<uint8_t> bytes = readFile("tools/unit/build/capture_display_beat.gif");
  REQUIRE(bytes.size() > 20);
  CHECK_EQ(std::string(reinterpret_cast<const char*>(bytes.data()), 6), std::string("GIF89a"));
  CHECK_EQ(countFrameBlocks(bytes), 2);
  std::remove("tools/unit/build/capture_display_beat.gif");
}

TEST_CASE("the gesture a page takes is named under the glass on the frame it drew") {
  reset();
  GestureStrip strip;
  beginStripCapture(strip);
  display.press("ok:double");
  strip.show(display.take());
  gardener::gfx::test::canvas.setPixel(4, 20, 255);
  gardener::gfx::test::display.flush();
  REQUIRE_EQ(display.frameCount(), 1);
  const std::vector<uint8_t>* frame = display.frame(0);
  REQUIRE(frame != nullptr);
  CHECK(packedInk(*frame, PageWidth, 0, PageHeight, PageWidth, GestureStrip::Rows));  // the strip
  CHECK_FALSE(packedInk(*frame, PageWidth, 0, 0, PageWidth, GestureStrip::Rows));  // not the glass
  CHECK(packedAt(*frame, PageWidth, 4, 20));  // and what the page drew
  CHECK_EQ(display.gestures().size(), 1);
  display.endCapture();
}

TEST_CASE("the gesture is on the frame it landed on and not the next") {
  reset();
  GestureStrip strip;
  beginStripCapture(strip);
  display.press("ok:single");
  strip.show(display.take());
  gardener::gfx::test::canvas.setPixel(4, 20, 255);
  gardener::gfx::test::display.flush();
  gardener::gfx::test::canvas.setPixel(6, 20, 255);
  gardener::gfx::test::display.flush();
  REQUIRE_EQ(display.frameCount(), 2);
  const std::vector<uint8_t>* first = display.frame(0);
  const std::vector<uint8_t>* second = display.frame(1);
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  CHECK(packedInk(*first, PageWidth, 0, PageHeight, PageWidth, GestureStrip::Rows));
  CHECK_FALSE(packedInk(*second, PageWidth, 0, PageHeight, PageWidth, GestureStrip::Rows));
  display.endCapture();
}

TEST_CASE("a gesture token renders as the label a reader sees") {
  CHECK_EQ(GestureStrip::label("ok:single"), std::string("OK"));
  CHECK_EQ(GestureStrip::label("up:double"), std::string("UP\xD7" "2"));
  CHECK_EQ(GestureStrip::label("cancel:5"), std::string("CANCEL 5s"));
  CHECK_EQ(GestureStrip::label("ok:single+up:double"), std::string("OK + UP\xD7" "2"));
  CHECK_EQ(GestureStrip::label("cmd:stop"), std::string("CMD:STOP"));
}

TEST_CASE("a component draws in the box the frame gives it") {
  reset();
  const Rect glass(6, 8, PageWidth, PageHeight);
  const int16_t frameWidth = static_cast<int16_t>(glass.x + glass.width + 20);
  const int16_t frameHeight = static_cast<int16_t>(glass.y + glass.height + 3);
  std::vector<CaptureDisplay::Component> components;
  // Beside the glass on the left: a write past its box's edge is clipped, not painted over its
  // neighbour.
  components.push_back(
      {Rect(0, 0, 6, PageHeight),
       callable([](Canvas& canvas) {
         canvas.setPixel(5, 3, 255);
         canvas.setPixel(6, 3, 255);
       })});
  // And one on the right, which knows nothing of where the other is.
  components.push_back({Rect(static_cast<int16_t>(glass.x + glass.width), 0, 20, 20),
                        callable([](Canvas& canvas) { canvas.setPixel(1, 1, 255); })});
  display.beginCapture(CaptureDisplay::Frame{frameWidth, frameHeight, glass},
                       std::move(components));
  gardener::gfx::test::canvas.setPixel(10, 10, 255);
  gardener::gfx::test::display.flush();
  REQUIRE_EQ(display.frameCount(), 1);
  const std::vector<uint8_t>* frame = display.frame(0);
  REQUIRE(frame != nullptr);
  CHECK_EQ(frame->size(), Capture::frameBytes(frameWidth, frameHeight));
  CHECK(packedAt(*frame, frameWidth, 16, 18));  // the page's (10, 10) where the glass sits
  CHECK_FALSE(packedAt(*frame, frameWidth, 10, 10));
  CHECK(packedAt(*frame, frameWidth, 5, 3));        // the left component's own origin
  CHECK_FALSE(packedAt(*frame, frameWidth, 6, 3));  // and its box's edge
  CHECK(packedAt(*frame, frameWidth, 135, 1));      // the right one's
  display.endCapture();
}

TEST_CASE("the overlay mark is hashed on every captured frame") {
  reset();
  display.beginCapture();
  display.markOverlay(40, 8);
  gardener::gfx::test::canvas.setPixel(4, 44, 255);
  gardener::gfx::test::display.flush();
  REQUIRE_EQ(display.frameCount(), 1);
  const std::vector<uint8_t>* frame = display.frame(0);
  REQUIRE(frame != nullptr);
  CHECK(packedAt(*frame, PageWidth, 0, 40));      // the outline
  CHECK(packedAt(*frame, PageWidth, 0, 44));      // the left edge
  CHECK_FALSE(packedAt(*frame, PageWidth, 1, 44));  // the spacing leaves this clear
  CHECK(packedAt(*frame, PageWidth, 4, 44));
  display.endCapture();
}

TEST_CASE("a one-bit display records the bits the canvas drew") {
  CaptureDisplay screen(64, 32, 1);
  CHECK_EQ(screen.bitsPerPixel(), 1);
  View canvas = screen.root();
  screen.beginCapture();
  canvas.setPixel(3, 3, 255);
  screen.flush();
  REQUIRE_EQ(screen.frameCount(), 1);

  const std::vector<uint8_t>* frame = screen.frame(0);
  REQUIRE(frame != nullptr);
  CHECK_EQ(frame->size(), Capture::frameBytes(64, 32));
  CHECK_EQ((*frame)[0], 0x00);
  CHECK_EQ((*frame)[3 * 8], 0x10);  // row 3, eighth byte, top bit
  CHECK_EQ(screen.at(3, 3), 255);
  CHECK_EQ(screen.at(4, 3), 0);
  screen.endCapture();
}

TEST_CASE("the display hands the page its presses in order") {
  reset();
  display.press("ok:single");
  display.press("up:double");
  CHECK_EQ(std::string(display.take()), std::string("ok:single"));
  CHECK_EQ(std::string(display.take()), std::string("up:double"));
  CHECK(display.take() == nullptr);
  CHECK_EQ(display.gestures().size(), 2);
}

namespace {

// A scene that answers one gesture and marks the pixel it drew it at, with the count kept outside
// because the stack owns the scene.
class OkScene : public Scene {
 public:
  OkScene(GestureQueue& gestures, int& handled) : Scene(gestures), handled_(handled) {}

 protected:
  bool handle(const char* gesture) override {
    if (strcmp(gesture, "ok:single") != 0) return false;
    handled_++;
    return true;
  }
  void drawPage(Canvas& canvas) override { canvas.setPixel(4, 20, 255); }

 private:
  int& handled_;
};

}  // namespace

TEST_CASE("a scene driven through the display takes its press from it") {
  reset();
  int handled = 0;
  GestureStrip strip;
  Ux ux;
  ux.reset(std::make_unique<OkScene>(display, handled));
  beginStripCapture(strip);
  display.press("ok:single");
  strip.show("ok:single");
  CHECK(ux.poll(gardener::gfx::test::canvas, 1));
  gardener::gfx::test::display.flush();
  CHECK_EQ(handled, 1);
  REQUIRE_EQ(display.frameCount(), 1);
  const std::vector<uint8_t>* frame = display.frame(0);
  REQUIRE(frame != nullptr);
  CHECK(packedAt(*frame, PageWidth, 4, 20));
  CHECK(packedInk(*frame, PageWidth, 0, PageHeight, PageWidth, GestureStrip::Rows));
  display.endCapture();
}
