#pragma once

#include <Arduino.h>

namespace gardener {

class Panel;

void panelTask(void* arg);

// The panel's boot spawns the panel task, which drives the one page loop the panel shows.
void spawnPanelTask(Panel& context);

}  // namespace gardener
