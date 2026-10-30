// CFMAKE - Cross-Forge Make (CMake-like configurator & build runner for Forge)
//
// :: for phase headers, => for checks, -- ... -- for done! lines, ==> for launch.
// Manages the build-releases/ folder (artifacts + cache/), generates the
// single hand-written Forgefile, then runs the forge compiler automatically.

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <thread>
#include <algorithm>
#include <thread>
#include <cstdlib>
#include <unistd.h>

namespace fs = std::filesystem;

// ============================================================
// CLI painting (:: / => / -- / ==>)
// ============================================================

static bool ttyOut() { return isatty(fileno(stdout)) != 0; }

static const char* BUBBLE[] = { "(   )", "( . )", "( o )", "( O )", "( o )", "( . )" };

static void paintPhase(const std::string& t) { std::cout << ":: " << t << "\n" << std::flush; }
static void paintCheck(const std::string& t) { std::cout << "=> " << t << "\n" << std::flush; }
static void paintDone(const std::string& t, double s) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.3f", s);
    std::cout << "-- " << t << "! -- {" << buf << "s}\n" << std::flush;
}
static void paintLaunch(const std::string& t) { std::cout << "==> " << t << "\n" << std::flush; }

static void bubble(int frames, int ms = 25) {
    if (!ttyOut()) return;
    for (int i = 0; i < frames; i++) {
        std::cout << "\r" << BUBBLE[i % 6] << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
    std::cout << "\r   \r" << std::flush;
}

static std::string trimLine(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static void writeFileText(const std::string& path, const std::string& content) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream f(path);
    if (f.is_open()) f << content;
}

// ============================================================
// Forgefile helpers
// ============================================================

// Extracted from the single hand-written file (or auto-filled).
struct ProjectInfo {
    std::string name;
    std::string version = "1.0.0";
    std::string output;
    std::string buildDir = "build-releases";
    std::vector<std::string> components;
    std::vector<std::string> modules;
    std::vector<std::string> libs;
    std::string libsDir;
    bool exists = false;
};

static std::string directiveArg(const std::string& line) {
    size_t open = line.find('(');
    size_t close = line.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close <= open) return "";
    std::string a = trimLine(line.substr(open + 1, close - open - 1));
    if (a.size() >= 2 && a.front() == '"') a = a.substr(1, a.size() - (a.back() == '"' ? 2 : 1));
    return a;
}

static ProjectInfo readForgefile(const std::string& path) {
    ProjectInfo pi;
    if (path.empty() || !fs::exists(path)) return pi;
    pi.exists = true;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        line = trimLine(line);
        if (line.empty() || line[0] == '#') continue;
        size_t open = line.find('(');
        if (open == std::string::npos) continue;
        std::string cmd = trimLine(line.substr(0, open));
        std::string a = directiveArg(line);
        if (cmd == "project" || cmd == "name") pi.name = a;
        else if (cmd == "version") pi.version = a;
        else if (cmd == "output") pi.output = a;
        else if (cmd == "builddir") pi.buildDir = a;
        else if (cmd == "component" || cmd == "file") pi.components.push_back(a);
        else if (cmd == "module") pi.modules.push_back(a);
        else if (cmd == "uselib" || cmd == "lib") pi.libs.push_back(a);
        else if (cmd == "libsdir") pi.libsDir = a;
    }
    return pi;
}

// Native `.so/.a` libraries discovered in a libs directory.
static std::vector<std::string> scanNativeLibs(const std::string& dir) {
    std::vector<std::string> found;
    if (dir.empty() || !fs::exists(dir)) return found;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        std::string ext = e.path().extension().string();
        if (ext == ".so" || ext == ".a" || ext == ".dylib") {
            found.push_back(e.path().string());
        }
    }
    std::sort(found.begin(), found.end());
    return found;
}

// Locate forge binaries: PATH + checkout layout (build-release/, build/)
// + standard install prefixes + next to cfmake itself.
static std::vector<std::string> findForgeBinaries() {
    std::vector<std::string> names = {"forgevm", "forge", "forge-lsp", "forge-debug"};
    std::vector<std::string> out;
    std::vector<std::string> searchDirs;
    const char* pathEnv = std::getenv("PATH");
    if (pathEnv) {
        std::stringstream ss(pathEnv);
        std::string d;
        while (std::getline(ss, d, ':')) if (!d.empty()) searchDirs.push_back(d);
    }
    searchDirs.push_back("./build-release");
    searchDirs.push_back("./build");
    searchDirs.push_back("/usr/local/bin");
    searchDirs.push_back("/usr/bin");
    const char* home = std::getenv("HOME");
    if (home) searchDirs.push_back(std::string(home) + "/.local/bin");
    // next to cfmake itself (checkout + install layouts)
    char exeBuf[4096];
    ssize_t len = readlink("/proc/self/exe", exeBuf, sizeof(exeBuf) - 1);
    if (len > 0) {
        exeBuf[len] = '\0';
        fs::path exeDir = fs::path(exeBuf).parent_path();
        searchDirs.push_back(exeDir.string());
        searchDirs.push_back(exeDir.parent_path().string());
    }
    for (auto& n : names) {
        for (auto& d : searchDirs) {
            fs::path cand = fs::path(d) / n;
            if (fs::exists(cand)) { out.push_back(cand.string()); break; }
        }
    }
    return out;
}

static bool runForge(const std::string& cmd) {
    std::cout << "\n";
    int ret = std::system(cmd.c_str());
    std::cout << "\n";
    return ret == 0;
}

// ============================================================
// Forgefile generation (pre-generated config, hand-tune after)
// ============================================================

static std::string generateForgefile(const std::string& name) {
    // Discover .fge sources for pre-filled components (the generated file is
    // still a single file - hand-edit components to add/remove pieces).
    std::vector<std::string> sources;
    if (fs::exists("src")) {
        for (const auto& e : fs::recursive_directory_iterator("src")) {
            if (e.is_regular_file() && e.path().extension() == ".fge") {
                sources.push_back(e.path().string());
            }
        }
    }
    if (!fs::exists("main.fge")) sources.insert(sources.begin(), "src/main.fge");

    std::ostringstream g;
    g << "# " << name << " - FORGE single-file project (auto-generated by CFMAKE)\n";
    g << "# ONE file for ONE binary: every component is written here by hand.\n";
    g << "# Edit this file freely - it is the only build recipe.\n\n";
    g << "project(" << name << ")\n";
    g << "version(1.0.0)\n";
    g << "arch(x64)                 # x86 | x64 | arm64\n";
    g << "type(executable)          # executable | shared\n";
    g << "output(" << name << ")\n";
    g << "builddir(build-releases)  # artifacts + cache/ + make.ts live here\n";
    g << "release(true)\n\n";
    g << "# --- components (hand-written, in link order) ---\n";
    if (sources.empty()) sources.push_back("src/main.fge");
    for (auto& s : sources) g << "component(" << s << ")\n";
    g << "\n# --- forge modules (runtime libraries) ---\n";
    g << "module(io)\n";
    g << "module(json)\n";
    g << "module(gui)   # remove if no liquid glass UI\n\n";
    g << "# --- native libraries ---\n";
    g << "libsdir(/usr/forge/libs)\n";
    g << "# uselib(mylib.so)   # also scanned in /usr/forge/libs\n\n";
    g << "# --- compiler c flags for the C runner build ---\n";
    g << "# cflags(-O2)\n";
    return g.str();
}

int main(int argc, char** argv) {
    auto t0 = std::chrono::steady_clock::now();

    paintPhase("running CFMake");

    if (argc < 2) {
        std::cout << "\nCFMake - CMake-like configurator for Forge\n\n";
        std::cout << "  cfmake init <name>     Scaffold project + hand-written Forgefile\n";
        std::cout << "  cfmake configure       Generate/config + detect forge + auto build\n";
        std::cout << "  cfmake configure --no-build\n";
        std::cout << "  cfmake build           Build without reconfiguring\n";
        std::cout << "  cfmake wizard          Interactive setup\n";
        std::cout << "  cfmake clean           Remove build-releases cache\n\n";
        std::cout << "Everything lives in one file: Forgefile (single hand-written project).\n";
        return (argc < 2) ? 1 : 0;
    }

    std::string cmd = argv[1];

    // ---------------------------------------------------------- init
    if (cmd == "init" && argc >= 3) {
        std::string name = argv[2];
        paintPhase("init " + name);
        fs::create_directories(name + "/src");
        fs::create_directories(name + "/lib");
        fs::create_directories(name + "/tests");

        writeFileText(name + "/src/main.fge",
                      std::string("// ") + name + " - main entry point\n\nfn main() {\n  print(\"Hello from " + name + "!\")\n}\n\nmain()\n");
        writeFileText(name + "/Forgefile", generateForgefile(name));
        writeFileText(name + "/README.md",
                      "# " + name + "\n\nBuilt with FORGE + CFMAKE.\n\n## Build\n```bash\ncfmake configure      # generate cache + build\n# or\nforgevm build         # manual single build\n```\n");

        paintCheck("dir " + name + "/src");
        paintCheck("Forgefile (single hand-written project file)");
        paintDone("init complete", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
        std::cout << "\nNext: cd " << name << " && cfmake configure\n";
        return 0;
    }

    // ---------------------------------------------------------- configure
    if (cmd == "configure") {
        bool noBuild = false;
        for (int i = 2; i < argc; i++) if (std::string(argv[i]) == "--no-build") noBuild = true;

        // 1. project source of truth: Forgefile (single file)
        std::string forgefile = fs::exists("Forgefile") ? "Forgefile" : (fs::exists("ForgeLists.txt") && !fs::exists("Forgefile") ? "ForgeLists.txt" : "");
        ProjectInfo pi = readForgefile(forgefile);

        std::string appName = pi.name;
        if (!pi.exists && forgefile.empty()) {
            // No config yet: pre-generate one that finds forge itself and the libs.
            appName = fs::current_path().filename().string();
            std::string freshlyGenerated = generateForgefile(appName);
            writeFileText("Forgefile", freshlyGenerated);
            forgefile = "Forgefile";
            pi = readForgefile(forgefile);
            paintCheck("PRE-GENERATED Forgefile (hand-tune the components)");
        }
        if (appName.empty()) appName = "app";
        std::string appOut = pi.output.empty() ? appName : pi.output;
        std::string dest = pi.exists ? pi.buildDir : "build-releases";

        paintPhase("configuring {dest: " + dest + ", app: " + appName + "}");

        // 2. build-releases + cache folder
        fs::create_directories(dest);
        fs::create_directories(dest + "/cache");

        // 3. find the FORGE binaries
        paintCheck("finding FORGE binaries..");
        auto bins = findForgeBinaries();
        std::string fvmBin = "forgevm";
        for (auto& b : bins) {
            paintCheck("found " + b);
            if (b.find("forgevm") != std::string::npos) fvmBin = b;
        }
        if (bins.empty()) paintCheck("no forge binaries found in PATH/build-release - will use bare 'forgevm'");
        bubble(3);

        // 4. check available libs (runtime modules + native libs dir)
        paintCheck("checking available libs...");
        std::vector<std::string> runtimeMods = pi.modules.empty()
            ? std::vector<std::string>{"io", "os", "json", "gui"}
            : pi.modules;
        for (auto& m : runtimeMods) {
            std::string up = m;
            std::transform(up.begin(), up.end(), up.begin(), ::toupper);
            paintCheck("lib " + up + "..");
        }

        std::string libsDir = pi.libsDir.empty() ? std::string("/usr/forge/libs") : pi.libsDir;
        auto native = scanNativeLibs(libsDir);

        // one stroke per lib with the same => style
        if (!native.empty()) paintCheck("FOUND " + std::to_string(native.size()) + " libs in " + libsDir);
        for (auto& l : native) paintCheck(l + " (FOUND)");
        for (auto& l : pi.libs) paintCheck("uselib " + l);
        if (native.empty() && pi.libs.empty()) paintCheck("no native libs in " + libsDir + " - using built-in runtime only");
        bubble(3);

        // 5. cache (make-generated .ts files)
        std::ostringstream cfg;
        cfg << "; cfmake cache - generated " << std::chrono::system_clock::now().time_since_epoch().count() << "\n";
        cfg << "fvmBinary=" << fvmBin << "\n";
        cfg << "libsDir=" << libsDir << "\n";
        cfg << "appName=" << appName << "\n";
        cfg << "output=" << appOut << "\n";
        cfg << "buildDir=" << dest << "\n";
        for (auto& b : bins) cfg << "foundBinary=" << b << "\n";
        for (auto& l : native) cfg << "foundLib=" << l << "\n";
        for (auto& m : runtimeMods) cfg << "module=" << m << "\n";
        writeFileText(dest + "/cache/config.ts", cfg.str());

        double cfgSecs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        paintDone("configuring done", cfgSecs);

        if (noBuild) {
            paintCheck("build skipped (--no-build)");
            return 0;
        }

        // 6. run the forge compiler automatically
        paintLaunch("running compiler..");
        bubble(5);
        bool ok = runForge(fvmBin + " build '" + forgefile + "'");
        if (!ok) {
            std::cout << "!! build failed - run with -v for full compiler args\n";
            return 1;
        }
        paintDone("CFMake done", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
        return 0;
    }

    // ---------------------------------------------------------- build
    if (cmd == "build") {
        std::string forgefile = fs::exists("Forgefile") ? "Forgefile" : "ForgeLists.txt";
        // prefer the fvm binary resolved during configure (cache/config.ts)
        std::string fvmBin;
        std::ifstream cache("build-releases/cache/config.ts");
        if (cache.is_open()) {
            std::string line;
            while (std::getline(cache, line)) {
                if (line.rfind("fvmBinary=", 0) == 0) fvmBin = trimLine(line.substr(10));
            }
        }
        if (fvmBin.empty() || !fs::exists(fvmBin)) {
            auto bins = findForgeBinaries();
            for (auto& b : bins) if (b.find("forgevm") != std::string::npos) fvmBin = b;
        }
        paintLaunch("running compiler..");
        bubble(4);
        return runForge(fvmBin + " build '" + forgefile + "'") ? 0 : 1;
    }

    // ---------------------------------------------------------- clean
    if (cmd == "clean") {
        paintPhase("cleaning");
        if (fs::exists("build-releases")) fs::remove_all("build-releases");
        paintDone("clean complete", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
        return 0;
    }

    // ---------------------------------------------------------- wizard
    if (cmd == "wizard") {
        std::cout << "[*] Interactive Project Wizard\n\n";
        std::string name, type;
        std::cout << "Project name: ";
        std::getline(std::cin, name);
        std::cout << "Type (executable/library/shared): ";
        std::getline(std::cin, type);
        if (type.empty()) type = "executable";
        fs::create_directories(name + "/src");
        std::ofstream fl(name + "/ForgeLists.txt");
        fl << "project(" << name << ")\nversion(1.0.0)\nsource(src)\noutput(" << name << ")\ntype(" << type << ")\narch(x64)\n";
        fl.close();
        std::ofstream main(name + "/src/main.fge");
        main << "fn main() { print(\"" << name << " initialized\") }\nmain()\n";
        main.close();
        std::cout << "\n[✓] Project created: " << name << "/\n";
        return 0;
    }

    std::cout << "[!] Unknown command: " << cmd << "\n";
    return 1;
}
