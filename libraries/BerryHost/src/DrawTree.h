#pragma once

#include <stdint.h>

#include "gfx/Canvas.h"

// The interpreter's VM, forward declared so its header stops here.
struct bvm;

namespace gardener {

class PanelContext;

// A page's drawing, walked: the node at `node` of the interpreter's stack turned into the paint it
// describes and drawn on `canvas`. The tree is what the `draw` record the screen module declares
// describes, and what that module builds one node at a time; a node that is not one of those, or a
// kind the drawing does not know, draws nothing.
//
// The strings a node carries belong to the interpreter and the paints capture them, so a tree is
// built and drawn in the same call and never kept across a beat. Nothing here raises: the walk runs
// after the page's own call has returned, where a raise would unwind through C++ frames.
void drawTree(bvm* vm, int node, PanelContext& panel, gfx::Canvas& canvas);

}  // namespace gardener
