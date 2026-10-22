#include "app/PanelBoot.h"

#include <Wire.h>
#include <stdlib.h>

#include "app/Logger.h"
#include "app/panel/Asset.h"
#include "core/Config.h"
#include "core/Units.h"

namespace gardener {

// The panel's side of bring-up: the panel comes up — the display first, which is fatal when it
// does not answer — the gauge and buzzer follow, and the interpreter starts. The panel's wire
// domains, its screens and its task belong to the composition that calls this.
void panelBoot(PanelContext& context, Logger& logger, Storage::Mount mount) {
  // The panel is the one part with no fallback: nothing on the device can draw
  // without it, so a panel that does not answer is fatal rather than a degraded
  // mode.
  Wire1.begin(cfg::PANEL_I2C_SDA, cfg::PANEL_I2C_SCL, cfg::PANEL_I2C_FREQ_HZ);
  const bool displayOk = context.display().begin();
  logger.info(displayOk ? "oled: online" : "oled: not found, aborting");
  if (!displayOk) {
    // abort() panics and the ESP32 resets on a panic, so the device keeps
    // trying rather than coming up blind and silent.
    delay(50);
    abort();
  }

  const bool batteryOk = context.battery().begin();
  const bool buzzerOk = context.buzzer().begin();

  // The mark is an image on the device rather than an array in a header. Every page draws it from
  // there when it needs it, so a data image without it is worth one line and nothing else — the pages
  // simply have no mark.
  if (context.storage().size(kMarkAsset) == 0) {
    logger.warn("ui: /assets/eggplant.rle missing, the disconnected page has no mark");
  }

  // The interpreter comes up here and stays: routines are loaded into it later, and a device with
  // no routines simply never enters it.
  const bool vmOk = context.routines().begin(context, context.storage(), logger,
                                             context.storeClient());

  // What came up, and what did not. The devices report as values and one place decides what is
  // worth saying.
  logger.info(buzzerOk ? "buzzer: ready" : "buzzer: PWM unavailable");
  logger.info(batteryOk ? "batt: gauge ready" : "batt: no sense pin, gauge off");
  logger.info("buttons: four keys on interrupt");

  switch (mount) {
    case Storage::Mount::Mounted:
      logger.info("fs: littlefs mounted");
      break;
    case Storage::Mount::Formatted:
      logger.info("fs: littlefs formatted, no filesystem found");
      break;
    case Storage::Mount::Failed:
      logger.warn("fs: littlefs mount failed, settings will not persist");
      break;
  }
  if (mount != Storage::Mount::Failed) {
    String usage = "fs: ";
    usage += String((uint32_t)context.storage().usedBytes());
    usage += " of ";
    usage += String((uint32_t)context.storage().totalBytes());
    usage += " bytes used";
    logger.info(usage.c_str());
  }
  String stored = "settings: unit=";
  stored += displayUnitInfo(context.settings().unit()).name;
  logger.info(stored.c_str());

  String vm = vmOk ? "vm: ready, heap " : "vm: no memory, routines off, heap ";
  vm += String((uint32_t)ESP.getFreeHeap());
  logger.info(vm.c_str());
}

}  // namespace gardener
