// The child canvases: an area of another canvas, its origin, its clipping, the turn it inherits and
// the flush that sends what it drew.

#include <doctest.h>

#include <string>

#include "suite.h"

namespace gardener::gfx::test {
namespace {

using namespace gardener::gfx::ui;

TEST_CASE("a child draws at its parents offset") {
  reset();
  canvas.flush(display);
  Canvas plot = canvas.child(Rect(20, 10, 40, 20));
  CHECK_EQ(plot.width(), 40);
  CHECK_EQ(plot.height(), 20);
  plot.setPixel(0, 0, Ink);
  plot.setPixel(39, 19, Ink);
  CHECK_EQ(display.at(20, 10), Ink);
  CHECK_EQ(display.at(59, 29), Ink);
}

TEST_CASE("a child is clipped to its rect") {
  reset();
  canvas.flush(display);
  Canvas plot = canvas.child(Rect(20, 10, 40, 20));
  plot.setPixel(40, 0, Ink);
  plot.setPixel(0, 20, Ink);
  plot.setPixel(-1, 0, Ink);
  plot.setPixel(0, -1, Ink);
  CHECK_EQ(display.at(60, 10), 0);
  CHECK_EQ(display.at(20, 30), 0);
  CHECK_EQ(display.at(19, 10), 0);
  CHECK_EQ(display.at(20, 9), 0);
}

TEST_CASE("siblings do not overlap") {
  reset();
  canvas.flush(display);
  Canvas left = canvas.child(Rect(0, 40, 10, 10));
  Canvas right = canvas.child(Rect(10, 40, 10, 10));
  left.setPixel(9, 9, Ink);
  CHECK_EQ(display.at(9, 49), Ink);
  CHECK_EQ(display.at(10, 49), 0);
  right.setPixel(0, 0, Ink);
  CHECK_EQ(display.at(10, 40), Ink);
}

TEST_CASE("a child of a child nests") {
  Canvas middle = canvas.child(Rect(30, 20, 40, 20));
  Canvas inner = middle.child(Rect(5, 5, 10, 10));
  CHECK_EQ(inner.width(), 10);
  reset();
  inner.setPixel(0, 0, Ink);
  CHECK_EQ(display.at(35, 25), Ink);
}

TEST_CASE("a child of a turned parent follows the turn") {
  reset();
  Canvas turned = display.rootCanvas();
  turned.setRotation(90);
  Canvas quarter = turned.child(Rect(10, 5, 30, 20));
  CHECK_EQ(quarter.width(), 30);
  CHECK_EQ(quarter.height(), 20);
  quarter.setPixel(0, 0, Ink);
  quarter.setPixel(29, 19, Ink);
  // A turned page's (x, y) is the panel's (width - 1 - y, x).
  CHECK_EQ(display.at(122, 10), Ink);
  CHECK_EQ(display.at(103, 39), Ink);
}

TEST_CASE("a child cannot be turned") {
  reset();
  Canvas plot = canvas.child(Rect(0, 0, 40, 20));
  plot.setRotation(90);
  CHECK_EQ(plot.rotation(), 0);
  canvas.setRotation(0);
}

TEST_CASE("a flush sends what a child drew") {
  canvas.flush(display);
  reset();
  Canvas plot = canvas.child(Rect(50, 30, 10, 10));
  plot.setPixel(0, 0, Ink);
  canvas.flush(display);
  CHECK_EQ(display.flushCount(), 1);
  const Rect dirty = display.lastDirty();
  CHECK_EQ(dirty.x, 50);
  CHECK_EQ(dirty.y, 30);
  CHECK_EQ(dirty.width, 1);
  CHECK_EQ(dirty.height, 1);
  canvas.flush(display);
  CHECK_EQ(display.lastDirty().width, 0);
}

TEST_CASE("a child outside its parent draws nothing") {
  Canvas plot = canvas.child(Rect(PageWidth + 10, PageHeight + 10, 10, 10));
  CHECK_EQ(plot.width(), 0);
  reset();
  plot.setPixel(0, 0, Ink);
  CHECK_EQ(display.inkCount(0, 0, PageWidth, PageHeight), 0);
}

}  // namespace

}  // namespace gardener::gfx::test
