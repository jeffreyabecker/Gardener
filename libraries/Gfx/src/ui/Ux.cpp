#include "ui/Ux.h"

namespace gardener::gfx::ui {

bool Ux::reset(Scene* root) {
  while (depth_ > 0) {
    depth_--;
    Scene* leaving = frames_[depth_];
    frames_[depth_] = nullptr;
    callbacks_[depth_] = nullptr;
    leaving->leave();
  }
  pending_ = Request::None;
  pendingScene_ = nullptr;
  pendingCallback_ = nullptr;
  if (root == nullptr) return false;
  place(*root, nullptr);
  return depth_ > 0;
}

bool Ux::push(Scene& scene, Callback callback) {
  if (polling_) {
    pending_ = Request::Push;
    pendingScene_ = &scene;
    pendingCallback_ = callback;
    return true;
  }
  return place(scene, callback);
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

Scene* Ux::top() const { return depth_ > 0 ? frames_[depth_ - 1] : nullptr; }

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

bool Ux::place(Scene& scene, const Callback& callback) {
  if (depth_ >= MaxFrames) return false;
  frames_[depth_] = &scene;
  callbacks_[depth_] = callback;
  depth_++;
  const bool held = polling_;
  polling_ = true;
  scene.activate();
  polling_ = held;
  return true;
}

bool Ux::take() {
  if (depth_ <= 1) return false;
  Scene* leaving = frames_[depth_ - 1];
  const Callback callback = callbacks_[depth_ - 1];
  depth_--;
  frames_[depth_] = nullptr;
  callbacks_[depth_] = nullptr;
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
    Scene* leaving = frames_[depth_];
    frames_[depth_] = nullptr;
    callbacks_[depth_] = nullptr;
    leaving->leave();
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
  Scene* scene = pendingScene_;
  const Callback callback = pendingCallback_;
  pending_ = Request::None;
  pendingScene_ = nullptr;
  pendingCallback_ = nullptr;
  const bool held = polling_;
  polling_ = true;
  if (request == Request::Push) {
    if (scene != nullptr) place(*scene, callback);
  } else if (request == Request::Pop) {
    take();
  } else {
    home();
  }
  polling_ = held;
}

}  // namespace gardener::gfx::ui
