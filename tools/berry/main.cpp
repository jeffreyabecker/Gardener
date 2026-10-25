// The host entry point for the Berry check.
//
// It builds the panel the device would have — the real interpreter, bindings, overlay and control
// law, with a bitmap for a glass and a chamber for a rig — loads the image through the routines'
// own loader, then loads the driver and runs it.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "HarnessSession.h"
#include "berry.h"

namespace {

const char* DefaultData = "firmware/gardener/data";

// A file the check loads by name: the fallback is the driver's, next to this program's sources.
char* readFile(const char* path, size_t* length) {
  FILE* file = fopen(path, "rb");
  if (file == nullptr) return nullptr;
  fseek(file, 0, SEEK_END);
  const long size = ftell(file);
  rewind(file);
  if (size < 0) {
    fclose(file);
    return nullptr;
  }
  char* text = static_cast<char*>(malloc((size_t)size + 1));
  if (text == nullptr) {
    fclose(file);
    return nullptr;
  }
  if (fread(text, 1, (size_t)size, file) != (size_t)size) {
    fclose(file);
    free(text);
    return nullptr;
  }
  text[size] = '\0';
  fclose(file);
  *length = (size_t)size;
  return text;
}

// A failure leaves the exception and its argument, the way the firmware reports one.
void report(bvm* vm, const char* what, const char* path) {
  printf("[E] vm: %s %s: ", path, what);
  if (be_isstring(vm, -2)) printf("%s: ", be_tostring(vm, -2));
  const char* detail = be_tostring(vm, -1);
  printf("%s\n", detail != nullptr ? detail : "error");
  be_pop(vm, 2);
}

bool loadFile(bvm* vm, const char* path) {
  size_t length = 0;
  char* source = readFile(path, &length);
  if (source == nullptr) {
    printf("[E] vm: %s unreadable\n", path);
    return false;
  }

  const int base = be_top(vm);
  bool ok = be_loadbuffer(vm, path, source, length) == BE_OK;
  if (ok) {
    const int rc = be_pcall(vm, 0);
    ok = rc == BE_OK;
    if (!ok) report(vm, "load", path);
  } else {
    report(vm, "compile", path);
  }
  be_pop(vm, be_top(vm) - base);
  free(source);
  return ok;
}

// The driver's own error and note lines, which the checks write as they run.
void printDriverLog(bvm* vm) {
  if (!be_getglobal(vm, "harnessLog")) {
    be_pop(vm, 1);
    return;
  }
  const char* log = be_tostring(vm, -1);
  if (log != nullptr && log[0] != '\0') fputs(log, stdout);
  be_pop(vm, 1);
}

int callCheck(bvm* vm) {
  if (!be_getglobal(vm, "harness_check")) {
    be_pop(vm, 1);
    printf("[E] vm: harness_check is missing\n");
    return 0;
  }
  const int rc = be_pcall(vm, 0);
  if (rc != BE_OK) {
    report(vm, "check", "harness");
    return 0;
  }
  const bool passed = be_isbool(vm, -1) && be_tobool(vm, -1);
  be_pop(vm, 1);
  return passed ? 1 : 0;
}

// The suite runner, registered by suite.be: runs every test, or those whose full name contains
// the filter, and answers whether any failed.
int callSuites(bvm* vm, const char* filter) {
  if (!be_getglobal(vm, "suite_run")) {
    be_pop(vm, 1);
    printf("[E] vm: suite_run is missing\n");
    return 0;
  }
  be_pushstring(vm, filter != nullptr ? filter : "");
  const int rc = be_pcall(vm, 1);
  if (rc != BE_OK) {
    report(vm, "suite_run", "harness");
    return 0;
  }
  // be_pcall leaves the argument on the stack under the result, as callArt does below.
  const int result = be_absindex(vm, -(1 + 1));
  const bool passed = be_isbool(vm, result) && be_tobool(vm, result);
  be_pop(vm, 2);
  return passed ? 1 : 0;
}

// Draw one of the device's own scenes — readout, browser or system — and save the frame. The
// driver reaches them by their globals, because the well-known scenes are not registry entries.
int callArtScene(bvm* vm, const char* name, int seconds, const char* path) {
  if (!be_getglobal(vm, "harness_art_scene")) {
    be_pop(vm, 1);
    printf("[E] vm: harness_art_scene is missing\n");
    return 0;
  }
  be_pushstring(vm, name);
  be_pushint(vm, seconds);
  be_pushstring(vm, path);
  const int rc = be_pcall(vm, 3);
  if (rc != BE_OK) {
    report(vm, "harness_art_scene", name);
    return 0;
  }
  const int result = be_absindex(vm, -(3 + 1));
  const bool ok = be_isbool(vm, result) && be_tobool(vm, result);
  be_pop(vm, 4);
  return ok ? 1 : 0;
}

int callArt(bvm* vm, const char* name, int seconds, const char* path, int sets) {
  if (!be_getglobal(vm, "harness_art")) {
    be_pop(vm, 1);
    printf("[E] vm: harness_art is missing\n");
    return 0;
  }
  be_pushstring(vm, name);
  be_pushint(vm, seconds);
  be_pushstring(vm, path);
  be_pushint(vm, sets);
  const int rc = be_pcall(vm, 4);
  if (rc != BE_OK) {
    report(vm, "harness_art", name);
    return 0;
  }
  const int result = be_absindex(vm, -(4 + 1));
  const bool ok = be_isbool(vm, result) && be_tobool(vm, result);
  be_pop(vm, 5);
  return ok ? 1 : 0;
}

// Draw one component by name — a drawing primitive from either framework — and save the frame.
int callArtComponent(bvm* vm, const char* name, const char* path) {
  if (!be_getglobal(vm, "harness_art_component")) {
    be_pop(vm, 1);
    printf("[E] vm: harness_art_component is missing\n");
    return 0;
  }
  be_pushstring(vm, name);
  be_pushstring(vm, path);
  const int rc = be_pcall(vm, 2);
  if (rc != BE_OK) {
    report(vm, "harness_art_component", name);
    return 0;
  }
  const int result = be_absindex(vm, -(2 + 1));
  const bool ok = be_isbool(vm, result) && be_tobool(vm, result);
  be_pop(vm, 3);
  return ok ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
  const char* data = DefaultData;
  const char* harnessPath = "harness.be";
  const char* driverPath = "driver.be";
  const char* artName = nullptr;
  const char* artPath = nullptr;
  const char* sceneName = nullptr;
  const char* componentName = nullptr;
  const char* testFilter = nullptr;
  int artSeconds = 0;
  int artSets = 0;
  float ceilingKpa = gardener::ChamberCeilingKpa;
  bool compileOnly = false;
  bool verbose = false;
  bool suites = false;
  std::vector<std::string> suiteFiles;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
      data = argv[++i];
    } else if (strcmp(argv[i], "--harness") == 0 && i + 1 < argc) {
      harnessPath = argv[++i];
    } else if (strcmp(argv[i], "--driver") == 0 && i + 1 < argc) {
      driverPath = argv[++i];
    } else if (strcmp(argv[i], "--suite") == 0 && i + 1 < argc) {
      suiteFiles.emplace_back(argv[++i]);
      suites = true;
    } else if (strcmp(argv[i], "--test-file") == 0 && i + 1 < argc) {
      suiteFiles.emplace_back(argv[++i]);
      suites = true;
    } else if (strcmp(argv[i], "--test") == 0 && i + 1 < argc) {
      testFilter = argv[++i];
      suites = true;
    } else if (strcmp(argv[i], "--compile-only") == 0) {
      compileOnly = true;
    } else if (strcmp(argv[i], "--verbose") == 0) {
      verbose = true;
    } else if (strcmp(argv[i], "--art") == 0 && i + 1 < argc) {
      artName = argv[++i];
    } else if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
      sceneName = argv[++i];
    } else if (strcmp(argv[i], "--component") == 0 && i + 1 < argc) {
      componentName = argv[++i];
    } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
      artPath = argv[++i];
    } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
      artSeconds = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--sets") == 0 && i + 1 < argc) {
      artSets = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--ceiling-kpa") == 0 && i + 1 < argc) {
      ceilingKpa = (float)atof(argv[++i]);
    } else {
      fprintf(stderr, "unknown argument %s\n", argv[i]);
      return 2;
    }
  }

  gardener::HarnessSession session(data, ceilingKpa);
  if (!session.begin()) {
    fprintf(stderr, "no data image at %s\n", data);
    return 2;
  }

  bvm* vm = session.vm();
  installHarnessModule(vm, session);

  int failed = 0;
  if (!loadFile(vm, harnessPath)) failed++;
  if (!loadFile(vm, driverPath)) failed++;
  for (const std::string& file : suiteFiles) {
    if (!loadFile(vm, file.c_str())) failed++;
  }

  // The registry is Berry, so the count of registered routines is read back from a global the
  // driver sets after the image has loaded.
  bint routineCount = 0;
  if (be_getglobal(vm, "harnessRoutineCount")) {
    routineCount = be_toint(vm, -1);
    be_pop(vm, 1);
  }
  int passed = 0;
  if (artName != nullptr) {
    passed = callArt(vm, artName, artSeconds, artPath != nullptr ? artPath : "frame.bmp", artSets);
  } else if (sceneName != nullptr) {
    passed = callArtScene(vm, sceneName, artSeconds, artPath != nullptr ? artPath : "frame.gif");
  } else if (componentName != nullptr) {
    passed = callArtComponent(vm, componentName, artPath != nullptr ? artPath : "frame.bmp");
  } else if (suites) {
    // One GIF per case, named from the stem the runner reads back through the harness. A .bmp
    // --out is meaningless under --tests, so it falls back to the default stem.
    std::string stem = "build/runs";
    if (artPath != nullptr) {
      const size_t len = strlen(artPath);
      if (len >= 4 && strcmp(artPath + len - 4, ".gif") == 0) {
        stem.assign(artPath, len - 4);
      }
    }
    session.setCaptureStem(stem);
    passed = callSuites(vm, testFilter);
  } else if (!compileOnly) {
    passed = callCheck(vm);
  } else {
    passed = 1;
  }

  printDriverLog(vm);
  if (verbose) fputs(session.logger().text().c_str(), stdout);

  printf("%lld routine%s", (long long)routineCount, routineCount == 1 ? "" : "s");
  if (artName != nullptr) {
    printf(", art %s %s", artName, passed && failed == 0 ? "saved" : "failed");
  } else if (sceneName != nullptr) {
    printf(", scene %s %s", sceneName, passed && failed == 0 ? "saved" : "failed");
  } else if (componentName != nullptr) {
    printf(", component %s %s", componentName, passed && failed == 0 ? "saved" : "failed");
  } else if (suites) {
    printf(", suites %s", passed && failed == 0 ? "passed" : "failed");
  } else if (!compileOnly) {
    printf(", check %s", passed && failed == 0 ? "passed" : "failed");
  }
  printf("\n");
  return (failed == 0 && passed != 0) ? 0 : 1;
}
