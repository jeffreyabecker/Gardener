#include "panel/page/Scene.h"

#include "Shapes.h"
#include "Drawing.h"

namespace gardener::panel::page {

Scene::Scene(GestureQueue& gestures) : gestures_(gestures) {}

void Scene::draw(gfx::Canvas& canvas, uint32_t nowMs) {
  canvas.draw(gfx::shapes::FillRect(0, 0, canvas.width(), canvas.height(), gfx::Paper));
  drawPage(canvas);
  lastDraw_ = nowMs;
}

bool Scene::poll(gfx::Canvas& canvas, uint32_t nowMs) {
  const bool acted = takeEvents();
  tick(nowMs);
  if (!acted && static_cast<uint32_t>(nowMs - lastDraw_) < RepaintMs) return false;
  draw(canvas, nowMs);
  return true;
}

bool Scene::takeEvents() {
  bool acted = false;
  for (const char* gesture = gestures_.take(); gesture != nullptr; gesture = gestures_.take()) {
    if (handle(gesture)) acted = true;
  }
  return acted;
}

}  // namespace gardener::panel::page
