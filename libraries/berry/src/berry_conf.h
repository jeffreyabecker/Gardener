#ifndef BERRY_CONF_H
#define BERRY_CONF_H

#include <assert.h>
#include <stdlib.h>

// Gardener's Berry configuration. The interpreter is unmodified; everything that differs from
// upstream's default is here, and each departure says why.
//
// Upstream's `default/berry_conf.h` is the reference. This file keeps the same macro names so a
// future upstream bump can be diffed against it.

// --- the machine -----------------------------------------------------------

// No debug build: it costs flash and RAM and nothing on the device reads it.
#define BE_DEBUG                        0
#define BE_DEBUG_SOURCE_FILE            0
#define BE_DEBUG_RUNTIME_INFO           0
#define BE_DEBUG_VAR_INFO               0
#define BE_USE_DEBUG_HOOK               0
#define BE_USE_DEBUG_GC                 0
#define BE_USE_DEBUG_STACK              0

// `long` is 32 bits on the ESP32-S3, so a bint is one word rather than two.
#define BE_INTGER_TYPE                  1

// The S3's FPU is single precision: a real is a float, and no double is pulled in.
#define BE_USE_SINGLE_FLOAT             1

// Largest `bytes` object, upstream's default. The bytes class is compiled in with the other core
// classes, so the ceiling has to be stated.
#define BE_BYTES_MAX_SIZE               (32 * 1024)

// --- memory ----------------------------------------------------------------

// Upstream's 20000 slots is a desktop number. At one bvalue per slot this bounds the VM's stack to
// well under the S3's free heap, and a routine that needs more than this is not a page.
#define BE_STACK_TOTAL_MAX              2000
#define BE_STACK_START                  50
#define BE_STACK_FREE_MIN               10

// Precompiled objects: upstream's default and the only path its non-precompiled macros compile on
// cleanly as C — the `be_native_module_*` macros name `bntvmodobj`, which is a type in C++ but a
// tag in C. The generated tree in `generate/` is vendored with the sources.
#define BE_USE_PRECOMPILED_OBJECT       1

// --- the interpreter -------------------------------------------------------

// No instruction counter: there is no observability hook, so a `poll` that never returns is by
// convention rather than enforcement. See docs and the plan for that trade.
#define BE_USE_PERF_COUNTERS            0

#define BE_CONST_SEARCH_SIZE            50
#define BE_USE_STR_HASH_CACHE           0
#define BE_USE_OVERLOAD_HASH            1
#define BE_MAX_PARSER_DEPTH             25
#define BE_USE_MEM_ALIGNED              0

// We compile source, so the compiler is on. Bytecode files are not something the device reads or
// writes, but leaving the loader and saver compiled keeps `be_filelib.c` (which calls them) intact.
#define BE_USE_SCRIPT_COMPILER          1
#define BE_USE_BYTECODE_SAVER           1
#define BE_USE_BYTECODE_LOADER          1

// No dynamic loading on a microcontroller.
#define BE_USE_SHARED_LIB               0

// The file class is compiled because `be_libs.c` loads it unconditionally, but `be_port.c` answers
// every call behind it with failure: Berry has no path to LittleFS.
#define BE_USE_FILE_SYSTEM              1

// --- modules ---------------------------------------------------------------

// `string` is what a routine formats with. `math` is cheap and occasionally wanted. Everything else
// would be dead weight: there is no console, no clock of its own, and no OS.
#define BE_USE_STRING_MODULE            1
#define BE_USE_MATH_MODULE              1
#define BE_USE_JSON_MODULE              0
#define BE_USE_TIME_MODULE              0
#define BE_USE_OS_MODULE                0
#define BE_USE_GLOBAL_MODULE            0
#define BE_USE_SYS_MODULE               0
#define BE_USE_DEBUG_MODULE             0
#define BE_USE_GC_MODULE                0
#define BE_USE_SOLIDIFY_MODULE          0
#define BE_USE_INTROSPECT_MODULE        0
#define BE_USE_STRICT_MODULE            0

// --- the platform ----------------------------------------------------------

// The ESP-IDF heap. These are the standard library's, which on the S3 are the heap's.
#define BE_EXPLICIT_ABORT               abort
#define BE_EXPLICIT_EXIT                exit
#define BE_EXPLICIT_MALLOC              malloc
#define BE_EXPLICIT_FREE                free
#define BE_EXPLICIT_REALLOC             realloc

#define be_assert(expr)                 assert(expr)

#endif
