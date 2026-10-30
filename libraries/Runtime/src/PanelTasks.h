#pragma once

#include <Arduino.h>

namespace gardener {

namespace panel {
class Panel;
}  // namespace panel

struct SystemState;

void panelTask(void* arg);

// The panel's boot spawns the panel task, which drives the one page loop the panel shows. `state` is
// the object the parts beat publishes the pack into and the page beat reads.
void spawnPanelTask(panel::Panel& context, SystemState& state);

}  // namespace gardener
