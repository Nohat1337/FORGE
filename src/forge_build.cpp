#include "forge_build.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace fs = std::filesystem;

namespace forge::build {

static bool verbose_ = false;

static void log(const std::string& msg) {
    if (verbose_) std::cout << "  " << msg << "\n";
}

static void err(const std::string& msg) {
    std::cerr << "Error: " << msg << "\n";
}

// ============================================================
// ForgeLists.txt Parser
// ============================================================

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::vector<std::string> splitArgs(const std::string& args) {
    std::vector<std::string> result;
    std::string current;
    bool inQuotes = false;
    for (char c : args) {
        if (c == '"') { inQuotes = !inQuotes; }
        else if (c == ',' && !inQuotes) {
            auto t = trim(current);
            if (!t.empty()) result.push_back(t);
            current.clear();
        } else {
            current += c;
        }
    }
    auto t = trim(current);
    if (!t.empty()) result.push_back(t);
    return result;
}

static bool parseForgeLists(const std::string& path, BuildConfig& config) {
    std::ifstream file(path);
    if (!file.is_open()) {
        err("Cannot open " + path);
        return false;
    }

    std::string line;
    int lineNum = 0;
    while (std::getline(file, line)) {
        lineNum++;
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        size_t parenOpen = line.find('(');
        size_t parenClose = line.rfind(')');
        if (parenOpen == std::string::npos || parenClose == std::string::npos) {
            err("Syntax error at line " + std::to_string(lineNum) + ": " + line);
            return false;
        }

        std::string cmd = trim(line.substr(0, parenOpen));
        std::string args = trim(line.substr(parenOpen + 1, parenClose - parenOpen - 1));

        if (cmd == "project") {
            config.projectName = args;
        } else if (cmd == "version") {
            config.version = args;
        } else if (cmd == "source") {
            config.sourceDirs = splitArgs(args);
        } else if (cmd == "output") {
            config.outputName = args;
        } else if (cmd == "arch") {
            config.arch = args;
        } else if (cmd == "compiler") {
            config.compiler = args;
        } else if (cmd == "builddir") {
            config.buildDir = args;
        } else if (cmd == "flags") {
            auto flags = splitArgs(args);
            config.extraFlags.insert(config.extraFlags.end(), flags.begin(), flags.end());
        } else if (cmd == "release") {
            config.release = (args == "true" || args == "1" || args == "ON");
        } else if (cmd == "shared") {
            config.shared = (args == "true" || args == "1" || args == "ON");
        } else if (cmd == "type") {
            config.shared = (args == "shared" || args == "library");
        } else if (cmd == "file") {
            config.sourceFiles.push_back(args);
        } else {
            err("Unknown command '" + cmd + "' at line " + std::to_string(lineNum));
            return false;
        }
    }
    return true;
}

// ============================================================
// Source File Discovery
// ============================================================

static std::vector<std::string> findSourceFiles(const std::vector<std::string>& dirs) {
    std::vector<std::string> files;
    for (auto& dir : dirs) {
        if (!fs::exists(dir)) {
            err("Source directory not found: " + dir);
            continue;
        }
        for (auto& entry : fs::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".fge") {
                files.push_back(entry.path().string());
            }
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

// ============================================================
// Core Library Discovery
// ============================================================

// Locates libforge_core.a: the build directory baked in at compile time
// first, then the usual install locations relative to the Forge root.
static std::string findCoreLibrary(const std::string& forgeRoot) {
    std::vector<std::string> candidates;
#ifdef FORGE_CORE_LIB
    candidates.push_back(FORGE_CORE_LIB);
#endif
    candidates.push_back(forgeRoot + "/build-release/libforge_core.a");
    candidates.push_back(forgeRoot + "/build/libforge_core.a");
    candidates.push_back(forgeRoot + "/lib/libforge_core.a");
    candidates.push_back(forgeRoot + "/build-release/lib/libforge_core.a");
    candidates.push_back(forgeRoot + "/build/lib/libforge_core.a");
    for (const auto& c : candidates) {
        if (fs::exists(c)) return c;
    }
    return "";
}



static std::string escapeForCString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 10);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\0': out += "\\0"; break;
            default:   out += c; break;
        }
    }
    return out;
}

// ============================================================
// Standalone Runner Generator (source-embedding approach)
// ============================================================

static std::string findForgeRoot() {
    // Use compile-time path if available
#ifdef FORGE_ROOT_DIR
    if (fs::exists(fs::path(FORGE_ROOT_DIR) / "src" / "fvm" / "runtime.cpp")) {
        return FORGE_ROOT_DIR;
    }
#endif
    // Try to find relative to current directory
    const char* candidates[] = {
        ".",
        "..",
    };
    for (auto c : candidates) {
        if (fs::exists(fs::path(c) / "src" / "fvm" / "runtime.cpp")) {
            return fs::absolute(c).string();
        }
    }
    // Try relative to /proc/self/exe (the running binary)
    char exePath[4096];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len > 0) {
        exePath[len] = '\0';
        fs::path exeDir = fs::path(exePath).parent_path();
        // Check if forgevm is next to src/fvm/
        fs::path candidate = exeDir.parent_path();
        if (fs::exists(candidate / "src" / "fvm" / "runtime.cpp")) {
            return candidate.string();
        }
    }
    return ".";
}

// Sources that make up the Forge runtime. Used for the self-contained
// fallback (when libforge_core.a is not available) so the generated runner
// still links.
static const char* CORE_UNITS[] = {
    "fvm/runtime.cpp", "fvm/classfile.cpp", "fvm/sdl2_ui.cpp",
    "fvm/liquid_glass.cpp", "fvm/gui_kit.cpp", "fvm/sdl_fvm_ui.cpp",
    "fvm/forge_ide.cpp", "fvm/ffi.cpp", "fvm/actors.cpp", "fvm/llm.cpp",
    "fvm/gguf_loader.cpp", "fvm/faw.cpp", "fvm/forge_compute.cpp",
    "fvm/hf_converter.cpp", "fvm/forge_export.cpp",
    "lexer.cpp", "parser.cpp", "value.cpp", "chunk.cpp", "compiler.cpp",
    "optimizer.cpp",
};

static bool generateRunner(const BuildConfig& config, const std::string& concatenatedSource,
                           const std::string& runnerPath, bool shared, bool selfContained) {
    std::ofstream out(runnerPath);
    if (!out.is_open()) {
        err("Cannot create runner file: " + runnerPath);
        return false;
    }

    std::string escaped = escapeForCString(concatenatedSource);

    if (shared) {
        // Exported library: the Forge program is embedded and reachable
        // through the stable C ABI in fvm/forge_export.hpp.
        out << R"(// Auto-generated by forge build --shared - do not edit
#include "fvm/forge_export.hpp"
#include <cstring>

static const char* FORGE_SOURCE = ")";
        out << escaped;
        out << R"(";

extern "C" {

// Load and run the embedded Forge program.
int forge_lib_init(void) {
    return forge_lib_load(FORGE_SOURCE);
}

// The embedded Forge source (read-only).
const char* forge_lib_source(void) {
    return FORGE_SOURCE;
}

} // extern "C"
)";
        out.close();
        return true;
    }

    out << "// Auto-generated by forge build - do not edit\n";
    out << "#include \"fvm/runtime.hpp\"\n";
    if (selfContained) {
        for (const char* unit : CORE_UNITS) {
            out << "#include \"" << unit << "\"\n";
        }
    }
    out << R"(#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib>

static const char* FORGE_SOURCE = ")";
    out << escaped;
    out << R"(";

int main(int argc, char** argv) {
    try {
        forge::fvm::ForgeVM vm;
        vm.setArgs(argc, argv);
        if (!vm.interpretSource(FORGE_SOURCE, "main.fge")) {
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
)";

    out.close();
    return true;
}

// Writes the public header for an exported shared library.
static bool generateExportHeader(const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open()) {
        err("Cannot create header file: " + path);
        return false;
    }
    out << R"(// Auto-generated by forge build --shared - do not edit
//
// Public C ABI of this Forge library. Link against the .so and call the
// exported Forge functions directly from C or C++.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Load and run the embedded Forge program.
int forge_lib_init(void);

// Re-run the embedded Forge program.
int forge_lib_run(void);

// Call a top-level Forge function by name. The suffix is the argument count.
int64_t  forge_lib_call_int0(const char* fn);
int64_t  forge_lib_call_int1(const char* fn, int64_t a);
int64_t  forge_lib_call_int2(const char* fn, int64_t a, int64_t b);

double   forge_lib_call_float0(const char* fn);
double   forge_lib_call_float1(const char* fn, double a);
double   forge_lib_call_float2(const char* fn, double a, double b);

const char* forge_lib_call_str0(const char* fn);
const char* forge_lib_call_str1(const char* fn, const char* a);

// Select the function used by the *_selected_* helpers.
int64_t forge_lib_select(const char* fn);

int64_t forge_lib_call_selected_int0(void);
int64_t forge_lib_call_selected_int1(int64_t a);
int64_t forge_lib_call_selected_int2(int64_t a, int64_t b);

double forge_lib_call_selected_float0(void);
double forge_lib_call_selected_float1(double a);
double forge_lib_call_selected_float2(double a, double b);

const char* forge_lib_call_selected_str0(void);
const char* forge_lib_call_selected_str1(const char* a);

// Last error message ("" when there is none).
const char* forge_lib_last_error(void);

// Release a string returned by forge_lib_call_str().
void forge_lib_free_string(const char* s);

// The embedded Forge source.
const char* forge_lib_source(void);

// Version of the embedded Forge runtime.
const char* forge_lib_version(void);

#ifdef __cplusplus
}
#endif
)";
    out.close();
    return true;
}

// ============================================================
// Build Command
// ============================================================

static bool runCommand(const std::string& cmd) {
    log("Running: " + cmd);
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        err("Command failed with exit code " + std::to_string(ret));
    }
    return ret == 0;
}

static std::string getArchFlag(const std::string& arch) {
    if (arch == "x86" || arch == "i386" || arch == "i686") return "-m32";
    if (arch == "x64" || arch == "x86_64" || arch == "amd64") return "-m64";
    if (arch == "arm64" || arch == "aarch64") return "";
    return "-m64";
}

int build_main(int argc, char* argv[]) {
    BuildConfig config;
    std::string configPath = "ForgeLists.txt";
    bool cleanFirst = false;

    for (int i = 0; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--verbose" || arg == "-v") { verbose_ = true; config.verbose = true; }
        else if (arg == "--clean") { cleanFirst = true; }
        else if (arg == "--debug") { config.release = false; }
        else if (arg == "--shared" || arg == "--library") { config.shared = true; }
        else if (arg == "--arch" && i + 1 < argc) { config.arch = argv[++i]; }
        else if (arg == "--output" && i + 1 < argc) { config.outputName = argv[++i]; }
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: forge build [options]\n\n";
            std::cout << "Builds a standalone binary from ForgeLists.txt project configuration.\n\n";
            std::cout << "Options:\n";
            std::cout << "  -v, --verbose     Verbose output\n";
            std::cout << "  --clean           Clean build directory first\n";
            std::cout << "  --debug           Debug build (no optimization)\n";
            std::cout << "  --arch <arch>     Target architecture (x86, x64, arm64)\n";
            std::cout << "  --output <name>   Output binary name\n";
            std::cout << "  --shared          Build a shared library (.so) exporting a C ABI\n";
            std::cout << "  -h, --help        Show this help\n";
            std::cout << "\nForgeLists.txt format:\n";
            std::cout << "  project(MyApp)\n";
            std::cout << "  version(1.0.0)\n";
            std::cout << "  source(src)\n";
            std::cout << "  output(myapp)\n";
            std::cout << "  shared(true)      Export as a shared library\n";
            std::cout << "  arch(x64)\n";
            return 0;
        }
        else if (!arg.empty() && arg[0] != '-' && i == 0) { configPath = arg; }
    }

    // Parse ForgeLists.txt
    if (!fs::exists(configPath)) {
        err("Build configuration not found: " + configPath);
        err("Create a ForgeLists.txt with:\n  project(MyProject)\n  source(src)\n  output(myapp)");
        return 1;
    }

    if (!parseForgeLists(configPath, config)) return 1;

    std::cout << "Forge Build System v1.0\n";
    std::cout << "Project: " << config.projectName << " v" << config.version << "\n";
    std::cout << "Arch: " << config.arch << "\n";

    // Find source files
    std::vector<std::string> sourceFiles;
    if (!config.sourceFiles.empty()) {
        // Use explicit file list
        for (auto& f : config.sourceFiles) {
            if (!fs::exists(f)) {
                err("Source file not found: " + f);
                return 1;
            }
            sourceFiles.push_back(f);
        }
    } else {
        sourceFiles = findSourceFiles(config.sourceDirs);
    }
    if (sourceFiles.empty()) {
        err("No .fge source files found");
        return 1;
    }

    std::cout << "Found " << sourceFiles.size() << " source file(s):\n";
    for (auto& f : sourceFiles) log("  " + f);

    // Concatenate all source files
    std::string allSource;
    for (auto& file : sourceFiles) {
        std::ifstream ifs(file);
        if (!ifs.is_open()) {
            err("Cannot open: " + file);
            return 1;
        }
        std::stringstream buf;
        buf << ifs.rdbuf();
        allSource += buf.str();
        allSource += "\n";
        log("Read " + std::to_string(buf.str().size()) + " bytes from " + file);
    }

    std::cout << "Total source: " << allSource.size() << " bytes\n";

    // Create build directory
    if (cleanFirst && fs::exists(config.buildDir)) {
        log("Cleaning build directory...");
        fs::remove_all(config.buildDir);
    }
    fs::create_directories(config.buildDir);

    // Find forge root for include paths
    std::string forgeRoot = findForgeRoot();

    // Prefer linking the prebuilt core library (fast); fall back to a
    // self-contained runner that compiles the runtime inline.
    std::string coreLib = findCoreLibrary(forgeRoot);
    bool selfContained = coreLib.empty();
    if (selfContained) {
        log("libforge_core.a not found - building a self-contained runner");
    }

    // Generate runner
    std::string runnerPath = config.buildDir + "/runner.cpp";
    if (!generateRunner(config, allSource, runnerPath, config.shared, selfContained)) return 1;
    log("Generated runner: " + runnerPath);

    // Compile with g++
    std::string outputBin = config.outputName;
    std::string archFlag = getArchFlag(config.arch);
    std::string optFlag = config.release ? "-O2" : "-O0 -g";
    std::string stdFlag = "-std=c++17";
    std::string includeFlag = "-I" + forgeRoot + "/src";

    std::string compileCmd = config.compiler + " " + optFlag + " " + archFlag + " " + stdFlag + " " + includeFlag;
    compileCmd += " " + std::string(FORGE_SDL2_INCLUDE_FLAGS);
    if (config.shared) {
        compileCmd += " -shared -fPIC";
        outputBin = config.outputName;
        if (outputBin.find(".so") == std::string::npos) outputBin += ".so";
    }
    compileCmd += " -o " + outputBin + " " + runnerPath;

    if (!selfContained) compileCmd += " " + coreLib;

    for (auto& f : config.extraFlags) compileCmd += " " + f;
    compileCmd += " -lpthread -ldl";
    compileCmd += " " + std::string(FORGE_SDL2_LINK_FLAGS);

    std::cout << "\nCompiling " << (config.shared ? "shared library" : "standalone binary") << "...\n";
    if (!runCommand(compileCmd)) {
        err("Compilation failed");
        return 1;
    }

    if (config.shared) {
        std::string headerPath = outputBin + ".h";
        if (generateExportHeader(headerPath)) {
            std::cout << "  header: " << headerPath << "\n";
        }
    }

    std::cout << "\nBuild successful: " << outputBin << "\n";
    if (config.shared) {
        std::cout << "Call it from C/C++ with:\n"
                  << "  #include \"" << outputBin << ".h\"\n"
                  << "  c++ app.cpp " << outputBin << " -o app\n"
                  << "  (or from Forge: import \"ffi\" -> ffi.open(\"./" << outputBin << "\"))\n";
    } else {
        std::cout << "Run with: ./" << outputBin << "\n";
    }
    return 0;
}

} // namespace forge::build
