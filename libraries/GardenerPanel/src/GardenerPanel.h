#pragma once

// Umbrella header for the Gardener panel library. The full device's sketch includes this one.
//
// VacuumCore comes first: arduino-cli finds a library by the umbrella header a sketch or library
// names, so naming it here is what puts VacuumCore's headers on the include path for the panel's
// own files.

#include "VacuumCore.h"

#include "app/App.h"
#include "app/AppContext.h"
#include "app/PanelRoot.h"
