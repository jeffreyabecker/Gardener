// The child canvases: an area of another canvas, its origin, its clipping, the turn it inherits and
// the flush that sends what it drew.

#include "suite.h"

namespace gardener::gfx::test {
namespace {

using namespace gardener::gfx::ui;

void aChildDrawsAtItsParentsOffset() {
  reset();
  canvas.flush(display);
  Canvas plot = canvas.child(Rect(20, 10, 40, 20));
  expectEq(plot.width(), 40, "the child is as wide as the area");
  expectEq(plot.height(), 20, "the child is as tall as the area");
  plot.setPixel(0, 0, Ink);
  plot.setPixel(39, 19, Ink);
  expectEq(display.at(20, 10), Ink, "the child's origin is the area's corner");
  expectEq(display.at(59, 29), Ink, "the child's far corner is the area's");
}

void aChildIsClippedToItsRect() {
  reset();
  canvas.flush(display);
  Canvas plot = canvas.child(Rect(20, 10, 40, 20));
  plot.setPixel(40, 0, Ink);
  plot.setPixel(0, 20, Ink);
  plot.setPixel(-1, 0, Ink);
  plot.setPixel(0, -1, Ink);
  expectEq(display.at(60, 10), 0, "a write past the right edge is dropped");
  expectEq(display.at(20, 30), 0, "a write past the bottom edge is dropped");
  expectEq(display.at(19, 10), 0, "a write before the left edge is dropped");
  expectEq(display.at(20, 9), 0, "a write above the top edge is dropped");
}

void siblingsDoNotOverlap() {
  reset();
  canvas.flush(display);
  Canvas left = canvas.child(Rect(0, 40, 10, 10));
  Canvas right = canvas.child(Rect(10, 40, 10, 10));
  left.setPixel(9, 9, Ink);
  expectEq(display.at(9, 49), Ink, "the first child drew at its far corner");
  expectEq(display.at(10, 49), 0, "and nothing landed in the second child's area");
  right.setPixel(0, 0, Ink);
  expectEq(display.at(10, 40), Ink, "the second child starts where its own area does");
}

void aChildOfAChildNests() {
  Canvas middle = canvas.child(Rect(30, 20, 40, 20));
  Canvas inner = middle.child(Rect(5, 5, 10, 10));
  expectEq(inner.width(), 10, "the inner canvas is its own area's width");
  reset();
  inner.setPixel(0, 0, Ink);
  expectEq(display.at(35, 25), Ink, "the inner origin is both areas' corners added up");
}

void aChildOfATurnedParentFollowsTheTurn() {
  reset();
  Canvas turned = display.rootCanvas();
  turned.setRotation(90);
  Canvas quarter = turned.child(Rect(10, 5, 30, 20));
  expectEq(quarter.width(), 30, "the child is the area's own width, as the parent counts it");
  expectEq(quarter.height(), 20, "and the area's own height");
  quarter.setPixel(0, 0, Ink);
  quarter.setPixel(29, 19, Ink);
  // A turned page's (x, y) is the panel's (width - 1 - y, x).
  expectEq(display.at(122, 10), Ink, "the child's origin is where the parent's would be");
  expectEq(display.at(103, 39), Ink, "and so is its far corner");
}

void aChildCannotBeTurned() {
  reset();
  Canvas plot = canvas.child(Rect(0, 0, 40, 20));
  plot.setRotation(90);
  expectEq(plot.rotation(), 0, "a child keeps the turn it inherited");
  canvas.setRotation(0);
}

void aFlushSendsWhatAChildDrew() {
  canvas.flush(display);
  reset();
  Canvas plot = canvas.child(Rect(50, 30, 10, 10));
  plot.setPixel(0, 0, Ink);
  canvas.flush(display);
  expectEq(display.flushCount(), 1, "one flush on the frame's canvas");
  const Rect dirty = display.lastDirty();
  expectEq(dirty.x, 50, "the flush covers the child's pixel");
  expectEq(dirty.y, 30, "the flush covers the child's pixel");
  expectEq(dirty.width, 1, "and nothing more than it");
  expectEq(dirty.height, 1, "and nothing more than it");
  canvas.flush(display);
  expectEq(display.lastDirty().width, 0, "a second flush has nothing to send");
}

void aChildOutsideItsParentDrawsNothing() {
  Canvas plot = canvas.child(Rect(PageWidth + 10, PageHeight + 10, 10, 10));
  expectEq(plot.width(), 0, "an area outside the canvas clamps to nothing");
  reset();
  plot.setPixel(0, 0, Ink);
  expectEq(display.inkCount(0, 0, PageWidth, PageHeight), 0, "and writes nothing");
}

}  // namespace

void addCanvasCases(std::vector<Case>& cases) {
  cases.push_back({"a child draws at its parent's offset", aChildDrawsAtItsParentsOffset});
  cases.push_back({"a child is clipped to its rect", aChildIsClippedToItsRect});
  cases.push_back({"siblings do not overlap", siblingsDoNotOverlap});
  cases.push_back({"a child of a child nests", aChildOfAChildNests});
  cases.push_back(
      {"a child of a turned parent follows the turn", aChildOfATurnedParentFollowsTheTurn});
  cases.push_back({"a child cannot be turned", aChildCannotBeTurned});
  cases.push_back({"a flush sends what a child drew", aFlushSendsWhatAChildDrew});
  cases.push_back({"a child outside its parent draws nothing", aChildOutsideItsParentDrawsNothing});
}

}  // namespace gardener::gfx::test
