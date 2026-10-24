# Forge

Forge is a language + toolchain (lexer/parser/compiler/VM + FVM) with a C API for embedding and exporting Forge programs to native binaries and shared libraries.

## Quick start

```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target forgevm -j4

./build-release/forgevm examples/simple.fge
./build-release/forgevm  # REPL
```

## Build from `ForgeLists.txt`

```text
project(MyApp)
version(1.0.0)
source(src)
output(myapp)
arch(x64)
```

```bash
forgevm build                # standalone binary
forgevm build --shared       # shared library with C ABI
```

## Export Forge to C/C++

A `shared(true)` project generates `libname.so` + `libname.so.h` with a C ABI (`forge_lib_init`, `forge_lib_call_selected_*`, `forge_lib_select`, etc.). C++ programs can call them directly; Forge can load them via `ffi` with `--trusted`.

See [`docs/SDK_GUIDE.md`](docs/SDK_GUIDE.md) for verified syntax, CLI, FFI, native libraries, Android liquid-glass GUI, LSP/VS Code, and `.fclass` decompilation.

## Native libraries (C/C++ interop)

```bash
forgevm lib new mylib --cpp   # scaffold C++ library (classes/templates behind extern "C")
cd mylib && sh build.sh
forgevm --trusted main.fge
```

## Android apps

```bash
forgevm android new ./myapp --package com.example.myapp
forgevm android build ./myapp
forgevm android install ./myapp
forgevm android run ./myapp
```

## Tools

- `forgevm` — FVM runtime/compiler/build/FFI/lib/android
- `forge` — interpreter
- `forge-lsp` — Language Server (completion/hover/diagnostics/definition/references)
- `tools/forge-decompiler` / `tools/fclass-decompile` — `.fclass` decompilation with binary hash matching
- `editors/vscode/` — VS Code extension for Forge

## Repository layout

- `src/` — compiler, VM/FVM runtime, modules, Android/LSP/tooling
- `examples/`, `tests/` — samples and tests
- `docs/SDK_GUIDE.md` — practical SDK documentation
- `GUIDE.md` — broader user guide
- `editors/vscode/` — VS Code extension
- `tools/` — utilities and LSP

## Commit notes

Stage source/docs only; leave `build-release/` (and `brag-output/`) unstaged.
