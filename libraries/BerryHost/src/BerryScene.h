#pragma once

#include <stdint.h>

#include "panel/page/Scene.h"

// The interpreter's VM, forward declared so its header stops here.
struct bvm;

namespace gardener {

namespace panel {
class PanelContext;
}  // namespace panel

// A page whose body is Berry: a scene whose activate, handle, tick and draw_page are methods of an
// instance the interpreter holds. The stack drives it like any other scene, so a page can move to
// C++ a hook at a time and one that is still Berry draws and answers keys as it did.
//
// The class is the boundary and nothing else. The gesture drain, the repaint cadence and the paper
// clear are Scene's; what is here is the lookup of a method by name, the call made so a raise is
// caught, and the answer pushed back.
//
// A method the page does not define is a hook it left out, as it is on Scene: activate, tick and
// draw_page do nothing and handle answers false.
//
// The collector marks globals, the interpreter's own stack and fixed objects, and a value a C++
// object holds is none of those. The scene therefore roots the page in a list the interpreter owns
// for as long as it lives, and lets it go when the scene does.
//
// The page answers its drawing rather than making it: draw_page returns the tree the screen module
// built node by node, and the scene renders it on the canvas it was handed — the page, which is the
// panel less the band's rows. A page that answers nothing draws nothing.
class BerryScene : public panel::page::Scene {
 public:
  // `page` is the instance at index `page` of the interpreter's stack, and the scene is what keeps
  // it; the caller's stack is left as it was found. The context is the device the page draws about,
  // which is what a pressure readout in its tree needs.
  BerryScene(panel::page::GestureQueue& gestures, bvm* vm, int page, panel::PanelContext& panel);
  ~BerryScene() override;

  BerryScene(const BerryScene&) = delete;
  BerryScene& operator=(const BerryScene&) = delete;

  // The page went on top: its own chance to read what it shows, before the stack paints it.
  void activate() override;

  // The page this scene holds, on the interpreter's stack: the instance the image built, so the stack
  // can answer which page is showing. False, pushing nothing, when the scene holds no page.
  bool pushPage(bvm* vm) const;

  // This scene, which is how ux.top() reaches the page it holds.
  const void* scripted() const override { return this; }

 protected:
  bool handle(const char* gesture) override;
  void tick(uint32_t nowMs) override;
  void drawPage(gfx::Canvas& canvas) override;

 private:
  // The page and its `method`, with the page as self, ready for the values a caller pushes next.
  // False when the scene is holding no page, when the page does not define the method, or when it
  // has raised once already.
  bool beginCall(const char* method);

  // Ends the call beginCall() made, with the number of values pushed after it. False for a raise,
  // which is reported and takes the scene out of service: every beat would raise the same way.
  bool endCall(const char* method, int argc);

  bvm* vm_ = nullptr;
  // The device the page draws about, held for a pressure readout in the tree it answers.
  panel::PanelContext& panel_;
  // Where the page sits in the interpreter's list, or -1 when the scene is holding nothing.
  int slot_ = -1;
  bool broken_ = false;
};

}  // namespace gardener
