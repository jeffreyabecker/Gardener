#pragma once

// The Berry interpreter's host side: the install that gives a fresh VM the modules a page is
// written against, the modules and the native class themselves, the scene that drives a page's
// instance like any other entry in the panel's stack, and the VM behind the core's
// `routines/Interpreter.h`. A build includes this and nothing else.
//
// The VM is the vendored Berry beside this library, and a page's canvas is Gfx. It also compiles
// against `libraries/Gardener` for the machine's bounds and run state, the two documents and the
// buzzer, which is why the manifest states that in prose rather than in `depends`: a `depends` there
// would close a cycle with the one that names this library.
//
// `BerryInterpreter.h` is the implementation of the core's `routines/Interpreter.h`. Everything else
// declared here forward-declares `bvm`, `Routines`, `PanelContext` and `Logger`, so a translation
// unit that only wants the install need not follow them into a Gardener header.

#include "BerryInterpreter.h"
#include "Bindings.h"
#include "BerryScene.h"
#include "Module.h"
