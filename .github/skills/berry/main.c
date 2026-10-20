// main.c — host entry point for the Berry check.
//
// It loads the harness, the data image's /modules, the driver and /routines in the
// order the firmware does, then calls harness_check(). A file that fails to compile
// or load is reported the way the firmware reports it and the rest still go on.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "berry.h"

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
  const char* iniNames[MAX_PATHS];
  const char* iniPaths[MAX_PATHS];
  int fileCount = 0;
  int iniCount = 0;
  int compileOnly = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--compile-only") == 0) {
      compileOnly = 1;
    } else if (strcmp(argv[i], "--ini") == 0 && i + 1 < argc) {
      const char* value = argv[++i];
      const char* split = strchr(value, '=');
      if (split == NULL || iniCount >= MAX_PATHS) {
        fprintf(stderr, "--ini needs name=path\n");
        return 2;
      }
      iniNames[iniCount] = value;
      iniPaths[iniCount] = split + 1;
      iniCount++;
    } else if (fileCount < MAX_PATHS) {
      files[fileCount++] = argv[i];
    }
  }

  bvm* vm = be_vm_new();
  if (vm == NULL) {
    fprintf(stderr, "no memory for the VM\n");
    return 2;
  }

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

  for (int i = 0; i < iniCount; i++) {
    size_t length = 0;
    char* text = readFile(iniPaths[i], &length);
    if (text == NULL) {
      printf("[E] vm: %s unreadable\n", iniPaths[i]);
      failed++;
      continue;
    }
    // The name is everything before the '=' check.py put there, and ends at the '='.
    char name[128];
    const char* split = strchr(iniNames[i], '=');
    size_t nameLength = split != NULL ? (size_t)(split - iniNames[i]) : 0;
    if (nameLength >= sizeof(name)) nameLength = sizeof(name) - 1;
    memcpy(name, iniNames[i], nameLength);
    name[nameLength] = '\0';
    if (!callWithText(vm, "harness_seed", name, text)) failed++;
    free(text);
  }

  int passed = 1;
  if (!compileOnly) {
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
  if (!compileOnly) {
    printf(", check %s", passed && failed == 0 ? "passed" : "failed");
  }
  printf("\n");
  return (failed == 0 && passed) ? 0 : 1;
}
