#pragma once

#include <Arduino.h>

namespace gardener {

namespace panel {
class Panel;
}  // namespace panel

void panelTask(void* arg);

// The panel's boot spawns the panel task, which drives the one page loop the panel shows.
void spawnPanelTask(panel::Panel& context);

}  // namespace gardener
