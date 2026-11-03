#pragma once

#include <stdint.h>

#include "Canvas.h"
#include "layout/Paint.h"

// The interpreter's VM, forward declared so its header stops here.
struct bvm;

namespace gardener {

struct SystemState;

// A page's drawing, walked: the node at `node` of the interpreter's stack turned into the paint it
// describes and drawn on `canvas`. The tree is what the `draw` record the screen module declares
// describes, and what that module builds one node at a time; a node that is not one of those, or a
// kind the drawing does not know, draws nothing.
//
// A node that borrows its text points into the interpreter's own string, so a tree is built and
// drawn in the same call and never kept across a beat. The state is the copy the caller took for
// this drawing, so every node reads one instant. Nothing here raises: the walk runs after the page's
// own call has returned, where a raise would unwind through C++ frames.
void drawTree(bvm* vm, int node, SystemState state, gfx::Canvas& canvas);

// The paint the node at `node` describes, for a caller that keeps it: what a row list holds as a
// row's picture. It is `drawTree()`'s other half, so the walk that turns one pair's draw into a
// paint stays the page's own. The state is the copy the caller took when it asked for the tree. A
// paint's borrowed text points into the node it was walked from, so whoever keeps the paint keeps
// the node.
gfx::PaintNodeRef paintFromTree(bvm* vm, int node, SystemState state);

}  // namespace gardener