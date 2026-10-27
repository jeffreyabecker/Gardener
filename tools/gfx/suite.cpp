#include "suite.h"

#include <stdio.h>
#include <string.h>

namespace gardener::gfx::test {
namespace {

int failures = 0;
const char* failureText = "";

}  // namespace

StubDisplay display(PageWidth, PageHeight);
Canvas canvas(display.buffer(), display.size(), display.size());

void reset() {
  display.clearFrame();
  display.resetFlushes();
}

void expect(bool condition, const char* what) {
  if (condition) return;
  if (failures == 0) failureText = what;
  failures++;
}

void expectEq(long actual, long expected, const char* what) {
  if (actual == expected) return;
  if (failures == 0) {
    static char message[160];
    snprintf(message, sizeof(message), "%s: expected %ld, got %ld", what, expected, actual);
    failureText = message;
  }
  failures++;
}

void expectText(const char* actual, const char* expected, const char* what) {
  if (strcmp(actual, expected) == 0) return;
  if (failures == 0) {
    static char message[160];
    snprintf(message, sizeof(message), "%s: expected \"%s\", got \"%s\"", what, expected, actual);
    failureText = message;
  }
  failures++;
}

bool runCase(const Case& item) {
  failures = 0;
  failureText = "";
  item.run();
  if (failures == 0) {
    printf("[x] %s\n", item.name);
    return true;
  }
  printf("[!] %s\n      %s\n", item.name, failureText);
  return false;
}

}  // namespace gardener::gfx::test
