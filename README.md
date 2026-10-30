<div align="center">

```
███████╗ ██████╗ ██████╗  ██████╗ ███████╗
██╔════╝██╔═══██╗██╔══██╗██╔════╝ ██╔════╝
█████╗  ██║   ██║██████╔╝██║  ███╗█████╗
██╔══╝  ██║   ██║██╔══██╗██║   ██║██╔══╝
██║     ╚██████╔╝██║  ██║╚██████╔╝███████╗
╚═╝      ╚═════╝ ╚═╝  ╚═╝ ╚═════╝ ╚══════╝
```

### **F O R G E** · A Modern Compiled-Like Language & Toolchain

*Run on FVM • Compile to `.fclass` bytecode • Export C ABI shared libraries • Build Android Liquid Glass apps*

[![Language](https://img.shields.io/badge/Language-Forge-ff6b35?style=for-the-badge)](https://github.com/Nohat1337/FORGE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows%20x64-4a9eff?style=for-the-badge)](https://github.com/Nohat1337/FORGE)
[![License](https://img.shields.io/badge/License-MIT-2ed573?style=for-the-badge)](LICENSE)

</div>

## ✦ Overview

Forge is a full-stack language toolchain with a hand-written lexer, parser, compiler, and the **Forge Virtual Machine (FVM)** — all C++17, no runtime dependencies except SDL2 (optional, for the windowed UI). Write `.fge` scripts, run them instantly on the VM, compile to `.fclass` bytecode, decompile it back, export Forge functions as a C-ABI shared library callable from C/C++, call native C++ libraries from Forge via FFI, or generate an Android Liquid Glass app.

## ✦ Quick Start (Linux)

```bash
git clone https://github.com/Nohat1337/FORGE.git
cd FORGE
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j4

# Run a script
./build-release/forgevm examples/simple.fge

# Drop into the REPL
./build-release/forgevm
```

On Windows use `install_forge_windows.bat` / `build_forge.bat` — see [README_WINDOWS.md](README_WINDOWS.md).

## ✦ Your First Program

```forge
// hello.fge
fn fib() {
    let a = 0
    let b = 1
    while (true) {
        yield a          // generators work
        let t = a + b
        a = b
        b = t
    }
}

let g = fib()
print(next(g)) // 0
print(next(g)) // 1
print(next(g)) // 1
```

```bash
./build-release/forgevm hello.fge
```

## ✦ Language Highlights (all verified)

| Feature | Example | Status |
| :-- | :-- | :-- |
| Variables | `let x = 10` / `const MAX = 5` | ✅ |
| Functions + closures | `fn add(a, b) { return a + b }` | ✅ |
| Arrow functions | `let f = fn(x) => x * 2` | ✅ |
| Nested functions + recursion | `fn` inside `fn`, self-recursive | ✅ |
| Generators | `gen yield(...)` + `next(g)` | ✅ |
| C-style & for-in loops | `for (let i = 0; i < n; i = i + 1)`, `for (let x in arr)` | ✅ |
| Ternary | `x > 5 ? "big" : "small"` | ✅ |
| `match` expressions | usable inline (`print(match n {...})`) | ✅ |
| Classes + inheritance | `class Dog extends Animal` (`super.init()`) | ✅ |
| Access specifiers | `public:` / `private:` / `protected:` accepted inside class/impl | ✅ |
| Structs + `impl` | `struct Point { x y }`, methods via `impl` | ✅ |
| String interpolation | `print("hello ${name}!")` | ✅ |
| try/catch/throw | `try { throw "boom" } catch (e) { print(e) }` | ✅ |
| Deep equality | `[1,2] == [1,2]` is `true` (arrays/maps compare structurally) | ✅ |
| Array ops | indexing, `+` concatenation, `push`, `slice`, `len` | ✅ |

## ✦ Builtin Functions

```
print  len  push  array  next  str  int  float  type  error
keys  values  has  entries  slice  clone
upper  lower  trim  split  contains  replace  substring  charAt
parseInt  toFloat
abs  min  max  sqrt  pow  floor  ceil  round
random  randomInt
```

## ✦ Modules

| Module | Purpose | Notes |
| :-- | :-- | :-- |
| `io` | File & stream I/O (`io.read`, `io.write`) | |
| `os` | `os.time`, `os.args`, and shell powers | `os.execute` / `os.capture` need `--trusted` |
| `json` | `json.stringify`, `json.parse` | real parser, 128-level depth limit |
| `path` | Cross-platform path utilities (`path.join`, `path.base`) | |
| `system` | System information | |
| `fs` | Filesystem: `read_file`, `write_file`, `remove`, `rename`, `create_dir`, `exists` | |
| `test` | `test.assert`, `test.assertEquals`, `test.describe`, `test.results` | used by `tests/` |
| `ffi` | Native interop: `ffi.open`, `ffi.sym`, `ffi.call_*`, raw memory | full module requires `--trusted` |
| `gui` | Liquid Glass UI components | desktop preview + Android |
| `llm` | Local LLM loading (GGUF), tokenization, NH-2-Coder | |
| `concurrent` | Actor model + channels | |
| `compute` | Compute context (accelerator abstraction) | |
| `sdl` | SDL2 windowed backend | via `--sdl` / `gui` command |

## ✦ CLI: `forgevm`

```bash
forgevm script.fge              # Run .fge (or .fclass)
forgevm -e 'print(1+1)'         # Execute string
forgevm                         # REPL
forgevm --trusted script.fge    # Enable FFI + os shell powers
forgevm compile app.fge app.fclass
forgevm build                   # Build project from ForgeLists.txt
forgevm build --shared          # Export as C-ABI shared library
forgevm lib new mylib --cpp     # Scaffold a native C++ library
forgevm java app.fge            # Transpile to Java / runnable .jar
forgevm android new ./myapp --package com.example.myapp
forgevm android build/install/run ./myapp
forgevm --sdl script.fge        # Windowed SDL2 UI
forgevm --ide                   # Forge IDE
forgevm --gc-stats script.fge   # GC statistics after run
```

## ✦ Build Projects with the `Forgefile` (one file, one binary)

The single hand-written project file — every component inside it, compiled into ONE single-file binary:

```forge
# Forgefile - every component written by hand, in link order
project(MyApp)
version(1.0.0)
arch(x64)
type(executable)          # executable | shared
output(MyApp)
builddir(build-releases)  # artifacts + cache/ + make.ts

component(src/main.fge)   # hand-written components
component(src/util.fge)

module(io)                # runtime modules
module(gui)
libsdir(/usr/forge/libs)  # native libs scanned here
uselib(mylib.so)
```

```bash
forgevm build             # builds Forgefile (legacy ForgeLists.txt still accepted)
forgevm build --shared    # output as C-ABI shared library
```

`forgevm build` paints the console with `::` phases, `=>` checks, `[ NN%]` progress (`src/main.fge -> obj/main.fclass`), and records every command with args into `build-releases/make.ts` (plus `cache/config.ts`). Pass `-v` to see all compiler args live.

**Sessions** — one file, four pipelines. The file REALIZES what to run:

```forge
# decompile: one exact binary -> real sources -> generated project -> compiled binary
session(decompile)
input(rtapp)
destination(decomp)
# decomp/src/*.fge (bit-exact) + decomp/ForgeLists.txt -> auto compile
# -> binary at decomp/build-releases/rtapp
```
`session(c2forge)` translates C components to `.fge` + generates a project list; `session(forge2c)` exports components to C; `session(compile)` is the default build. Every forge binary embeds its exact sources, so decompiling a forge binary recovers the original files — full binary→source→binary round trip.

```text
:: running Forge build
:: configuring {app: MyApp -> build-releases}
=> checking 2 component(s)..
=> component src/main.fge
-- components resolved! -- {0.012s}
:: compiling components
[  50%] src/main.fge -> build-releases/obj/main.fclass
[ 100%] build-releases/runner.cpp -> MyApp
-- build complete! -- {1.7s}
```

`cfmake` scaffolds/configures/builds this automatically: creates `build-releases/` with its cache, pre-generates the `Forgefile` (finding the forge binaries and libs itself), then runs the compiler for you:

```bash
cfmake init MyApp     # scaffold src/ + Forgefile
cfmake configure      # :: => -- ==> painting, cache, auto build
cfmake build          # rerun the compiler directly
```

## ✦ Export Forge to C / C++

```cpp
// app.cpp — generated header from `forgevm build --shared`
#include "libmathforge.so.h"
#include <cstdio>

int main() {
    if (forge_lib_init() != 0) return 1;
    printf("add(2,3) = %lld\n", (long long)forge_lib_call_int2("add", 2, 3));
    printf("greet    = %s\n", forge_lib_call_str1("greet", "C++"));
    return 0;
}
```

```bash
c++ -std=c++17 app.cpp ./libmathforge.so -o app && ./app
```

## ✦ Native C/C++ Libraries + FFI

```bash
forgevm lib new mylib --cpp   # Real C++ (classes, std::vector, templates) behind extern "C"
cd mylib && sh build.sh
forgevm --trusted main.fge    # FFI always requires trusted mode
```

```forge
import "ffi"
let lib = ffi.open("./mylib.so")
let add = ffi.sym(lib, "add")
print(ffi.call_int2(add, 2, 3))
ffi.close(lib)
```

FFI calls are checked **lazily at call time** — `--trusted` must actually be on the invocation that loads the script.

## ✦ Android Apps with Liquid Glass UI

```forge
// liquid_demo.fge
import "gui"
gui.create_card("card1", 10, 10, 300, 200)
gui.set_glass("card1", 0.8, 1.2, 0.9)   // blur, saturation, opacity
gui.set_tint("card1", "#AARRGGBB")

gui.create_button("btn1", "Click Me", 50, 150)
gui.on_click("btn1", fn() {
    print("Button clicked!")
})
```

| Widget | Function |
| :-- | :-- |
| bottombar | `gui.create_bottombar(id, x, y, w, h)` |
| segmented | `gui.create_segmented(id, items, x, y, w, h)` |
| card | `gui.create_card(id, x, y, w, h)` |
| stepper | `gui.create_stepper(id, x, y, w, h)` |
| magnifier | `gui.create_magnifier(id, x, y, w, h)` |
| search | `gui.create_search(id, x, y, w, h)` |
| draggertabs | `gui.create_draggertabs(id, tabs, x, y, w, h)` |

```bash
forgevm android build ./myapp     # Android (Kyant0 Liquid Glass 1.0.6)
forgevm android install ./myapp   # requires ADB
forgevm android run ./myapp
```

## ✦ Developer Tooling

| Tool | Description |
| :-- | :-- |
| **`forgevm`** | Primary runtime — run, compile, build, lib, android, java, export |
| **`forge`** | Lightweight interpreter / REPL |
| **`forge-lsp`** | Language Server Protocol (`--stdio`): completion, hover, go-to-definition, references, diagnostics |
| **`forge-debug`** | Bytecode debugger with breakpoints, stepping, watch expressions |
| **`c2forge`** | C/C++ → Forge translator (rough — manual review required) |
| **`forge2c`** | Forge → C/C++ translator (rough — manual review required) |
| **`forge-rebuild`** | Rebuild engine: `ios2android`, `java2forge`, `cpp2forge`, `swift2forge`, `kotlin2forge`, `bundle`, `analyze` |
| **`cfmake`** | CMake-like project configurator (init / configure / wizard) |
| **`forge-decompiler`** | `.fclass` decompiler with binary hashing |
| **`forge-fclass-decompiler`** | Structural decompiler: `binary/`, `src/`, `methods/*.dis`, `constant_pool.dis`, `HASH_INDEX.txt` |
| **`editors/vscode/`** | VS Code extension |

## ✦ Transpilers & Rebuild Engine

```bash
# C/C++ <-> Forge (heuristic translators — expect to hand-tune output)
c2forge app.c app.fge
forge2c app.fge app.c

# Cross-platform rebuild
forge-rebuild ios2android ./MyiOSApp.xcodeproj
forge-rebuild bundle app.fge --os win --arch x64      # .exe
forge-rebuild bundle app.fge --os darwin --arch arm64 # .app bundle
forge-rebuild bundle app.fge --os linux --arch x64    # ELF
```

## ✦ Documentation

- **[LLMS.md](LLMS.md)** — Verified language reference for humans and AI agents
- **[CODE_STYLE.md](CODE_STYLE.md)** — Style guide for `.fge` and the C++ toolchain
- **[docs/SDK_GUIDE.md](docs/SDK_GUIDE.md)** — Practical SDK guide (FFI, exports, CLI, Android)
- **[GUIDE.md](GUIDE.md)** — Extended user guide
- **[README_WINDOWS.md](README_WINDOWS.md)** — Building on Windows (incl. Windows 10 IoT LTSC 2021)

## ✦ Security Model

- **Default mode is sandboxed-ish:** `ffi.*`, `os.execute`, and `os.capture` refuse to run unless you pass `--trusted`.
- Argument type mismatches surface as clean `Type error:` runtime messages, never crashes.
- `json.parse` guards against extreme nesting; the parser caps expression nesting depth; the VM guards its value stack.
- `--trusted` is **all-or-nothing**: give it only to code you wrote or reviewed.

## ✦ Build Targets

```bash
cmake --build build-release -j4     # Everything
cmake --build build-release --target forgevm forge-lsp -j4
cmake --build build-release --target forge-debug -j4
```

## ✦ Notes

- Don't stage `build-release/` or `brag-output/` in commits (already git-ignored).
- CI (`.github/workflows/ci.yml`) builds Linux/macOS/Windows and runs smoke tests.
- `.ua/knowledge-graph.json` holds the project knowledge graph.

<div align="center">

**[GitHub](https://github.com/Nohat1337/FORGE)** • **Built with C++17**

</div>
