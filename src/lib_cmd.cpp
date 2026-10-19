#include "lib_cmd.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <cstdlib>

namespace fs = std::filesystem;

namespace forge::libcmd {

namespace {

void writeFileIfMissing(const fs::path& p, const std::string& content) {
    if (fs::exists(p)) {
        std::cout << "  skip (exists): " << p.string() << "\n";
        return;
    }
    std::ofstream f(p);
    f << content;
    std::cout << "  created: " << p.string() << "\n";
}

std::string scaffoldLibC(const std::string& name) {
    return R"LIBC(// )LIBC" + name + R"LIBC( - Forge native library (C ABI)
//
// Any language that emits a C ABI works here: C, C++ (extern "C"),
// Rust, Zig, Go... Forge loads the built .so through `ffi`.
//
// Build: sh build.sh

#include <stdint.h>
#include <stdio.h>
#include <math.h>

// --- integers -------------------------------------------------
int64_t add(int64_t a, int64_t b) { return a + b; }

int64_t fibonacci(int64_t n) {
    int64_t a = 0, b = 1;
    for (int64_t i = 0; i < n; i++) { int64_t t = a + b; a = b; b = t; }
    return a;
}

// --- floats ---------------------------------------------------
double hypotenuse(double a, double b) { return sqrt(a * a + b * b); }

// --- strings --------------------------------------------------
const char* greet(const char* name) {
    static char buf[128];
    snprintf(buf, sizeof(buf), "Hello from native code, %s!", name);
    return buf;
}

// --- buffers --------------------------------------------------
// fills dst with n bytes; returns bytes written
int64_t fill_bytes(uint8_t* dst, int64_t n) {
    for (int64_t i = 0; i < n; i++) dst[i] = (uint8_t)(i * 31 + 7);
    return n;
}
)LIBC";
}

std::string scaffoldMainFge(const std::string& name) {
    return R"FGE(// Demo: call the native ")FGE" + name + R"FGE(" library from Forge.
//
//   sh build.sh                  # builds lib)FGE" + name + R"FGE(.so
//   forgevm --trusted main.fge   # FFI requires trusted mode
import "ffi"

let lib = ffi.open("./lib)FGE" + name + R"FGE(.so")

let p_add  = ffi.sym(lib, "add")
let p_fib  = ffi.sym(lib, "fibonacci")
let p_hyp  = ffi.sym(lib, "hypotenuse")
let p_greet = ffi.sym(lib, "greet")
let p_fill = ffi.sym(lib, "fill_bytes")

print("add(2, 3)          = " + str(ffi.call_int(p_add, 2, 3)))
print("fibonacci(20)      = " + str(ffi.call_int(p_fib, 20)))
print("hypotenuse(3, 4)   = " + str(ffi.call_float(p_hyp, 3.0, 4.0)))
print("greet              = " + ffi.call_str(p_greet, ffi.str_ptr("Forge")))

// raw buffer round-trip through memory
let buf_ptr = ffi.mem_alloc(16)
ffi.call_void(p_fill, buf_ptr, 16)
print("fill_bytes         = " + str(ffi.mem_read(buf_ptr, 8)) + " ...")
ffi.mem_free(buf_ptr)
)FGE";
}

std::string scaffoldBuildSh(const std::string& name) {
    return R"(#!/bin/sh
# Build lib)" + name + R"(.so — add -l flags for any dependencies here
set -e
CC="${CC:-cc}"
$CC -O2 -shared -fPIC lib.c -o "lib)" + name + R"(.so"
echo "Built lib)" + name + R"(.so"
)";
}

std::string scaffoldManifest(const std::string& name) {
    return "{\n"
           "  \"name\": \"" + name + "\",\n"
           "  \"version\": \"0.1.0\",\n"
           "  \"description\": \"Native library for Forge\",\n"
           "  \"entry\": \"lib" + name + ".so\",\n"
           "  \"demo\": \"main.fge\",\n"
           "  \"abi\": \"c\",\n"
           "  \"trusted\": true\n"
           "}\n";
}

} // namespace

int lib_main(int argc, char** argv) {
    std::string cmd = argc > 1 ? argv[1] : "";
    std::string name = argc > 2 ? argv[2] : "";

    if (cmd == "--help" || cmd.empty()) {
        std::cout << "Usage:\n"
                  << "  forgevm lib new <name>     Scaffold a native library project\n"
                  << "  forgevm lib build [dir]    Build it (runs build.sh in dir)\n"
                  << "\nThe generated project uses the C ABI, so the native part can be\n"
                  << "written in C, C++ (extern \"C\"), Rust, Zig or Go. Load it from\n"
                  << "Forge with the ffi module (requires --trusted).\n";
        return 0;
    }

    if (cmd == "new") {
        if (name.empty() || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos) {
            std::cerr << "Error: lib new requires a valid name [A-Za-z0-9_-]\n";
            return 1;
        }
        fs::path dir(name);
        if (fs::exists(dir)) {
            std::cerr << "Error: directory '" << name << "' already exists\n";
            return 1;
        }
        std::error_code ec;
        fs::create_directories(dir, ec);
        if (ec) {
            std::cerr << "Error: cannot create directory: " << ec.message() << "\n";
            return 1;
        }
        std::cout << "Scaffolding native library '" << name << "'...\n";
        writeFileIfMissing(dir / ("lib.c"), scaffoldLibC(name));
        writeFileIfMissing(dir / "build.sh", scaffoldBuildSh(name));
        writeFileIfMissing(dir / "manifest.json", scaffoldManifest(name));
        writeFileIfMissing(dir / "main.fge", scaffoldMainFge(name));
        std::cout << "\nNext steps:\n"
                  << "  cd " << name << "\n"
                  << "  sh build.sh\n"
                  << "  forgevm --trusted main.fge\n";
        return 0;
    }

    if (cmd == "build") {
        fs::path dir = name.empty() ? "." : fs::path(name);
        fs::path script = dir / "build.sh";
        if (!fs::exists(script)) {
            std::cerr << "Error: no build.sh in " << dir.string() << "\n";
            return 1;
        }
        // build.sh references lib.c relative to the library dir, so run it
        // from there (no shell interpolation of the path at all)
        std::error_code ec;
        fs::path old = fs::current_path(ec);
        fs::current_path(dir, ec);
        if (ec) {
            std::cerr << "Error: cannot enter " << dir.string() << ": " << ec.message() << "\n";
            return 1;
        }
        int rc = std::system("sh build.sh");
        fs::current_path(old, ec);
        return rc == 0 ? 0 : 1;
    }

    std::cerr << "Unknown lib command: " << cmd << " (see forgevm lib --help)\n";
    return 1;
}

} // namespace forge::libcmd
