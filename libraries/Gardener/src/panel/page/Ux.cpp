#include "panel/page/Ux.h"

#include "Layout.h"
#include "Shapes.h"

namespace gardener::panel::page {

bool Ux::reset(std::unique_ptr<Scene> root) {
  while (depth_ > 0) {
    depth_--;
    if (frames_[depth_]) frames_[depth_]->leave();
    frames_[depth_] = nullptr;
    callbacks_[depth_] = nullptr;
  }
  pending_ = Request::None;
  pendingScene_ = nullptr;
  pendingCallback_ = nullptr;
  if (!root) return false;
  place(std::move(root), nullptr);
  return depth_ > 0;
}

bool Ux::push(std::unique_ptr<Scene> scene, Callback callback) {
  if (!scene) return false;
  if (polling_) {
    pending_ = Request::Push;
    pendingScene_ = std::move(scene);
    pendingCallback_ = callback;
    return true;
  }
  return place(std::move(scene), callback);
}

bool Ux::pop() {
  if (polling_) {
    pending_ = Request::Pop;
    return true;
  }
  return take();
}

bool Ux::popToRoot() {
  if (polling_) {
    pending_ = Request::ToRoot;
    return true;
  }
  return home();
}

Scene* Ux::top() const { return depth_ > 0 ? frames_[depth_ - 1].get() : nullptr; }

int16_t Ux::depth() const { return depth_; }

void Ux::setBand(const std::vector<gfx::Paint>& cells) { band_ = cells; }

bool Ux::enter(gfx::Canvas& canvas, uint32_t nowMs) {
  Scene* scene = top();
  if (scene == nullptr) return false;
  beginBeat(canvas, nowMs);
  const bool held = polling_;
  polling_ = true;
  scene->activate();
  paintScene(top());
  polling_ = held;
  apply();
  endBeat();
  return depth_ > 0;
}

bool Ux::poll(gfx::Canvas& canvas, uint32_t nowMs) {
  Scene* scene = top();
  if (scene == nullptr) return false;
  beginBeat(canvas, nowMs);
  const bool held = polling_;
  polling_ = true;
  gfx::Canvas own = page();
  scene->poll(own, nowMs);
  polling_ = held;
  apply();
  endBeat();
  return depth_ > 0;
}

void Ux::beginBeat(gfx::Canvas& canvas, uint32_t nowMs) {
  beat_ = &canvas;
  beatMs_ = nowMs;
}

void Ux::endBeat() {
  gfx::Canvas* canvas = beat_;
  beat_ = nullptr;
  if (canvas == nullptr) return;
  if (depth_ > 0) paintBand(*canvas);
}

// The canvas the scenes draw in: the beat's own less the rows the band takes, so a page never draws
// under the band and never has to know how many rows it gives up.
gfx::Canvas Ux::page() {
  const int16_t height = static_cast<int16_t>(beat_->height() - bandRows());
  return beat_->child(gfx::Rect(0, 0, beat_->width(), height < 0 ? 0 : height));
}

// A scene that went on top paints at once rather than on its own repaint cadence, which is what
// keeps the page that popped over from keeping the pixels of the page that left.
void Ux::paintScene(Scene* scene) {
  if (scene == nullptr || beat_ == nullptr) return;
  gfx::Canvas own = page();
  scene->draw(own, beatMs_);
}

// The band after the page, over the bottom rows of the glass: the cells are laid out in equal shares
// of the width, left to right, each in a canvas of its own so what it paints is clipped to its cell.
// The band is opaque — it clears its rows first — because it lies over the bottom of whatever page
// is showing, and a cell given nothing draws nothing.
void Ux::paintBand(gfx::Canvas& canvas) {
  const int16_t rows = bandRows();
  if (rows <= 0) return;
  const int16_t top = static_cast<int16_t>(canvas.height() - rows);
  if (top < 0) return;
  gfx::Canvas band = canvas.child(gfx::Rect(0, top, canvas.width(), rows));
  band.draw(gfx::shapes::FillRect(0, 0, band.width(), band.height(), gfx::Paper));

  const float share = 1.0f / static_cast<float>(band_.size());
  std::vector<gfx::Child> cells;
  cells.reserve(band_.size());
  for (const gfx::Paint& cell : band_) {
    cells.push_back(gfx::Child{gfx::Cell(share, gfx::Cell::Whole), cell});
  }
  gfx::row(1.0f, cells)(band);
}

bool Ux::place(std::unique_ptr<Scene> scene, const Callback& callback) {
  if (!scene || depth_ >= MaxFrames) return false;
  Scene* showing = scene.get();
  frames_[depth_] = std::move(scene);
  callbacks_[depth_] = callback;
  depth_++;
  const bool held = polling_;
  polling_ = true;
  showing->activate();
  polling_ = held;
  paintScene(showing);
  return true;
}

// The scene that pops is held here until its callback has run, and goes when this returns.
bool Ux::take() {
  if (depth_ <= 1) return false;
  std::unique_ptr<Scene> leaving = std::move(frames_[depth_ - 1]);
  const Callback callback = callbacks_[depth_ - 1];
  frames_[depth_ - 1] = nullptr;
  callbacks_[depth_ - 1] = nullptr;
  depth_--;
  leaving->leave();
  Scene* below = top();
  const bool held = polling_;
  polling_ = true;
  if (callback) {
    callback(*leaving);
  } else if (below != nullptr) {
    below->activate();
  }
  polling_ = held;
  paintScene(top());
  return true;
}

bool Ux::home() {
  // Nothing showing is nothing to go home to, which is the false a caller reads as no page.
  if (depth_ <= 0) return false;
  if (depth_ == 1) return true;
  while (depth_ > 1) {
    depth_--;
    if (frames_[depth_]) frames_[depth_]->leave();
    frames_[depth_] = nullptr;
    callbacks_[depth_] = nullptr;
  }
  Scene* scene = top();
  if (scene == nullptr) return false;
  const bool held = polling_;
  polling_ = true;
  scene->activate();
  polling_ = held;
  paintScene(scene);
  return depth_ > 0;
}

void Ux::apply() {
  const Request request = pending_;
  if (request == Request::None) return;
  std::unique_ptr<Scene> scene = std::move(pendingScene_);
  const Callback callback = pendingCallback_;
  pending_ = Request::None;
  pendingScene_ = nullptr;
  pendingCallback_ = nullptr;
  const bool held = polling_;
  polling_ = true;
  if (request == Request::Push) {
    place(std::move(scene), callback);
  } else if (request == Request::Pop) {
    take();
  } else {
    home();
  }
  polling_ = held;
}

}  // namespace gardener::panel::page
