#include <stdio.h>

#include "ui/Renderer.h"
#include "ui/pages/Pages.h"

namespace eggplan {
namespace pages {
namespace {
constexpr int16_t kTop = 12;
constexpr int16_t kHeight = 42;
}

void renderGraph(Renderer& renderer, const SystemSnapshot& snapshot) {
  char buffer[28];
  snprintf(buffer, sizeof(buffer), "PRESSURE %.1f kPa", snapshot.kpa);
  renderer.header(buffer, snapshot.bleConnected);

  const float* samples = nullptr;
  uint8_t count = 0;
  uint8_t head = 0;
  renderer.sampleView(samples, count, head);

  float minimum = 1e9f;
  float maximum = -1e9f;
  for (uint8_t i = 0; i < count; i++) {
    if (samples[i] < minimum) minimum = samples[i];
    if (samples[i] > maximum) maximum = samples[i];
  }
  if (count < 2 || maximum - minimum < 1.0f) {
    minimum = snapshot.kpa - 5.0f;
    maximum = snapshot.kpa + 5.0f;
  }

  renderer.frame(0, kTop, renderer.width(), kHeight);

  if (count > 1) {
    int16_t previousY = -1;
    for (uint8_t i = 0; i < count; i++) {
      uint8_t slot = (uint8_t)((head + Renderer::historySize() - count + i) %
                               Renderer::historySize());
      float normalised = (samples[slot] - minimum) / (maximum - minimum);
      int16_t y = (int16_t)(kTop + kHeight - 2 - normalised * (kHeight - 4));
      if (previousY >= 0) renderer.plot((int16_t)i - 1, previousY, (int16_t)i, y);
      previousY = y;
    }
  }

  snprintf(buffer, sizeof(buffer), "%.1f..%.1f kPa", minimum, maximum);
  renderer.footer(buffer);
}

}  // namespace pages
}  // namespace eggplan
