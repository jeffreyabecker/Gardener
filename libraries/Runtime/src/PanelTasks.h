#pragma once

#include <Arduino.h>

namespace gardener {

class Panel;
class HomeUI;

void panelTask(void* arg);

// The panel's boot spawns the panel task, with the one page it shows.
void spawnPanelTask(Panel& context, HomeUI* home);

}  // namespace gardener
