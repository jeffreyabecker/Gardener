// The platform port: what Berry's C library calls to reach the outside world.
//
// Upstream ships this file per platform in `default/` and expects it to be replaced. This one is
// the firmware's: the device has a logger and a filesystem, and neither is Berry's to use.
//
// * `print` writes nowhere. Diagnostics are the firmware's, and a routine that has something to say
//   calls `vacuum.log()`, which reaches the serial line as a `[I]` record.
// * Every file and directory call fails. The `file` class is still registered — `be_libs.c` loads
//   it unconditionally — but nothing behind it can open a path, so a routine cannot read or write
//   LittleFS. The settings store stays the firmware's.

#include <stddef.h>

#include "berry.h"
#include "be_sys.h"

BERRY_API void be_writebuffer(const char* buffer, size_t length) {
  (void)buffer;
  (void)length;
}

BERRY_API char* be_readstring(char* buffer, size_t size) {
  (void)buffer;
  (void)size;
  return NULL;
}

void* be_fopen(const char* filename, const char* modes) {
  (void)filename;
  (void)modes;
  return NULL;
}

int be_fclose(void* hfile) {
  (void)hfile;
  return -1;
}

size_t be_fwrite(void* hfile, const void* buffer, size_t length) {
  (void)hfile;
  (void)buffer;
  (void)length;
  return 0;
}

size_t be_fread(void* hfile, void* buffer, size_t length) {
  (void)hfile;
  (void)buffer;
  (void)length;
  return 0;
}

char* be_fgets(void* hfile, void* buffer, int size) {
  (void)hfile;
  (void)buffer;
  (void)size;
  return NULL;
}

int be_fseek(void* hfile, long offset) {
  (void)hfile;
  (void)offset;
  return -1;
}

long int be_ftell(void* hfile) {
  (void)hfile;
  return -1;
}

long int be_fflush(void* hfile) {
  (void)hfile;
  return -1;
}

size_t be_fsize(void* hfile) {
  (void)hfile;
  return 0;
}

int be_isdir(const char* path) {
  (void)path;
  return 0;
}

int be_isfile(const char* path) {
  (void)path;
  return 0;
}

int be_isexist(const char* path) {
  (void)path;
  return 0;
}

char* be_getcwd(char* buf, size_t size) {
  (void)buf;
  (void)size;
  return NULL;
}

int be_chdir(const char* path) {
  (void)path;
  return -1;
}

int be_mkdir(const char* path) {
  (void)path;
  return -1;
}

int be_unlink(const char* filename) {
  (void)filename;
  return -1;
}

int be_dirfirst(bdirinfo* info, const char* path) {
  (void)info;
  (void)path;
  return 1;
}

int be_dirnext(bdirinfo* info) {
  (void)info;
  return 1;
}

int be_dirclose(bdirinfo* info) {
  (void)info;
  return 1;
}
