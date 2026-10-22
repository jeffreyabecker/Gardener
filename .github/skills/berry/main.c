// main.c — host entry point for the Berry check.
//
// It loads the harness, the data image's /modules, the driver and /routines in the
// order the firmware does, then calls harness_check(). A file that fails to compile
// or load is reported the way the firmware reports it and the rest still go on.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "berry.h"
#include "frame.h"

#define MAX_PATHS 256

static char* readFile(const char* path, size_t* length) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) return NULL;
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  rewind(file);
  if (size < 0) {
    fclose(file);
    return NULL;
  }
  char* text = (char*)malloc((size_t)size + 1);
  if (text == NULL) {
    fclose(file);
    return NULL;
  }
  if (fread(text, 1, (size_t)size, file) != (size_t)size) {
    fclose(file);
    free(text);
    return NULL;
  }
  text[size] = '\0';
  fclose(file);
  *length = (size_t)size;
  return text;
}

static void report(bvm* vm, const char* what, const char* path) {
  printf("[E] vm: %s %s: ", path, what);
  if (be_isstring(vm, -2)) printf("%s: ", be_tostring(vm, -2));
  const char* detail = be_tostring(vm, -1);
  printf("%s\n", detail != NULL ? detail : "error");
  be_pop(vm, 2);
}

static int callWithText(bvm* vm, const char* function, const char* name, const char* text) {
  if (!be_getglobal(vm, function)) {
    be_pop(vm, 1);
    return 0;
  }
  be_pushstring(vm, name);
  be_pushstring(vm, text);
  if (be_pcall(vm, 2) != BE_OK) {
    report(vm, function, name);
    return 0;
  }
  be_pop(vm, 1);
  return 1;
}

// --art draws instead of checking: the page runs and harness_art saves the frame it left. The call
// leaves its arguments above the result, so the answer is argc + 1 places down.
static int callArt(bvm* vm, const char* name, int seconds, const char* path) {
  be_pop(vm, be_top(vm));  // the seeded values leave their call's arguments behind
  if (!be_getglobal(vm, "harness_art")) {
    be_pop(vm, 1);
    printf("[E] vm: harness_art is missing\n");
    return 0;
  }
  be_pushstring(vm, name);
  be_pushint(vm, seconds);
  be_pushstring(vm, path);
  if (be_pcall(vm, 3) != BE_OK) {
    report(vm, "harness_art", name);
    return 0;
  }
  const int result = be_absindex(vm, -(3 + 1));
  const int ok = be_isbool(vm, result) && be_tobool(vm, result);
  be_pop(vm, 4);
  return ok;
}

static void printLog(bvm* vm) {
  if (!be_getglobal(vm, "harnessLog")) {
    be_pop(vm, 1);
    return;
  }
  const char* log = be_tostring(vm, -1);
  if (log != NULL && log[0] != '\0') fputs(log, stdout);
  be_pop(vm, 1);
}

int main(int argc, char** argv) {
  const char* files[MAX_PATHS];
  const char* seedNames[MAX_PATHS];
  const char* seedPaths[MAX_PATHS];
  int fileCount = 0;
  int seedCount = 0;
  int compileOnly = 0;
  const char* artName = NULL;
  const char* artPath = NULL;
  int artSeconds = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--compile-only") == 0) {
      compileOnly = 1;
    } else if (strcmp(argv[i], "--art") == 0 && i + 1 < argc) {
      artName = argv[++i];
    } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
      artPath = argv[++i];
    } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
      artSeconds = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--values") == 0 && i + 1 < argc) {
      const char* value = argv[++i];
      const char* split = strchr(value, '=');
      if (split == NULL || seedCount >= MAX_PATHS) {
        fprintf(stderr, "--values needs name=path\n");
        return 2;
      }
      seedNames[seedCount] = value;
      seedPaths[seedCount] = split + 1;
      seedCount++;
    } else if (fileCount < MAX_PATHS) {
      files[fileCount++] = argv[i];
    }
  }

  bvm* vm = be_vm_new();
  if (vm == NULL) {
    fprintf(stderr, "no memory for the VM\n");
    return 2;
  }
  // The frame is a native module: the harness's screen stub forwards to it, so it has to be a
  // global before any file names it.
  frameRegister(vm);

  int failed = 0;
  int loaded = 0;
  for (int i = 0; i < fileCount; i++) {
    size_t length = 0;
    char* source = readFile(files[i], &length);
    if (source == NULL) {
      printf("[E] vm: %s unreadable\n", files[i]);
      failed++;
      continue;
    }
    const int base = be_top(vm);
    int rc = be_loadbuffer(vm, files[i], source, length);
    if (rc != BE_OK) {
      report(vm, "compile", files[i]);
      failed++;
    } else {
      rc = be_pcall(vm, 0);
      if (rc != BE_OK) {
        report(vm, "load", files[i]);
        failed++;
      } else {
        loaded++;
      }
    }
    be_pop(vm, be_top(vm) - base);
    free(source);
  }

  for (int i = 0; i < seedCount; i++) {
    size_t length = 0;
    char* text = readFile(seedPaths[i], &length);
    if (text == NULL) {
      printf("[E] vm: %s unreadable\n", seedPaths[i]);
      failed++;
      continue;
    }
    // The name is everything before the '=' check.py put there, and ends at the '='.
    char name[128];
    const char* split = strchr(seedNames[i], '=');
    size_t nameLength = split != NULL ? (size_t)(split - seedNames[i]) : 0;
    if (nameLength >= sizeof(name)) nameLength = sizeof(name) - 1;
    memcpy(name, seedNames[i], nameLength);
    name[nameLength] = '\0';
    if (!callWithText(vm, "harness_seed", name, text)) failed++;
    free(text);
  }

  int passed = 1;
  if (artName != NULL) {
    passed = callArt(vm, artName, artSeconds, artPath != NULL ? artPath : "frame.bmp");
  } else if (!compileOnly) {
    if (!be_getglobal(vm, "harness_check")) {
      be_pop(vm, 1);
      printf("[E] vm: harness_check is missing\n");
      passed = 0;
    } else {
      const int rc = be_pcall(vm, 0);
      if (rc != BE_OK) {
        report(vm, "check", "harness");
        passed = 0;
      } else {
        passed = be_isbool(vm, -1) && be_tobool(vm, -1);
      }
      be_pop(vm, 1);
    }
  }

  printLog(vm);
  be_vm_delete(vm);

  printf("%d of %d file%s loaded", loaded, fileCount, fileCount == 1 ? "" : "s");
  if (artName != NULL) {
    printf(", art %s %s", artName, passed && failed == 0 ? "saved" : "failed");
  } else if (!compileOnly) {
    printf(", check %s", passed && failed == 0 ? "passed" : "failed");
  }
  printf("\n");
  return (failed == 0 && passed) ? 0 : 1;
}
