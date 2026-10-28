#pragma once

namespace gardener {

// RoutineRegistry's own cases, over an in-memory image loaded from `dataDir`: printed as a suite's
// cases are, and answering whether every one passed.
bool runRoutineRegistryCases(const char* dataDir);

}  // namespace gardener
