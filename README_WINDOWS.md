# Building Forge on Windows 10 IoT Enterprise LTSC 2021 (x64)

## Prerequisites
- Visual Studio 2022 with "Desktop development with C++" workload
- CMake 3.20+
- (Optional) [vcpkg](https://github.com/microsoft/vcpkg) for SDL2: `vcpkg install sdl2:x64-windows`

## Quick build
```bat
build_windows.bat
```
or
```powershell
.\build_windows.ps1
```

## Manual
```bat
set VCPKG_ROOT=C:\path\to\vcpkg
cmake -B build-win -S . -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build build-win --config Release --parallel
```

## Binaries (build-win\Release\)
- `forgevm.exe` - VM/runtime, FFI, lib/android/build/export
- `forge.exe` - interpreter
- `forge-lsp.exe` - Language Server (use with VS Code extension)
- `forge-decompiler.exe` - C decompiler: parse .fclass, disassemble, hash binaries, structure to src/methods/binary
- `fclass-decompile.exe` - Quick decompiler
- `fclass-structurer.exe` - Structure binaries into source tree with hash matching
- `forge-format.exe`, `forge-lint.exe`, `forge-debug.exe`

## Decompile .fclass on Windows
```bat
forge-decompiler.exe path\to\file.fclass decompiled_out
forge-decompiler.exe C:\binaries decompiled_out
```

Output: `binary\` (original + `META.info` with djb2/structural hashes), `src\`, `methods\` (`.dis`, `.bytecode`), `constant_pool.dis`, `verify.sh`, `HASH_INDEX.txt`

## VS Code extension
Open `editors\vscode\` in VS Code and press F5, or package. Set `forge.languageServerPath` to `forge-lsp.exe` if needed.

## Using our own language to recompile
After structuring, edit files under `src/` and compile with:
```bat
forgevm.exe compile src\main.fge output.fclass
```
Hash matching in `META.info` helps verify binary equivalence.
