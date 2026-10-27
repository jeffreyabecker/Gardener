#include "ui/Ux.h"

namespace gardener::gfx::ui {

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

bool Ux::enter() {
  Scene* scene = top();
  if (scene == nullptr) return false;
  const bool held = polling_;
  polling_ = true;
  scene->activate();
  polling_ = held;
  apply();
  return depth_ > 0;
}

bool Ux::poll(Canvas& canvas, uint32_t nowMs) {
  Scene* scene = top();
  if (scene == nullptr) return false;
  const bool held = polling_;
  polling_ = true;
  scene->poll(canvas, nowMs);
  polling_ = held;
  apply();
  return depth_ > 0;
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
  return true;
}

bool Ux::home() {
  if (depth_ <= 1) return true;
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

}  // namespace gardener::gfx::ui
