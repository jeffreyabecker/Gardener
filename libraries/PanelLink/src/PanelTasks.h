#pragma once

#include <Arduino.h>

namespace gardener {

class Panel;
class ConnectedUI;
class DisconnectedUI;

// Shared scalars, one writer each. stopSeq is the panel task's counter, bumped when the Back hold
// stops a run and when control is handed between the panel and an external link; the routine's
// takeStopped() compares against it and reads it dirty.
extern volatile uint32_t stopSeq;

// goHomeSeq is the panel task's counter for Cancel pressed while the Back hold vents: the
// disconnected page watches it and hands the panel back to `default` on a bump. One writer, read
// dirty.
extern volatile uint32_t goHomeSeq;

void panelTask(void* arg);

// The panel's boot spawns the panel task, with the two screens it owns.
void spawnPanelTask(Panel& context, ConnectedUI* connected,
                    DisconnectedUI* disconnected);

}  // namespace gardener
