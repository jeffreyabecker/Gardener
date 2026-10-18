#pragma once

#include "app/AppContext.h"

namespace gardener {

// The composition root: builds the whole hardware graph and returns it as interfaces. It is the
// only place outside the drivers that names a fitted part or includes a vendor library. The
// AppContext itself is returned by value — it is seven pointers, so a copy is nothing.
AppContext buildAppContext();

}  // namespace gardener
