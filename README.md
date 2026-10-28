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

*Compile to native binaries • Export to C/C++ shared libraries • Run on FVM • Build Android apps*

[![Language](https://img.shields.io/badge/Language-Forge-ff6b35?style=for-the-badge)](https://github.com/Nohat1337/FORGE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows%20x64-4a9eff?style=for-the-badge)](https://github.com/Nohat1337/FORGE)
[![License](https://img.shields.io/badge/License-MIT-2ed573?style=for-the-badge)](LICENSE)

</div>

## ✦ Overview

Forge is a full-stack language toolchain featuring a hand-written lexer, parser, compiler, and the **Forge Virtual Machine (FVM)**. Write in `.fge`, run instantly with `forgevm`, compile to standalone executables or export as C-compatible shared libraries callable from C/C++ or back from Forge via FFI.

## ✦ Quick Start

```bash
# Clone & build
git clone https://github.com/Nohat1337/FORGE.git
cd FORGE
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target forgevm -j4

# Run a script
./build-release/forgevm examples/simple.fge

# Drop into the REPL
./build-release/forgevm
```

## ✦ Build Projects with `ForgeLists.txt`

```text
project(MyApp)
version(1.0.0)
source(src)
output(myapp)
arch(x64)
```

```bash
forgevm build           # Standalone native binary
forgevm build --shared  # Export as shared library with C ABI
```

## ✦ Export Forge to C / C++

Forge can compile directly into loadable shared objects with a stable C ABI. Call them from C++ or re-import them from any Forge script via FFI.

```cpp
// app.cpp
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

## ✦ Native C/C++ Libraries

```bash
forgevm lib new mylib --cpp   # Real C++ (classes, std::vector, templates) behind extern "C"
cd mylib && sh build.sh
forgevm --trusted main.fge
```

## ✦ Android Apps with Liquid Glass UI

```bash
forgevm android new ./myapp --package com.example.myapp
forgevm android build ./myapp
forgevm android install ./myapp
forgevm android run ./myapp
```

## ✦ Developer Tooling

| Tool | Description |
| :-- | :-- |
| **`forgevm`** | Primary runtime — execute, compile, build, FFI, lib, android, export |
| **`forge`** | Lightweight interpreter |
| **`forge-lsp`** | Language Server Protocol (completion, hover, diagnostics, go-to-def, references) |
| **`forge-decompiler`** | Pure C++ `.fclass` decompiler with binary hashing & structured reconstruction |
| **`forge-fclass-decompiler`** | Full structural decompiler with hash-matched output tree |
| **`editors/vscode/`** | Rich VS Code extension with syntax highlighting & integrated commands |

## ✦ Binary Decompilation

```bash
# Decompile single file or entire directory
forge-decompiler path/to/file.fclass ./decompiled
forge-fclass-decompiler ./binaries ./structured_out
```

Output mirrors the binary structure with `binary/` (original + `META.info` containing `djb2_hash` & `structural_hash`), `src/` (reconstructed `.fge`), `methods/` (`.dis` disassembly + raw `.bytecode`), `constant_pool.dis`, and `HASH_INDEX.txt`.

## ✦ Documentation

- **[docs/SDK_GUIDE.md](docs/SDK_GUIDE.md)** — Practical SDK guide (verified language subset, FFI, exports, CLI, Android)
- **[GUIDE.md](GUIDE.md)** — Extended user guide
- **[README_WINDOWS.md](README_WINDOWS.md)** — Building on Windows 10 IoT Enterprise LTSC 2021

## ✦ Build Targets

```bash
cmake --build build-release --target forgevm -j4      # VM + builder + FFI
cmake --build build-release --target forge-lsp -j4     # Language server
cmake --build build-release --target forge_core -j4    # Embeddable runtime library
```

## ✦ Notes

- Use `--trusted` for FFI and native interop
- Leave `build-release/` and `brag-output/` unstaged when committing
- On Windows, use `build_windows.bat` or `CMakeLists.windows.txt`

<div align="center">

**[GitHub](https://github.com/Nohat1337/FORGE)** • **Built with C++17**

</div>

## ✦ Android Liquid Glass SDK

Forge has built-in Android Liquid Glass UI support powered by Kyant0 backdrop (v1.0.6).

### Setup
```bash
forgevm android new ./myapp --package com.example.myapp
cd myapp
```

### Creating Glass UI Elements
```forge
// glass_demo.fge
gui.create_bottombar("tabbar", 0, 500, 400, 80)
gui.set_glass("tabbar", 0.6, 1.1, 0.85)
gui.set_tint("tabbar", "#40FFFFFF")

gui.create_button("action_btn", "Launch", 150, 200)
gui.on_click("action_btn", fn() {
  print("Action triggered")
})

gui.create_magnifier("lens", 100, 300, 80, 80)
gui.set_glass("lens", 1.0, 1.3, 0.7)
```

### Available Widgets
| Widget | Function | Notes |
| :-- | :-- | :-- |
| `bottombar` | `gui.create_bottombar(id, x,y,w,h)` | Liquid glass bottom navigation |
| `segmented` | `gui.create_segmented(id, items, x,y,w,h)` | Segmented control with glass |
| `card` | `gui.create_card(id, x,y,w,h)` | Glass card container |
| `stepper` | `gui.create_stepper(id, x,y,w,h)` | +/- stepper |
| `magnifier` | `gui.create_magnifier(id, x,y,w,h)` | Liquid lens effect |
| `search` | `gui.create_search(id, x,y,w,h)` | Glass search bar |
| `draggertabs` | `gui.create_draggertabs(id, tabs, x,y,w,h)` | Draggable tabs |

### Glass Effects
```forge
// Apply frosted glass effect
gui.set_glass(widget_id, blur, saturation, opacity)
// blur: 0.0-1.5, saturation: 0.8-1.5, opacity: 0.0-1.0

// Apply tint color
gui.set_tint(widget_id, "#RRGGBB")      // solid tint
gui.set_tint(widget_id, "#AARRGGBB")    // with alpha
```

### Button Callbacks
```forge
gui.create_button("btn", "Press", 50, 50)
gui.on_click("btn", fn() {
  // Handle click
  gui.set_tint("btn", "#FF6B35")
})
```

### Build & Run
```bash
forgevm android build ./myapp
forgevm android install ./myapp  # requires ADB
forgevm android run ./myapp
```

## ✦ Transpilers & Rebuild Engine

```bash
# C/C++ <-> Forge
c2forge app.c app.fge
forge2c app.fge app.c

# Cross-platform rebuild
forge-rebuild ios2android ./MyiOSApp.xcodeproj
forge-rebuild bundle app.fge --os win --arch x64    # .exe
forge-rebuild bundle app.fge --os darwin --arch arm64 # .app/.dmg
forge-rebuild bundle app.fge --os linux --arch x64   # raw ELF
```
