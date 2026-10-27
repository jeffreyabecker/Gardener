#pragma once

// What every check file shares: the frame the cases draw into, the expectations they assert with,
// and the one case type the run collects. A file contributes its cases by name, in the order they
// run, and main.cpp is the only thing that runs them.

#include <stdint.h>

#include <vector>

#include <Gfx.h>

#include "host/StubDisplay.h"

namespace gardener::gfx::test {

constexpr int16_t PageWidth = 128;
constexpr int16_t PageHeight = 64;

// One case of the run: what it is called and what it asserts.
struct Case {
  const char* name;
  void (*run)();
};

// The page-sized stub and the canvas over the whole of it.
extern StubDisplay display;
extern Canvas canvas;

// Clears the frame the last case left and the flush count with it.
void reset();

// A condition, a number and a string the case asserts; a failure records the first one and counts.
void expect(bool condition, const char* what);
void expectEq(long actual, long expected, const char* what);
void expectText(const char* actual, const char* expected, const char* what);

// Runs one case and prints its line, answering whether it passed.
bool runCase(const Case& item);

// The queue a scene drains when nothing is pressed: a case drives a scene through the hooks it
// answers rather than through the keys.
class NoGestures : public ui::GestureQueue {
 public:
  const char* take() override { return nullptr; }
};

// The cases each file holds, appended in the order that file was asked for.
void addDrawingCases(std::vector<Case>& cases);
void addLayoutCases(std::vector<Case>& cases);
void addEditorCases(std::vector<Case>& cases);
void addOverlayCases(std::vector<Case>& cases);
void addSceneCases(std::vector<Case>& cases);
void addCanvasCases(std::vector<Case>& cases);

}  // namespace gardener::gfx::test
