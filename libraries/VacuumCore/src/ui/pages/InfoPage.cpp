#include <stdio.h>

#include "core/Config.h"
#include "ui/Renderer.h"
#include "ui/pages/Pages.h"

namespace eggplan {
namespace pages {

void renderInfo(Renderer& renderer, const SystemSnapshot& snapshot) {
  renderer.header("INFO", snapshot.bleConnected);

  char buffer[30];
  snprintf(buffer, sizeof(buffer), "fw %s", cfg::FIRMWARE_VERSION);
  renderer.line(0, buffer);

  snprintf(buffer, sizeof(buffer), "chip %s", snapshot.device ? snapshot.device->chip : "-");
  renderer.line(1, buffer);

  snprintf(buffer, sizeof(buffer), "vm %s", snapshot.runtime);
  renderer.line(2, buffer);

  snprintf(buffer, sizeof(buffer), "progs %u  up %lus", (unsigned)snapshot.programCount,
           (unsigned long)(snapshot.uptimeMs / 1000));
  renderer.line(3, buffer);

  snprintf(buffer, sizeof(buffer), "heap %ukB", (unsigned)(snapshot.device ? snapshot.device->freeHeap / 1024 : 0));
  renderer.line(4, buffer);

  renderer.footer("OK=next page");
}

}  // namespace pages
}  // namespace eggplan
