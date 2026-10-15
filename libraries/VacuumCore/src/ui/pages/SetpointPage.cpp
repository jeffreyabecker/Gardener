#include <stdio.h>

#include "ui/Renderer.h"
#include "ui/pages/Pages.h"

namespace eggplan {
namespace pages {

void renderSetpoint(Renderer& renderer, const SystemSnapshot& snapshot) {
  renderer.header("TARGET", snapshot.bleConnected);

  char buffer[28];
  if (snapshot.targetActive) {
    snprintf(buffer, sizeof(buffer), "%.1f", snapshot.targetKpa);
    renderer.centered(buffer, 14, 2);
  } else {
    renderer.centered("--", 14, 2);
  }

  snprintf(buffer, sizeof(buffer), "now  %.1f kPa", snapshot.kpa);
  renderer.line(2, buffer);

  if (snapshot.targetActive && snapshot.targetKpa < snapshot.floorKpa) {
    renderer.line(3, "target beyond pump");
  } else {
    snprintf(buffer, sizeof(buffer), "floor %.1f kPa", snapshot.floorKpa);
    renderer.line(3, buffer);
  }

  renderer.footer("UP/DOWN +-1  OK=hold");
}

}  // namespace pages
}  // namespace eggplan
