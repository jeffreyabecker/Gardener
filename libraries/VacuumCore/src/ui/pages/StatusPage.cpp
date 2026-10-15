#include <stdio.h>

#include "ui/Renderer.h"
#include "ui/pages/Pages.h"

namespace eggplan {
namespace pages {

void renderStatus(Renderer& renderer, const SystemSnapshot& snapshot) {
  renderer.header("EGGPLAN VACUUM", snapshot.bleConnected);

  char buffer[28];
  if (snapshot.sensorOk) {
    snprintf(buffer, sizeof(buffer), "%.1f", snapshot.kpa);
    renderer.centered(buffer, 12, 2);
  } else {
    renderer.centered("NO SENSOR", 16, 1);
  }

  snprintf(buffer, sizeof(buffer), "vac %.1f kPa  %.0f%%", snapshot.vacuumKpa,
           snapshot.vacuumPercent);
  renderer.line(3, buffer);

  renderer.bar(0, 42, renderer.width(), 7, snapshot.duty / 100.0f);

  if (snapshot.targetActive) {
    snprintf(buffer, sizeof(buffer), "%s %.1f  %.0f%%", controlStateName(snapshot.state),
             snapshot.targetKpa, snapshot.duty);
  } else {
    snprintf(buffer, sizeof(buffer), "%s  %.0f%%", controlStateName(snapshot.state),
             snapshot.duty);
  }
  renderer.line(5, buffer);

  if (snapshot.scriptRunning) {
    snprintf(buffer, sizeof(buffer), "run %s", snapshot.program);
  } else if (snapshot.autostart[0] != '\0') {
    snprintf(buffer, sizeof(buffer), "auto %s", snapshot.autostart);
  } else {
    snprintf(buffer, sizeof(buffer), "no autostart");
  }
  renderer.line(6, buffer);
}

}  // namespace pages
}  // namespace eggplan
