// The host entry point for the Berry check, as hostMain: entry.cpp holds the `main` the runtime
// calls, because the API header declares a core's `int main()` beside it.
//
// It builds the panel the device would have — the real interpreter, bindings, overlay and control
// law, with a bitmap for a glass and a chamber for a rig — loads the image through the routines'
// own loader, then loads the driver and runs it.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <utility>
#include <vector>

#include "HarnessSession.h"
#include "berry.h"

namespace {

const char* DefaultData = "firmware/data";

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

// Draw one of the device's own scenes — home, browser or system — and save the frame. The
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

int callArt(bvm* vm, const char* name, int seconds, const char* path) {
  if (!be_getglobal(vm, "harness_art")) {
    be_pop(vm, 1);
    printf("[E] vm: harness_art is missing\n");
    return 0;
  }
  be_pushstring(vm, name);
  be_pushint(vm, seconds);
  be_pushstring(vm, path);
  const int rc = be_pcall(vm, 3);
  if (rc != BE_OK) {
    report(vm, "harness_art", name);
    return 0;
  }
  const int result = be_absindex(vm, -(3 + 1));
  const bool ok = be_isbool(vm, result) && be_tobool(vm, result);
  be_pop(vm, 4);
  return ok ? 1 : 0;
}

// The figures the report prints are kilobytes, one decimal: a file is not a whole number of them and
// the difference between two of them is what the table is read for.
double kbOf(size_t bytes) { return (double)bytes / 1024.0; }

// The boot's own figures: the interpreter before the image loads, the framework once `/system` has,
// and the image once the root scene is built.
void printMemorySummary(gardener::HarnessSession& session) {
  printf("[I] mem: vm %.1f KB, framework %.1f KB, image %.1f KB\n", kbOf(session.vmBytes()),
         kbOf(session.frameworkBytes()), kbOf(session.imageBytes()));
}

// What each image file added, in the order they loaded, and then what the root scene added.
void printMemoryTable(gardener::HarnessSession& session) {
  for (const std::pair<std::string, size_t>& file : session.fileCosts()) {
    printf("[I] mem: %s %.1f KB\n", file.first.c_str(), kbOf(file.second));
  }
  printf("[I] mem: root scene %.1f KB\n", kbOf(session.sceneBytes()));
}

}  // namespace

int hostMain(int argc, char** argv) {
  const char* data = DefaultData;
  const char* harnessPath = "harness.be";
  const char* driverPath = "driver.be";
  const char* artName = nullptr;
  const char* artPath = nullptr;
  const char* sceneName = nullptr;
  const char* testFilter = nullptr;
  int artSeconds = 0;
  float ceilingKpa = gardener::ChamberCeilingKpa;
  bool compileOnly = false;
  bool verbose = false;
  bool memoryOnly = false;
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
    } else if (strcmp(argv[i], "--memory") == 0) {
      memoryOnly = true;
    } else if (strcmp(argv[i], "--art") == 0 && i + 1 < argc) {
      artName = argv[++i];
    } else if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
      sceneName = argv[++i];
    } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
      artPath = argv[++i];
    } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
      artSeconds = atoi(argv[++i]);
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

  // The measurement path: the image alone. No harness module, no driver, no suites, so the figures
  // are what the panel holds rather than what the check brings with it.
  if (memoryOnly) {
    printMemorySummary(session);
    printMemoryTable(session);
    return 0;
  }

  installHarnessModule(vm, session);

  int failed = 0;
  if (!loadFile(vm, harnessPath)) failed++;
  if (!loadFile(vm, driverPath)) failed++;
  for (const std::string& file : suiteFiles) {
    if (!loadFile(vm, file.c_str())) failed++;
  }

  // The registry is Berry, so the two counts the summary prints are read back from globals the
  // harness sets once the image has loaded: the keys it registered, and the keys a run drove.
  bint routineCount = 0;
  if (be_getglobal(vm, "harnessRoutineCount")) {
    routineCount = be_toint(vm, -1);
    be_pop(vm, 1);
  }
  int passed = 0;
  if (artName != nullptr) {
    passed = callArt(vm, artName, artSeconds, artPath != nullptr ? artPath : "frame.gif");
  } else if (sceneName != nullptr) {
    passed = callArtScene(vm, sceneName, artSeconds, artPath != nullptr ? artPath : "frame.gif");
  } else if (suites) {
    // One GIF per case, named from the stem the runner reads back through the harness. A --out
    // that is not a .gif is meaningless under --tests, so it falls back to the default stem.
    std::string stem = "build/runs";
    if (artPath != nullptr) {
      const size_t len = strlen(artPath);
      if (len >= 4 && strcmp(artPath + len - 4, ".gif") == 0) {
        stem.assign(artPath, len - 4);
      }
    }
    session.setCaptureStem(stem);
    const bool suitesPassed = callSuites(vm, testFilter) != 0;
    passed = suitesPassed ? 1 : 0;
  } else if (!compileOnly) {
    passed = callCheck(vm);
  } else {
    passed = 1;
  }

  // The boot's own figures, ahead of the driver's log: what the interpreter, the framework and the
  // image cost, so that a total has something to be read against.
  if (suites) printMemorySummary(session);
  printDriverLog(vm);
  if (verbose) fputs(session.logger().text().c_str(), stdout);

  // The keys a run drove: the harness appends each routine whose page it pushed, so a key that
  // answered no page is visible in the count rather than a quiet gap in the log.
  bint drivenCount = 0;
  if (be_getglobal(vm, "harnessDrivenCount")) {
    drivenCount = be_toint(vm, -1);
    be_pop(vm, 1);
  }

  // What a run is a statement about: the image's files loaded of the files found, and the routines
  // driven of the routines registered. A file that did not compile, a `.be` file `/system/order`
  // does not name, a page that raised on a beat and a key that answered no page each leave these
  // counts apart, or the log holding an error line.
  const long long loaded = (long long)session.filesLoaded();
  const long long found = (long long)session.filesFound();
  const long long registered = (long long)routineCount;
  const long long driven = (long long)drivenCount;
  // A --tests, --art or --compile run drives the pages its cases, its name or nothing reach, not
  // every registration, so only a full check holds the two counts to each other.
  const bool whole = loaded == found && session.errorLines() == 0;
  const bool drove = artName != nullptr || sceneName != nullptr || suites || compileOnly ||
                     driven == registered;
  const bool ok = failed == 0 && passed != 0 && whole && drove;

  printf("%lld of %lld files loaded, %lld of %lld routines driven", loaded, found, driven, registered);
  if (artName != nullptr) {
    printf(", art %s %s", artName, ok ? "saved" : "failed");
  } else if (sceneName != nullptr) {
    printf(", scene %s %s", sceneName, ok ? "saved" : "failed");
  } else if (suites) {
    printf(", suites %s", ok ? "passed" : "failed");
  } else if (!compileOnly) {
    printf(", check %s", ok ? "passed" : "failed");
  }
  printf("\n");
  return ok ? 0 : 1;
}
