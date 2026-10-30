# Forge Code Style Guide

Style rules for two audiences: **Forge code** (`.fge`) and the **C++ toolchain sources** (`src/`, `tools/`).

## Part 1 — Forge (`*.fge`) Style

### Layout
- 2-space indentation; no tabs.
- Statements separated by newlines; semicolons optional but avoid mixing.
- One statement per line in match cases:
  ```forge
  let label = match day {
    case 1: "Mon"
    case _: "Weekend"
  }
  ```
- Braces on the same line as the construct (`fn f() {`).

### Naming
- `camelCase` — variables and functions (`total`, `parseName`)
- `PascalCase` — classes/structs (`class TokenReader`)
- `SCREAMING_SNAKE_CASE` — const globals (`const MAX_RETRIES = 3`)
- Prefix booleans with `is`/`has` (`hasAlreadyRun`)

### Declarations
- Use `let` by default, `const` for values that never change.
- Class methods are always declared with `fn` — bare method names are a parse error.
- Struct fields are space-separated: `struct Point { x y }` (not commas).
- Prefer early `return`; avoid deep nesting.

### Idioms
- Ternary over if-expression (`if ... else` is not an expression).
- `match` for multi-way value selection; it's an expression, so assign it.
- Constructors: `fn init(...)`; call `super.init(...)` first in subclasses.
- Prefer builtins over hand-rolled loops: `len`, `contains`, `slice`, `clone`, `keys`, `entries`.
- Iterate maps via `for (let k in keys(m))` — never `for (let kv in m)`.

```forge
fn greet(name) {
  print("Hello " + name)
}

class Person {
  fn init(name, age) {
    this.name = name
    this.age = age
  }

  fn introduce() {
    print(this.name)
  }
}
```

### Error Handling
- Report failures with `throw "message"`; catch in `try/catch (e)`.
- `error("msg")` throws; `assert(cond, "msg")` throws on false.
- Keep `try` blocks small and catch specific to the failing call.

### FFI / Native (`--trusted` only)
- Never enable `--trusted` in code reviews without justification; the script itself cannot flip it.
- Validate every `ffi.*` return before use.
- Pair `ffi.open/close` and free anything allocatable (`ffi.mem_free`).

### Comments
- `//` line comments; explain *why*, not *what*.
- Mark unfinished work with `// TODO(name):`.

## Part 2 — C++ Toolchain Style (`src/`, `tools/`)

### General
- C++17. Binary targets compile with warnings-as-errors-clean code.
- 2-space indent, same-line braces, matching existing files.
- `snake_case` for functions/fields, `PascalCase` for types, `constexpr`/UPPER for constants.
- Prefer `std::string_view`, `std::filesystem`, and structured bindings where already used nearby.

### Headers / Files
- One primary type per header (`runtime.hpp` is the exception as the FVM core).
- Minimal includes per file; forward-declare FVM types (`FVMThread`, `GCUpvalue`, …) in headers, define out-of-line where incomplete types meet `std::unique_ptr`.

### Memory & GC
- Heap objects derive from `GCObject` and are allocated via `gc_.newObj<T>()` — **never** raw `new` for anything the GC should own.
- Any new `GCObjType` must be handled in `traceReferences()` and `freeObject()`.
- Pointers into thread stacks are stable (stacks resize once at creation); never store `FValue*` beyond a call.

### VM / Interpreter Rules
- Every `as*()` accessor throws on type mismatch — do not add null-check workarounds; natives should still guard argument counts/types with explicit messages.
- New opcodes: add to `src/chunk.hpp` (append at the end — do **not** renumber), implement in **both** dispatch loops (`runtime.cpp` FVM and `vm.cpp` legacy) or the other VM will report "Unknown instruction".
- Thread-crossing state (try frames, open upvalues) lives on `FVMThread`, never shared VM fields — generator/actor threads interleave.
- Errors flow through `handleError(thread, msg)` returning `bool`; never `std::terminate` or crash on script mistakes.
- Native functions: validate `args.size()` with a clear message, then validate types; allocate results with `gc_` accessors; strings via `GCString`.

### Security Checklist (always apply)
- Anything executing shell (`system`, `popen`) must gate on trusted mode.
- Every user-controlled string reaching a syscall boundary must be validated (`isString()` before `asString()`).
- Parsers/recursive algorithms: enforce a depth cap (see `Parser::MAX_DEPTH`, `JsonParser::MAX_DEPTH`).
- Value stacks: `push()` must bounds-check; container recursion (`equals`) must depth-guard.

### Build System Output Convention (`src/forge_build.cpp`, `tools/cfmake.cpp`)
- Console painting: `:: ` for phase headers, `=> ` for checks/discoveries, `-- text! -- {secs}` for done lines, `==> ` for launches, `[ NN4%]` (see `fmtPct`) for ninja-style progress rows.
- Default output hides command lines ("cool GUI"); `-v` reveals them. Per-file args always land in `build-releases/make.ts` and config resolution in `cache/config.ts` (`.ts` = make-generated transcript).
- Single-file project file is `Forgefile`: no glob discovery — components are hand-written `component(path)` lines; legacy `ForgeLists.txt` remains accepted.

### Tests
- New builtins/behaviors: add assertions to `tests/test_core.fge` following the `test.describe/assert/assertEquals` pattern, run `./build-release/forgevm tests/test_core.fge`.
- Keep the build green: `cmake --build build-release -j4`, then rerun `test_minimal` and LSP smoke (`python3 tools/lsp_smoke.py`) when touching parser/LSP.

### Commits
- Concise, imperative subject (`fix: ...`, `feat: ...`, `docs: ...` similar to existing history).
- Never stage `build-release/`, `brag-output/`, or generated artifacts.
