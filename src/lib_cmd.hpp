#pragma once

// ============================================================
// forgevm lib - native & Forge library tooling
//
//   forgevm lib new <name>     Scaffold a native library project
//                              (lib.c + main.fge demo + build.sh)
//   forgevm lib build [dir]    Compile the scaffolded library (.so)
//
// Native libraries use the C ABI so they can be written in C,
// C++ (extern "C"), Rust, Zig, Go (cgo), etc. Forge loads them
// at runtime through the `ffi` module.
// ============================================================

namespace forge::libcmd {

int lib_main(int argc, char** argv);

} // namespace forge::libcmd
