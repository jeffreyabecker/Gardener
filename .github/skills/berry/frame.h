#pragma once

#include "berry.h"

// Register the frame: the host's one-bit panel, so a page's drawing verbs land in pixels the check
// can save as an image. Called once, before the data image loads.
void frameRegister(bvm* vm);
