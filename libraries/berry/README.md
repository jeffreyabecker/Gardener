# Berry 1.1.0, vendored

Upstream: https://github.com/berry-lang/berry, tag/commit 1.1.0 (BERRY_VERSION in `src/berry.h`).
MIT, see [LICENSE](./LICENSE).

`src/` is upstream's tree as-is except for two files this project owns and one file left out:

| File | Why |
| --- | --- |
| `src/berry_conf.h` | the build configuration — embedded choices, no debug, no optional modules |
| `src/be_port.c` | the platform port — stdout and the file API, implemented so Berry cannot reach LittleFS |
| `src/be_modtab.c` | upstream's module table from `default/`, moved here because it has to be compiled |
| `src/be_repl.*` | left out: the read-eval-print loop is the CLI's, and nothing in firmware uses it |
| `generate/` | the precompiled const tables, produced by upstream's `tools/coc`. `BE_USE_PRECOMPILED_OBJECT` is 1 because the non-precompiled macros name `bntvmodobj`, which is a type only in C++ — the interpreter is C. Regenerate with `make` in an upstream checkout if the sources are ever updated. |

`src/be_filelib.c` is still compiled: `be_libs.c` calls `be_load_filelib` unconditionally, so removing
the file would mean patching the interpreter. The `file` class therefore exists in the VM, but every
call behind it fails — `be_port.c` answers `be_fopen` with null and the directory calls with "no".
Deleting the class is a `be_libs.c` patch if that ever matters.

The module table is the extension point for the firmware's own modules (`vacuum`, `screen`,
`buttons`, `routines`); it carries explicit `user-defined modules` markers for exactly that.
