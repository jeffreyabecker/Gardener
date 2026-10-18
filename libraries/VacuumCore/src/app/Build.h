#pragma once

namespace gardener {

class AppContext;

// The composition root: builds the whole hardware graph and returns it as interfaces. It is the
// only place outside the drivers that names a fitted part or includes a vendor library.
AppContext* buildAppContext();

}  // namespace gardener
