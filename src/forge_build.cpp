#include "forge_build.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <thread>
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
        size_t cpos = line.find("//");
        if (cpos != std::string::npos) line = trim(line.substr(0, cpos));
        if (line.empty()) continue;

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
        } else if (cmd == "author") {
            // informational only
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
        } else if (cmd == "libsdir") {
            config.libsDir = args;
        } else if (cmd == "flags" || cmd == "cflags") {
            auto flags = splitArgs(args);
            config.extraFlags.insert(config.extraFlags.end(), flags.begin(), flags.end());
        } else if (cmd == "release") {
            config.release = (args == "true" || args == "1" || args == "ON");
        } else if (cmd == "shared") {
            config.shared = (args == "true" || args == "1" || args == "ON");
        } else if (cmd == "type") {
            config.shared = (args == "shared" || args == "library");
        } else if (cmd == "file" || cmd == "component") {
            config.sourceFiles.push_back(args);
            if (config.flavor == BuildConfig::Flavor::Legacy) config.flavor = BuildConfig::Flavor::Forgefile;
        } else if (cmd == "module") {
            config.modules.push_back(args);
        } else if (cmd == "uselib") {
            config.nativeLibs.push_back(args);
        } else if (cmd == "forgevm") {
            // explicit path to the forgevm binary that drives per-file compiles
            config.fvmBin = args;
        } else if (cmd == "session") {
            if (args == "compile" || args == "build") config.session = BuildConfig::Session::Compile;
            else if (args == "decompile") config.session = BuildConfig::Session::Decompile;
            else if (args == "c2forge") config.session = BuildConfig::Session::C2Forge;
            else if (args == "forge2c") config.session = BuildConfig::Session::Forge2C;
            else {
                err("Unknown session '" + args + "' (compile|decompile|c2forge|forge2c)");
                return false;
            }
        } else if (cmd == "input") {
            config.inputs.push_back(args);
        } else if (cmd == "destination" || cmd == "dest" || cmd == "outdir") {
            config.destination = args;
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

// ============================================================
// CLI Painting (:: / => / -- / ==> phases, bubble spinner,
// [ NN%] ninja-style progress, .ts build transcripts)
// ============================================================

static bool stdoutIsTty() {
    return isatty(fileno(stdout)) != 0;
}

// Bubble spinner frames: an ASCII bubble, expanding point in the middle.
static const char* BUBBLE_FRAMES[] = {
    "(   )", "( . )", "( o )", "( O )", "( o )", "( . )"
};
static const int BUBBLE_N = 6;

static void bubble(int frames, int msPerFrame = 25) {
    if (!stdoutIsTty()) return;
    for (int i = 0; i < frames; i++) {
        std::cout << "\r" << BUBBLE_FRAMES[i % BUBBLE_N] << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(msPerFrame));
    }
    std::cout << "\r \r" << std::flush;
}

// Ninja/cmake style row head: "[  12%]"
static std::string fmtPct(int pct) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "[%4d%%]", pct);
    return std::string(buf);
}

// ============================================================
// .ts Build Transcripts (make-style generated files)
//
//   [  25%] src/main.fge -> obj/main.fclass { forgevm compile ... }
//   [ 100%] runner.cpp -> MyApp             { g++ -O2 -m64 ... }
//
// Verbose (-v) prints the same args live; otherwise they stay hidden.
// ============================================================

static std::ofstream tsFile_;

static void openTsFile(const std::string& buildDir) {
    if (tsFile_.is_open()) tsFile_.close();
    fs::create_directories(buildDir);
    fs::path p = fs::path(buildDir) / "make.ts";
    tsFile_.open(p.string());
    if (tsFile_.is_open()) tsFile_ << "; Forge make transcript - auto-generated\n";
}

static void appendTsFile(const std::string& from, const std::string& to, const std::string& args, int pct) {
    if (!tsFile_.is_open()) return;
    tsFile_ << fmtPct(pct) << " " << from << " -> " << to;
    if (!args.empty()) tsFile_ << " { " << args << " }";
    tsFile_ << "\n";
    tsFile_.flush();
}

// Phase painters matching the :: => -- ==> convention.
static void paintPhase(const std::string& text) {  // :: one per phase
    std::cout << ":: " << text << "\n" << std::flush;
}

static void paintCheck(const std::string& text) {  // => check
    std::cout << "=> " << text << "\n" << std::flush;
}

static void paintDone(const std::string& text, double secs) {  // -- done! --
    std::cout << "-- " << text << "! -- {" << secs << "s}\n" << std::flush;
}

static void paintLaunch(const std::string& text) {  // ==> launch
    std::cout << "==> " << text << "\n" << std::flush;
}

static std::string readSelfExe() {
    char exePath[4096];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len <= 0) return "";
    exePath[len] = '\0';
    return std::string(exePath);
}

// ============================================================
// Sessions — the single file REALIZEs which pipeline to run:
// compile | decompile | c2forge | forge2c
// (destinations are read from the file, hand-written)
// ============================================================

static const char* SOURCE_MAGIC = "static const char* FORGE_SOURCE = \"";
static const size_t SOURCE_MAGIC_LEN = 33;

static std::string unescapeCString(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); i++) {
        char c = raw[i];
        if (c != '\\' || i + 1 >= raw.size()) { out += c; continue; }
        char next = raw[++i];
        switch (next) {
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '0': out += '\0'; break;
            default: out += next; break;
        }
    }
    return out;
}

static void writeFileBytesNeeded(const std::string& path, const std::string& content) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream f(path);
    if (f.is_open()) f << content;
}

static std::vector<uint8_t> readBinaryFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return {};
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> data((size_t)size);
    if (size > 0) f.read((char*)data.data(), size);
    return data;
}

// Recover the embedded forge source (with component markers) from a
// single-file forge binary. The runner keeps the original sources inside
// FORGE_SOURCE; in .rodata those bytes start at the first metadata marker
// and end at the string literal's NUL terminator.
static bool extractEmbeddedSource(const std::string& binaryPath, std::string& outSource) {
    auto bytes = readBinaryFile(binaryPath);
    if (bytes.empty()) return false;

    static const std::string needle = "// FORGE-BUILD-METADATA: file=";
    std::string data((const char*)bytes.data(), bytes.size());
    size_t pos = data.find(needle);
    if (pos == std::string::npos) return false;

    size_t end = data.find('\0', pos);
    if (end == std::string::npos || end <= pos) return false;

    outSource = data.substr(pos, end - pos);
    // sanity: needs at least one marker (the one we anchored on) and not to
    // swallow an embedded-in-source false copy
    if (outSource.size() < needle.size() + 20) return false;
    return true;
}

// Split a marked source blob back into (component, content) pairs.
static std::vector<std::pair<std::string, std::string>> splitMarkedSource(const std::string& source) {
    std::vector<std::pair<std::string, std::string>> parts;
    static const std::string marker = "// FORGE-BUILD-METADATA: file=";
    size_t search = 0;
    size_t idx;
    while ((idx = source.find(marker, search)) != std::string::npos) {
        size_t lineEnd = source.find('\n', idx);
        if (lineEnd == std::string::npos) break;
        std::string file = trim(source.substr(idx + marker.size(), lineEnd - idx - marker.size()));
        size_t next = source.find(marker, lineEnd + 1);
        size_t pieceEnd = (next == std::string::npos) ? source.size() : next;
        std::string piece = source.substr(lineEnd + 1, pieceEnd - lineEnd - 1);
        parts.emplace_back(file, piece);
        search = pieceEnd;
    }
    return parts;
}

// Find a helper tool (c2forge, forge-fclass-decompiler, ...) near forgevm or PATH.
static std::string findTool(const std::string& name) {
    std::vector<std::string> searchDirs;
    std::string selfExe = readSelfExe();
    if (!selfExe.empty()) {
        fs::path exeDir = fs::path(selfExe).parent_path();
        searchDirs.push_back(exeDir.string());
        searchDirs.push_back((exeDir / "tools").string());
        searchDirs.push_back(exeDir.parent_path().string());
        searchDirs.push_back((exeDir.parent_path() / "tools").string());
    }
    searchDirs.push_back("./build-release/tools");
    searchDirs.push_back("./build-release");
    const char* pathEnv = std::getenv("PATH");
    if (pathEnv) {
        std::stringstream ss(pathEnv);
        std::string d;
        while (std::getline(ss, d, ':')) if (!d.empty()) searchDirs.push_back(d);
    }
    for (auto& d : searchDirs) {
        fs::path cand = fs::path(d) / name;
        if (fs::exists(cand)) return cand.string();
    }
    return "";
}

static std::string sessionTitle(const BuildConfig& c) {
    switch (c.session) {
        case BuildConfig::Session::Decompile: return "decompile";
        case BuildConfig::Session::C2Forge: return "c2forge";
        case BuildConfig::Session::Forge2C: return "forge2c";
        default: return "compile";
    }
}

// ------------------------------------------------------------
// DECOMPILE session: one binary -> real source files ->
// auto-generated ForgeLists.txt with destinations -> compile
// ------------------------------------------------------------
static int runDecompileSession(BuildConfig& config, const std::string& configPath) {
    paintPhase("decompiling {into: " + (config.destination.empty() ? std::string("decompiled") : config.destination) + "}");

    std::vector<std::string> inputs = config.inputs;
    if (inputs.empty()) {
        // no input declared - try the project's own output
        inputs.push_back(config.outputName);
        paintCheck("no input() declared - defaulting to output '" + config.outputName + "'");
    }

    for (auto& in : inputs) {
        std::string inPath = in;
        if (!fs::exists(inPath)) {
            err("input not found: " + in);
            return 1;
        }
        fs::path dest(config.destination.empty() ? "decompiled" : config.destination);
        fs::create_directories(dest);

        std::string ext = fs::path(inPath).extension().string();
        bool isElf = false;
        {
            std::ifstream f(inPath, std::ios::binary);
            char magic[4] = {0};
            f.read(magic, 4);
            isElf = (f.gcount() >= 4 && std::memcmp(magic, "\x7f" "ELF", 4) == 0);
        }

        // ---------------------------------------------------------------
        // 1. Binary with embedded source -> REAL sources (fastfetch style)
        // ---------------------------------------------------------------
        std::string embedded;
        if ((isElf || ext != ".fclass") && extractEmbeddedSource(inPath, embedded)) {
            auto parts = splitMarkedSource(embedded);
            if (parts.empty()) {
                err("no component markers inside embedded source of " + inPath);
                return 1;
            }
            paintCheck("found " + std::to_string(parts.size()) + " component(s) embedded in " + inPath);

            // components are stored relative to the destination folder
            std::vector<std::string> components;
            for (auto& [file, content] : parts) {
                fs::path outPath = dest / file;
                writeFileBytesNeeded(outPath.string(), content);
                paintCheck("decompiled " + inPath + " -> " + outPath.string());
                fs::path rel = fs::relative(outPath, dest);
                components.push_back(rel.string());
            }

            // auto-generated single file for the next session (compile).
            // The binary lands inside destination/build-releases/.
            std::string stem = fs::path(inPath).filename().string();
            std::string binName = fs::path(stem).stem().string();
            std::string projectFile = (dest / "ForgeLists.txt").string();
            std::ostringstream gen;
            gen << "// " << binName << " - FORGE single-file project (decompile session output - hand-tune)\n";
            gen << "session(compile)\n";
            gen << "project(" << binName << ")\n";
            gen << "version(1.0.0)\n";
            gen << "arch(x64)\n";
            gen << "type(executable)\n";
            gen << "output(build-releases/" << binName << ")\n";
            gen << "builddir(build-releases)\n";
            gen << "\n// --- destinations of fge files (decompile session put them here) ---\n";
            for (auto& c : components) gen << "component(" << c << ")\n";
            gen << "\nmodule(io)\nmodule(json)\n";
            writeFileBytesNeeded(projectFile, gen.str());
            paintCheck("generated " + projectFile);
            paintDone("decompile session done", 0.0);

            if (config.autoBuild) {
                paintLaunch("running compiler..");
                std::string buildCmd = (config.fvmBin.empty() ? readSelfExe() : config.fvmBin) + " build 'ForgeLists.txt'";
                paintCheck("compiling project inside " + dest.string());
                fs::path prevCwd = fs::current_path();
                fs::current_path(dest);
                int rc = std::system(buildCmd.c_str());
                fs::current_path(prevCwd);
                if (rc != 0) {
                    err("recompile of decompiled project failed");
                    return 1;
                }
                paintCheck("binary output in " + (dest / "build-releases" / binName).string());
                paintDone("decompile->compile chain done", 0.0);
            }
            return 0;
        }

        // ---------------------------------------------------------------
        // 2. .fclass bytecode (no embedded source) -> structural decompile
        // ---------------------------------------------------------------
        if (ext == ".fclass") {
            std::string tool = findTool("forge-fclass-decompiler");
            if (tool.empty()) {
                err("forge-fclass-decompiler not found next to forgevm");
                return 1;
            }
            std::string outDir = dest.string();
            std::string cmd = tool + " '" + inPath + "' '" + outDir + "'";
            if (std::system(cmd.c_str()) != 0) {
                err("structural decompile failed for " + inPath);
                return 1;
            }
            paintCheck("structural decompile: " + inPath + " -> " + outDir);
            paintCheck("generated " + (fs::path(outDir) / "Forgefile").string() + " (hand-tune)");
            if (config.autoBuild) {
                paintLaunch("running compiler..");
                std::string buildCmd = (config.fvmBin.empty() ? readSelfExe() : config.fvmBin) + " build '" + outDir + "/Forgefile'";
                if (std::system(buildCmd.c_str()) != 0) {
                    err("recompile of decompiled project failed");
                    return 1;
                }
            }
            return 0;
        }

        err(inPath + " is not a forge-built single binary (no embedded source) and not .fclass");
        err("- decompile needs either a forge binary or a .fclass file");
        return 1;
    }
    return 0;
}

// ------------------------------------------------------------
// C2FORGE session: C/C++ components -> .fge -> generated lists -> compile
// ------------------------------------------------------------
static int runC2ForgeSession(BuildConfig& config) {
    std::string dest = config.destination.empty() ? "forged" : config.destination;
    paintPhase("c2forge session {destination: " + dest + "}");

    std::string tool = findTool("c2forge");
    if (tool.empty()) {
        err("c2forge tool not found next to forgevm or in PATH");
        return 1;
    }

    std::vector<std::string> sources = config.inputs;
    for (auto& c : config.sourceFiles) sources.push_back(c);
    if (sources.empty()) {
        err("session(c2forge) needs input(...) or component(...) entries with C/C++ files");
        return 1;
    }

    fs::create_directories(dest);
    std::vector<std::string> fgeFiles;
    for (auto& s : sources) {
        if (!fs::exists(s)) {
            err("input not found: " + s);
            return 1;
        }
        std::string out = (fs::path(dest) / (fs::path(s).stem().string() + ".fge")).string();
        std::string cmd = tool + " '" + s + "' '" + out + "'";
        if (std::system(cmd.c_str()) != 0) {
            err("c2forge failed for " + s);
            return 1;
        }
        paintCheck(s + " -> " + out);
        fgeFiles.push_back(fs::path(out).filename().string());
    }

    // generated single file listing the .fge destinations, ready to compile
    std::string binName = fs::path(fgeFiles.front()).stem().string();
    std::string projectFile = (fs::path(dest) / "ForgeLists.txt").string();
    std::ostringstream gen;
    gen << "// FORGE project transpiled from C (c2forge session output - hand-tune)\n";
    gen << "session(compile)\n";
    gen << "project(" << binName << ")\n";
    gen << "arch(x64)\ntype(executable)\n";
    gen << "output(build-releases/" << binName << ")\n";
    gen << "builddir(build-releases)\n\n";
    gen << "// --- destinations of fge files (c2forge session put them here) ---\n";
    for (auto& f : fgeFiles) gen << "component(" << f << ")\n";
    writeFileBytesNeeded(projectFile, gen.str());
    paintCheck("generated " + projectFile);
    paintDone("c2forge session done", 0.0);

    if (config.autoBuild) {
        paintLaunch("running compiler..");
        std::string buildCmd = (config.fvmBin.empty() ? readSelfExe() : config.fvmBin) + " build 'ForgeLists.txt'";
        fs::path prevCwd = fs::current_path();
        fs::current_path(dest);
        int rc = std::system(buildCmd.c_str());
        fs::current_path(prevCwd);
        if (rc != 0) {
            err("compile of c2forge output failed (translate output may need hand-tuning)");
            return 1;
        }
        paintCheck("binary output in " + (fs::path(dest) / "build-releases" / binName).string());
    }
    return 0;
}

// ------------------------------------------------------------
// FORGE2C session: .fge components -> C files -> destination
// ------------------------------------------------------------
static int runForge2CSession(BuildConfig& config) {
    std::string dest = config.destination.empty() ? "c_out" : config.destination;
    paintPhase("forge2c session {destination: " + dest + "}");

    std::string tool = findTool("forge2c");
    if (tool.empty()) {
        err("forge2c tool not found next to forgevm or in PATH");
        return 1;
    }

    std::vector<std::string> sources = config.inputs;
    for (auto& c : config.sourceFiles) sources.push_back(c);
    if (sources.empty()) {
        err("session(forge2c) needs input(...) or component(...) entries with .fge files");
        return 1;
    }

    fs::create_directories(dest);
    std::vector<std::string> cFiles;
    for (auto& s : sources) {
        if (!fs::exists(s)) {
            err("input not found: " + s);
            return 1;
        }
        std::string out = (fs::path(dest) / (fs::path(s).stem().string() + ".c")).string();
        std::string cmd = tool + " '" + s + "' '" + out + "'";
        if (std::system(cmd.c_str()) != 0) {
            err("forge2c failed for " + s);
            return 1;
        }
        paintCheck(s + " -> " + out);
        cFiles.push_back(out);
    }

    // generated lists documenting the C export - hand-tune to build it
    std::string projectFile = (fs::path(dest) / "ForgeLists.txt").string();
    std::ostringstream gen;
    gen << "// C export of " << config.projectName << " (forge2c session output)\n";
    gen << "// Compile with your C compiler of choice - forge does not build C.\n\n";
    gen << "// --- destinations of C files (forge2c session put them here) ---\n";
    for (auto& f : cFiles) gen << "// " << f << "\n";
    writeFileBytesNeeded(projectFile, gen.str());
    paintCheck("generated " + projectFile);
    paintDone("forge2c session done", 0.0);
    return 0;
}

static int runSession(BuildConfig& config, const std::string& configPath) {
    switch (config.session) {
        case BuildConfig::Session::Decompile: return runDecompileSession(config, configPath);
        case BuildConfig::Session::C2Forge:   return runC2ForgeSession(config);
        case BuildConfig::Session::Forge2C:   return runForge2CSession(config);
        default: return -1;  // compile session falls through to normal build
    }
}

int build_main(int argc, char* argv[]) {
    BuildConfig config;
    std::string configPath;
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
            std::cout << "Usage: forgevm build [Forgefile] [options] -v --clean --debug --shared --arch x64 --output name\n\n";
            std::cout << "Builds ONE single-file binary from a single hand-written project file.\n\n";
            std::cout << "Single-file project format (Forgefile): \n";
            std::cout << "  project(MyApp)          # app name\n";
            std::cout << "  version(1.0.0)\n";
            std::cout << "  type(executable)        # executable | shared\n";
            std::cout << "  arch(x64)\n";
            std::cout << "  output(MyApp)\n";
            std::cout << "\nComponents - every piece hand-written, in link order:\n";
            std::cout << "  component(src/main.fge)\n";
            std::cout << "  component(src/util.fge)\n";
            std::cout << "\nLibraries:\n";
            std::cout << "  module(io) module(json) module(gui)\n";
            std::cout << "  libsdir(/usr/forge/libs)\n";
            std::cout << "  uselib(mylib.so)\n";
            std::cout << "\nBuild output:\n";
            std::cout << "  builddir(build-releases)  # artifacts + cache + make.ts live here\n";
            std::cout << "  release(true)\n";
            std::cout << "  cflags(-O2)\n";
            return 0;
        }
        else if (configPath.empty() && !arg.empty() && arg[0] != '-') { configPath = arg; }
    }

    // Pick the single project file: Forgefile first, then legacy ForgeLists.txt
    if (configPath.empty()) {
        if (fs::exists("Forgefile")) configPath = "Forgefile";
        else configPath = "ForgeLists.txt";
    }

    auto tStart = std::chrono::steady_clock::now();

    paintPhase("running Forge build");

    if (!fs::exists(configPath)) {
        err("Build configuration not found: " + configPath);
        if (configPath == "Forgefile" && fs::exists("ForgeLists.txt")) {
            // fall through to legacy below - ForgeLists.txt still works
            configPath = "ForgeLists.txt";
        } else {
            err("Create a single hand-written project file (Forgefile) with:");
            err("  project(MyApp)\n  component(src/main.fge)\n  output(MyApp)");
            err("(legacy ForgeLists.txt is still accepted)");
            return 1;
        }
    }

    if (!parseForgeLists(configPath, config)) return 1;

    // Single-file project convention: releases + cache live in build-releases/
    if (config.flavor == BuildConfig::Flavor::Forgefile && config.buildDir == "build") {
        config.buildDir = "build-releases";
    }

    // Session dispatch: the file realizes compile/decompile/c2forge/forge2c
    if (config.session != BuildConfig::Session::Compile) {
        paintCheck("session: " + sessionTitle(config));
        paintCheck("destination: " + (config.destination.empty() ? std::string("(session default)") : config.destination));
        paintDone("session realized", std::chrono::duration<double>(std::chrono::steady_clock::now() - tStart).count());
        int rc = runSession(config, configPath);
        if (rc >= 0) return rc;
        // rc < 0: fall through and treat as compile
    }

    // destination() also steers where the compiled binary goes
    if (!config.destination.empty() && fs::path(config.outputName).parent_path().string().empty()) {
        fs::create_directories(config.destination);
        config.outputName = (fs::path(config.destination) / config.outputName).string();
    }

    paintPhase("configuring {app: " + config.projectName + " -> " + config.buildDir + "}");

    // Resolve the fvm binary that drives per-file compilation
    if (config.fvmBin.empty()) config.fvmBin = readSelfExe();
    bool canCompilePerFile = !config.fvmBin.empty() && fs::exists(config.fvmBin);
    if (!canCompilePerFile) {
        paintCheck("no fvm compiler found - per-file .fclass steps skipped");
    }

    // ------------------------------------------------------------
    // Components (every piece hand-written in the single file)
    // ------------------------------------------------------------
    std::vector<std::string> sourceFiles;
    if (!config.sourceFiles.empty()) {
        for (auto& f : config.sourceFiles) {
            if (!fs::exists(f)) {
                err("Component not found: " + f);
                err("All components must exist - write them by hand in " + configPath);
                return 1;
            }
            sourceFiles.push_back(f);
        }
    } else if (config.flavor == BuildConfig::Flavor::Forgefile) {
        err("Forgefile with no component(...) entries");
        err("Hand-write every component in " + configPath + ":");
        err("  component(src/main.fge)");
        return 1;
    } else {
        sourceFiles = findSourceFiles(config.sourceDirs);
    }
    if (sourceFiles.empty()) {
        err("No .fge source files found");
        return 1;
    }

    paintCheck("checking " + std::to_string(sourceFiles.size()) + " component(s)..");
    for (auto& f : sourceFiles) paintCheck("component " + f);

    paintDone("components resolved", std::chrono::duration<double>(std::chrono::steady_clock::now() - tStart).count());

    // ------------------------------------------------------------
    // Build directory + transcript (.ts files)
    // ------------------------------------------------------------
    if (cleanFirst && fs::exists(config.buildDir)) {
        log("Cleaning build directory...");
        fs::remove_all(config.buildDir);
    }
    fs::create_directories(config.buildDir);
    fs::create_directories(fs::path(config.buildDir) / "cache");
    fs::path objDir = fs::path(config.buildDir) / "obj";
    fs::create_directories(objDir);
    openTsFile(config.buildDir);

    // Cache: record the config resolved in this run
    {
        std::ofstream cacheTs(fs::path(config.buildDir) / "cache" / "config.ts");
        if (cacheTs.is_open()) {
            cacheTs << "; forge build cache - generated\n";
            cacheTs << "config=" << configPath << "\n";
            cacheTs << "project=" << config.projectName << "\n";
            cacheTs << "version=" << config.version << "\n";
            cacheTs << "output=" << config.outputName << "\n";
            cacheTs << "arch=" << config.arch << "\n";
            cacheTs << "shared=" << (config.shared ? "true" : "false") << "\n";
            for (auto& m : config.modules) cacheTs << "module=" << m << "\n";
            for (auto& l : config.nativeLibs) cacheTs << "uselib=" << l << "\n";
            cacheTs << "threads=1\n";
        }
    }

    // Concatenate all sources (embedded into the single binary)
    // Each component is fenced with a metadata marker so a decompile session
    // can recover the exact original files from the single binary.
    std::string allSource;
    for (auto& file : sourceFiles) {
        std::ifstream ifs(file);
        if (!ifs.is_open()) {
            err("Cannot open: " + file);
            return 1;
        }
        std::stringstream buf;
        buf << "// FORGE-BUILD-METADATA: file=" << file << "\n";
        allSource += buf.str();
        buf.str("");
        buf << ifs.rdbuf();
        allSource += buf.str();
        if (allSource.empty() || allSource.back() != '\n') allSource += "\n";
        log("Read " + std::to_string(buf.str().size()) + " bytes from " + file);
    }
    log("Total source: " + std::to_string(allSource.size()) + " bytes");

    // ------------------------------------------------------------
    // Per-component compile: src/main.fge -> obj/main.fclass
    // ------------------------------------------------------------
    int nComp = (int)sourceFiles.size();
    if (canCompilePerFile) {
        paintPhase("compiling components");
        for (int i = 0; i < nComp; i++) {
            const std::string& srcPath = sourceFiles[i];
            fs::path outPath = objDir / (fs::path(srcPath).stem().string() + ".fclass");
            if (!verbose_ && stdoutIsTty()) bubble(4);
            std::string cmd = config.fvmBin + " compile \"" + srcPath + "\" \"" + outPath.string() + "\" 2>&1";
            bool ok = (std::system(cmd.c_str()) == 0);
            if (!ok) {
                err("Component failed to compile: " + srcPath);
                return 1;
            }
            int pct = (int)((100.0 * (i + 1)) / (nComp + 1));
            std::cout << fmtPct(pct) << " " << srcPath << " -> " << outPath.string() << "\n" << std::flush;
            appendTsFile(srcPath, outPath.string(),
                         config.fvmBin + " compile " + srcPath + " " + outPath.string(), pct);
        }
    }

    // ------------------------------------------------------------
    // Runner generation + link (the ONE single binary)
    // ------------------------------------------------------------
    std::string forgeRoot = findForgeRoot();
    std::string coreLib = findCoreLibrary(forgeRoot);
    bool selfContained = coreLib.empty();
    if (selfContained) {
        paintCheck("no libforge_core.a - building self-contained runner");
    }

    std::string runnerPath = config.buildDir + "/runner.cpp";
    if (!generateRunner(config, allSource, runnerPath, config.shared, selfContained)) return 1;
    log("Generated runner: " + runnerPath);

    paintPhase("single binary");
    paintCheck("running compiler..");

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

    // native libs from uselib(...) - resolve against libsDir when not local
    for (auto& lib : config.nativeLibs) {
        if (lib.rfind("-l", 0) == 0) { compileCmd += " " + lib; continue; }
        if (fs::exists(lib)) { compileCmd += " " + lib; continue; }
        std::string base = config.libsDir.empty() ? std::string("/usr/forge/libs") : config.libsDir;
        fs::path candidate = fs::path(base) / fs::path(lib).filename();
        if (fs::exists(candidate)) compileCmd += " " + candidate.string();
        else {
            err("uselib not found: " + lib + " (looked locally and in " + base + ")");
            return 1;
        }
    }

    for (auto& f : config.extraFlags) compileCmd += " " + f;
    compileCmd += " -lpthread -ldl";
    compileCmd += " " + std::string(FORGE_SDL2_LINK_FLAGS);

    if (!verbose_ && stdoutIsTty()) bubble(6);
    if (!runCommand(compileCmd)) {
        err("Compilation failed");
        return 1;
    }
    std::cout << fmtPct(100) << " " << runnerPath << " -> " << outputBin << "\n" << std::flush;
    appendTsFile(runnerPath, outputBin, compileCmd, 100);

    double total = std::chrono::duration<double>(std::chrono::steady_clock::now() - tStart).count();
    paintDone("build complete", total);

    if (config.shared) {
        std::string headerPath = outputBin + ".h";
        if (generateExportHeader(headerPath)) {
            std::cout << "  header: " << headerPath << "\n";
        }
    }

    std::cout << "\nBuild successful (single file): " << outputBin << "\n";
    if (config.shared) {
        std::cout << "Call it from C/C++ with:\n"
                  << "  #include \"" << outputBin << ".h\"\n"
                  << "  c++ app.cpp " << outputBin << " -o app\n"
                  << "  (or from Forge: import \"ffi\" -> ffi.open(\"./" << outputBin << "\"))\n";
    } else {
        std::cout << "Run with: ./" << outputBin << "\n";
    }
    appendTsFile("build", "done {" + std::to_string(total) + "s}", "", 100);
    return 0;
}

} // namespace forge::build
