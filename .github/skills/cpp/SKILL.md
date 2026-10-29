---
name: cpp
description: 'C++ naming and formatting standards for the Gardener firmware. Use when writing, changing, refactoring or reviewing C++ under libraries/ or firmware/: PascalCase constants with no k prefix, camelCase globals with no g prefix, PascalCase types, camelCase functions and variables, trailing-underscore members, two-space indents and a 100-column line.'
---

# C++

## When to use

- Writing, changing, refactoring or reviewing C++ under `libraries/` or `firmware/`
- Naming something new — a type, a constant, a global, a member, a file
- Reviewing a change for style

`libraries/berry/` is vendored third-party C and keeps its own naming. The rules here are for the
project's own code.

What the code should be — where a type, a driver or an interface belongs, and which facilities to
reach for instead of hand-rolling — is the [firmware skill](../firmware/SKILL.md)'s.

## Naming

| What | Case | Example |
| --- | --- | --- |
| Types — `class`, `struct`, `enum`, alias | PascalCase | `VacuumSystem`, `RunState`, `RoutineSettingStore` |
| Functions and methods | camelCase | `service()`, `startRefusal()`, `buildPanelRoot()` |
| Local variables and parameters | camelCase | `targetPressureKpa`, `nowMs` |
| Data members | camelCase, trailing underscore | `pump_`, `logger_`, `pidIntegral_` |
| Constants — `constexpr`, `const`, `static constexpr` | PascalCase | `PanelPriority`, `BackStopMs`, `MarkAsset` |
| Globals | camelCase | `machineCommandQueue`, `serialConnected` |
| Enum values | PascalCase | `Button::Back`, `RunState::Pulling` |
| Files | PascalCase, one type or one topic per file | `VacuumSystem.h`, `RoutineSettingStore.cpp` |
| Namespaces | lowercase | `gardener`, `fonts` |

**Constants are PascalCase, and never carry a `k` prefix.** `PanelWidth`, `DefaultHoldMs`,
`MarkAsset` — the prefix belongs to a convention from somewhere else, and it is not this
codebase's. A type is PascalCase too, so a constant and a type can look alike; the declaration
already says which one it is, and nobody has to decode a letter to read the line.

**Globals are camelCase, and never carry a `g` prefix.** `gContext` is `context`, `gLogger` is
`logger`. The same reason: a prefix repeats what the declaration already tells the reader, and a
name that needs a letter in front of it to be understood is working against the language.

Existing prefixed names are not a rewrite-on-sight target. The `SCREAMING_SNAKE_CASE` constants in
`libraries/Gardener/src/machine/Machine.h` are an earlier decision. Rename them where you are already
changing those lines for another reason; a sweep of the codebase for its own sake is its own
decision with its own measurement.

## Formatting

- Two spaces per indent, never tabs.
- A brace opens on the same line as the statement; a closing `}` gets its own line.
- Access specifiers sit one space in: ` public:`, ` private:`, ` protected:`.
- `#pragma once` in every header, not an include guard.
- The pointer or reference binds to the type: `Pump* pump_`, `Logger& logger`, `const char* text`.
- `const` goes before what it qualifies: `const char*`, `const uint32_t digit`.
- Lines aim at 100 columns; break a call or a condition and line the continuation up with the code
  inside the parenthesis.
- One blank line between functions. A closing namespace is `}  // namespace gardener`, or
  `}  // namespace` for an anonymous one.
- A long header groups its declarations with a divider comment:
  `// --- commands -----------------------------------------------------------`.
- Includes come in three groups, each separated by a blank line: the file's own header, then the
  angle-bracket headers, then the project's quoted ones. A class declares its public interface
  first, then ` private:` with the data members at the end, initialised where they are declared.

## Types and values

- Use the fixed-width types from `<stdint.h>` for anything stored, sent or compared against a
  limit: `uint8_t`, `uint16_t`, `uint32_t`, `size_t`. `int`, `bool` and `float` are fine for a
  loop counter, a local and arithmetic.
- `float`, not `double`, and the `f` suffix on the literal: `0.0f`, `2.5f`.
- `nullptr`, never `NULL`.
- `enum class` with an explicit underlying type when the value is stored or sent:
  `enum class RunState : uint8_t`.
- `constexpr` for a value known at compile time, rather than a `const` or a macro.
- `virtual` on the base, `override` on the override, and `= default` for a trivial destructor.
- Mark a method `const` when it does not change the object.
- Delete the copy constructor and the assignment for a type there is one of:
  `Machine(const Machine&) = delete;`.

## Comments

A comment earns its line by saying why the code is what it is — the constraint, the failure it
avoids, the reason the obvious version is wrong. Don't restate the code in a sentence. Short prose
on the line above the thing it explains, or trailing a field when the reason is about that field.
