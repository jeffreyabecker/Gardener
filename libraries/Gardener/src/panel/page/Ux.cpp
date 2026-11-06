#include "panel/page/Ux.h"

#include <utility>

namespace gardener {

bool Ux::reset(std::unique_ptr<Scene> root) {
  while (!frames_.empty()) {
    std::unique_ptr<Scene> leaving = std::move(frames_.back().scene);
    frames_.pop_back();
    if (leaving) leaving->leave();
  }
  pending_ = Request::None;
  pendingScene_ = nullptr;
  pendingCallback_ = nullptr;
  if (!root) return false;
  place(std::move(root), nullptr);
  return !frames_.empty();
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

Scene* Ux::top() const { return frames_.empty() ? nullptr : frames_.back().scene.get(); }

int16_t Ux::depth() const { return static_cast<int16_t>(frames_.size()); }

void Ux::setOverlay(int16_t startY, gfx::PaintNodeRef overlay) {
  overlayStart_ = startY < 0 ? 0 : startY;
  overlay_ = std::move(overlay);
}

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
  return !frames_.empty();
}

bool Ux::poll(gfx::Canvas& canvas, uint32_t nowMs) {
  Scene* scene = top();
  if (scene == nullptr) return false;
  beginBeat(canvas, nowMs);
  const bool held = polling_;
  polling_ = true;
  gfx::View own = page();
  scene->poll(own, nowMs);
  polling_ = held;
  apply();
  endBeat();
  return !frames_.empty();
}

void Ux::beginBeat(gfx::Canvas& canvas, uint32_t nowMs) {
  beat_ = &canvas;
  beatMs_ = nowMs;
}

void Ux::endBeat() {
  gfx::Canvas* canvas = beat_;
  beat_ = nullptr;
  if (canvas == nullptr) return;
  if (!frames_.empty()) paintOverlay(*canvas);
}

// The canvas the scenes draw in: the beat's own less the rows the overlay takes, so a page never
// draws under the overlay and never has to know where it starts.
gfx::View Ux::page() {
  const int16_t bottom = overlay_ ? overlayStart_ : beat_->height();
  const int16_t height =
      bottom > beat_->height() ? beat_->height() : static_cast<int16_t>(bottom < 0 ? 0 : bottom);
  return beat_->child(gfx::Rect(0, 0, beat_->width(), height));
}

// A scene that went on top paints at once rather than on its own repaint cadence, which is what
// keeps the page that popped over from keeping the pixels of the page that left.
void Ux::paintScene(Scene* scene) {
  if (scene == nullptr || beat_ == nullptr) return;
  gfx::View own = page();
  scene->draw(own, beatMs_);
}

// The overlay after the page, over the rows from where it starts to the bottom of the glass. What
// it draws is the caller's business: it is handed a canvas over those rows and nothing narrower.
void Ux::paintOverlay(gfx::Canvas& canvas) {
  if (!overlay_) return;
  int16_t top = overlayStart_;
  if (top > canvas.height()) top = canvas.height();
  const int16_t rows = static_cast<int16_t>(canvas.height() - top);
  if (rows <= 0) return;
  gfx::View own = canvas.child(gfx::Rect(0, top, canvas.width(), rows));
  overlay_->draw(own);
}

bool Ux::place(std::unique_ptr<Scene> scene, const Callback& callback) {
  if (!scene || frames_.size() >= static_cast<size_t>(MaxFrames)) return false;
  Scene* showing = scene.get();
  frames_.push_back({std::move(scene), callback});
  const bool held = polling_;
  polling_ = true;
  showing->activate();
  polling_ = held;
  paintScene(showing);
  return true;
}

// The scene that pops is held here until its callback has run, and goes when this returns.
bool Ux::take() {
  if (frames_.size() <= 1) return false;
  std::unique_ptr<Scene> leaving = std::move(frames_.back().scene);
  const Callback callback = frames_.back().callback;
  frames_.pop_back();
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
  if (frames_.empty()) return false;
  if (frames_.size() == 1) return true;
  while (frames_.size() > 1) {
    std::unique_ptr<Scene> leaving = std::move(frames_.back().scene);
    frames_.pop_back();
    if (leaving) leaving->leave();
  }
  Scene* scene = top();
  if (scene == nullptr) return false;
  const bool held = polling_;
  polling_ = true;
  scene->activate();
  polling_ = held;
  paintScene(scene);
  return !frames_.empty();
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

}  // namespace gardener