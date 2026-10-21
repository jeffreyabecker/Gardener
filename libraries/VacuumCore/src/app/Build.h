#pragma once

#include "app/AppContext.h"

namespace gardener {

// The composition root: builds the whole hardware graph and returns the machine and panel graphs
// as interfaces. It is the only place outside the drivers that names a fitted part or includes a
// vendor library. The AppContext is returned by value.
AppContext buildAppContext();

}  // namespace gardener
