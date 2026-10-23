#pragma once

#include <Arduino.h>

namespace gardener {

class Panel;
class HomeUI;

// The panel task's counter for the force-quit: Cancel held 5 s stops and vents, and the page watches
// this and hands the panel back to `default` on a bump. One writer, read dirty.
extern volatile uint32_t goHomeSeq;

void panelTask(void* arg);

// The panel's boot spawns the panel task, with the one page it shows.
void spawnPanelTask(Panel& context, HomeUI* home);

}  // namespace gardener
