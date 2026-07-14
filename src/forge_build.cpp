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
// C String Escaping
// ============================================================

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

static bool generateRunner(const BuildConfig& config, const std::string& concatenatedSource,
                           const std::string& runnerPath) {
    std::ofstream out(runnerPath);
    if (!out.is_open()) {
        err("Cannot create runner file: " + runnerPath);
        return false;
    }

    std::string escaped = escapeForCString(concatenatedSource);

    out << R"(// Auto-generated by forge build - do not edit
#include "fvm/runtime.hpp"
#include "fvm/runtime.cpp"
#include "lexer.hpp"
#include "lexer.cpp"
#include "parser.hpp"
#include "parser.cpp"
#include "value.hpp"
#include "value.cpp"
#include "chunk.hpp"
#include "chunk.cpp"
#include "compiler.hpp"
#include "compiler.cpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib>

static const char* FORGE_SOURCE = ")";
    out << escaped;
    out << R"(";

int main(int argc, char* argv[]) {
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
            std::cout << "  -h, --help        Show this help\n";
            std::cout << "\nForgeLists.txt format:\n";
            std::cout << "  project(MyApp)\n";
            std::cout << "  version(1.0.0)\n";
            std::cout << "  source(src)\n";
            std::cout << "  output(myapp)\n";
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

    // Generate standalone runner
    std::string runnerPath = config.buildDir + "/runner.cpp";
    if (!generateRunner(config, allSource, runnerPath)) return 1;
    log("Generated runner: " + runnerPath);

    // Find forge root for include paths
    std::string forgeRoot = findForgeRoot();

    // Compile with g++
    std::string outputBin = config.outputName;
    std::string archFlag = getArchFlag(config.arch);
    std::string optFlag = config.release ? "-O2" : "-O0 -g";
    std::string stdFlag = "-std=c++17";
    std::string includeFlag = "-I" + forgeRoot + "/src";

    std::string compileCmd = config.compiler + " " + optFlag + " " + archFlag + " " + stdFlag + " " + includeFlag;
    compileCmd += " -o " + outputBin + " " + runnerPath;

    for (auto& f : config.extraFlags) compileCmd += " " + f;
    compileCmd += " -lpthread";

    std::cout << "\nCompiling standalone binary...\n";
    if (!runCommand(compileCmd)) {
        err("Compilation failed");
        return 1;
    }

    std::cout << "\nBuild successful: " << outputBin << "\n";
    std::cout << "Run with: ./" << outputBin << "\n";
    return 0;
}

} // namespace forge::build
