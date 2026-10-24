# Forge SDK Guide

This guide documents the **verified** API surface of Forge/FVM as implemented in this repository. Treat anything not listed here as untested. Language features that are broken in the current FVM are explicitly called out.

## Quick Start

```bash
# Build the toolchain (from repo root)
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target forgevm -j4

# Run a script
./build-release/forgevm examples/simple.fge

# Run with FFI/trusted features enabled
./build-release/forgevm --trusted path/to/script.fge

# Interactive REPL (no file)
./build-release/forgevm
```

Binaries produced: `build-release/forge`, `build-release/forgevm`. Leave `build-release/` unstaged when committing.

## CLI

```text
forgevm [options] <file.fge | file.fclass>
forgevm compile <input.fge> [output.fclass]
forgevm java <file.fge>
forgevm lib new <name> [--cpp]
forgevm lib build [dir]
forgevm pkg <command>
forgevm build [options]        # build from ForgeLists.txt
forgevm android new <dir> [--package ...] [--name ...] [--source ...]
forgevm android build [dir]
forgevm android install [dir]
forgevm android run [dir]
forgevm android clean [dir]
```

Options: `-v`/`-h`, `-e <code>`, `--sdl`, `--gui`, `--ide`, `--gc-stats`, `--trusted`.

## Language (verified subset)

### Supported
- `let` / `const` declarations
- Functions: `fn name(a, b) { ... }` (no implicit return)
- Closures: `fn(v) { ... }` (explicit `return`)
- `if` / `else` statements
- `while` loops
- C-style `for` loops
- `for (let x in array) { ... }`
- `match` with `case` / `case _`
- `try` / `catch`
- `throw`
- Classes: `class`, `extends`, `super.init(...)`
- `print(...)`
- Arrays `[]`, maps `{}`

### Not supported / broken (current FVM)
| Feature | Observed |
| --- | --- |
| Arrow functions `fn(x) => expr` | `Error: Expected ')' ... (got '=')` |
| Default parameters | `Error: Expected ')' ... (got '=')` |
| `let y = if (...) ... else ...` (expression form) | `Error: Unexpected token 'if'` |
| Generators `gen` / `yield` | `Runtime Error: Undefined variable 'gen'` |
| String/map method calls like `"hello".upper()`, `.keys()` | `Runtime Error: Cannot access property on non-object` |
| `for` over maps | `Runtime Error: Map key must be a string` |

## Modules

Desktop (FVM) modules registered: `gui`, `io`, `os`, `system`, `json`, `path`.  
Note: `json.parse` is currently a stub returning `nil`.

Java backend maps: `io`, `os`, `system`, `gui`.

## FFI (native interop) — requires `--trusted`

```forge
import "ffi"

let lib = ffi.open("./libmathx.so")
let p_add = ffi.sym(lib, "add")
print(ffi.call_int(p_add, 2, 3))  // 5
ffi.close(lib)
```

Functions: `ffi.open(path)`, `ffi.close(h)`, `ffi.sym(h, name)`, `ffi.call_int`, `ffi.call_float`, `ffi.call_str`, `ffi.call_void`, `ffi.str_ptr(s)`, `ffi.mem_alloc(n)`, `ffi.mem_free(p)`, `ffi.mem_read(p,n)`, `ffi.mem_write(p,bytes)`, `ffi.sizeof_int/long/ptr`.

## Building native libraries

```bash
forgevm lib new mylib        # C scaffold
forgevm lib new mylib --cpp  # C++ scaffold (extern "C" boundary)
cd mylib && sh build.sh
forgevm --trusted main.fge
```

C++ scaffolds use real C++ (classes, `std::vector`, `std::string`) behind `extern "C"` exports callable via `ffi`.

## Exporting Forge as a shared library (C ABI)

```text
project(MathLib)
version(1.0.0)
source(src)
output(libmathforge)
shared(true)
```

```bash
forgevm build --shared
# produces libmathforge.so and libmathforge.so.h
```

Exported C API (from generated header): `forge_lib_init()`, `forge_lib_run()`, `forge_lib_select(fn)`, `forge_lib_call_selected_int0/1/2`, `forge_lib_call_selected_float0/1/2`, `forge_lib_call_selected_str0/1`, `forge_lib_call_str1`, `forge_lib_last_error()`, `forge_lib_free_string()`, `forge_lib_source()`, `forge_lib_version()`.  
Forge programs can also call exported libraries via `ffi` (see round-trip example above).

## Standalone binaries

```text
project(MyApp)
source(src)
output(myapp)
```

```bash
forgevm build
./myapp
```

Uses `libforge_core.a` when available (built with the repo) for fast linking; falls back to self-contained compilation otherwise.

## GUI (desktop / FVM)

Module `gui` provides: `window`, `button`, `textfield`, `panel`, `label`, `screenshot`, `checkbox`, `progress`, `slider`, `gauge`, `toggle`, `radio`, `tabs`, `list`, `combo`, `set_value`, `get_value`, `set_items`, `select`, `get_selected`, `tick`, `poll`, `set_text`, `get_text`, `set_checked`, `set_progress`, `close`.

## Android liquid glass

`forgevm android new/build/install/run` scaffolds and builds Android apps.  
Android-only `gui` natives: `search`, `segmented`, `card`, `stepper`, `magnifier`, `bottombar`, `set_glass`, `set_tint` (plus `draggertabs` kind).  
Tuning: `gui.set_glass(id, blur, saturation, opacity)`, `gui.set_tint(id, "#RRGGBB"|"#AARRGGBB")`.

## Language Server & VS Code

- `tools/forge_lsp` / `build-release/tools/forge-lsp`: LSP server with diagnostics, completion, hover, definition, references (JSON-RPC over stdio).
- `editors/vscode/`: VS Code extension with syntax highlighting, LSP integration, run/build/export/scaffold commands.
- Smoke test: `python3 tools/lsp_smoke.py`
- VS Code integration test: `node editors/vscode/test/client.test.js`

## Binary decompilation (.fclass)

C tools to decompile and structurally organize binaries with hash matching:

```bash
# Build decompilers
g++ -std=c++17 -I. -o tools/forge-decompiler tools/forge_decompiler.cpp src/fvm/classfile.cpp
g++ -std=c++17 -I. -o tools/fclass-decompile tools/fclass_decompile.cpp src/fvm/classfile.cpp

# Decompile a .fclass
./tools/forge-decompiler path/to/file.fclass ./decompiled

# Or entire directory
./tools/forge-decompiler ./some/dir ./decompiled_out
```

Output structure per binary: `binary/` (original + `META.info` with `djb2_hash`, `structural_hash`, size, magic), `src/` (reconstructed `.fge`), `methods/` (per-method `.dis` + raw `.bytecode`), `constant_pool.dis`, `verify.sh`. Top-level `HASH_INDEX.txt` indexes all binaries.

## Testing

```bash
./build-release/forgevm tests/test_core.fge
./build-release/forgevm tests/test_for.fge
# Note: tests/test_core.fge:139 has a pre-existing issue due to json.parse stub returning nil
```
