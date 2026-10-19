#include "pkg_manager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <cstdlib>
#include <functional>
#include <cstdio>

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#else
#include <windows.h>
#endif

namespace fs = std::filesystem;

static const char* REGISTRY_URL = "https://raw.githubusercontent.com/Nohat1337/FORGE/main/packages/registry.json";

// ============================================================
// Security helpers
// ============================================================

// Package names come from CLI args AND from registry JSON (attacker-influenced).
// Restrict to a strict charset so names can never form paths or shell metachars.
static bool isValidPkgName(const std::string& name) {
    if (name.empty() || name.size() > 128) return false;
    if (name[0] == '.') return false;
    for (char c : name) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
        if (!ok) return false;
    }
    // Reject traversal-looking names even though '/' is already banned
    if (name == "." || name == "..") return false;
    if (name.find("..") != std::string::npos) return false;
    return true;
}

// True only if 'candidate' resolves strictly inside directory 'base'.
static bool isPathInsideDir(const fs::path& base, const fs::path& candidate) {
    std::error_code ec;
    fs::path cb = fs::weakly_canonical(base, ec);
    if (ec) return false;
    fs::path cc = fs::weakly_canonical(candidate, ec);
    if (ec) return false;
    std::string b = cb.string();
    std::string c = cc.string();
    while (!b.empty() && b.back() == '/') b.pop_back();
    if (b.empty()) return false;
    if (c == b) return true;
    return c.size() > b.size() && c.compare(0, b.size(), b) == 0 && c[b.size()] == '/';
}

// Run a process directly (no shell). Returns exit code, -1 on spawn failure.
// If captureOut is non-null, stdout is captured into it.
// If outFile is non-empty, stdout is written to that file instead.
static int runProcess(const std::vector<std::string>& args,
                      std::string* captureOut = nullptr,
                      const std::string& outFile = "") {
#ifndef _WIN32
    // Temp file used for stdout capture (avoids pipe deadlock complexity)
    FILE* tmp = nullptr;
    int outFd = -1;
    if (!outFile.empty()) {
        FILE* f = fopen(outFile.c_str(), "wb");
        if (!f) return -1;
        outFd = fileno(f);
    } else if (captureOut) {
        tmp = tmpfile();
        if (!tmp) return -1;
        outFd = fileno(tmp);
    }

    fflush(nullptr);
    pid_t pid = fork();
    if (pid < 0) {
        if (tmp) fclose(tmp);
        return -1;
    }
    if (pid == 0) {
        if (outFd >= 0) {
            dup2(outFd, STDOUT_FILENO);
        }
        // stderr -> /dev/null to keep output clean
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    if (tmp) fclose(tmp);

    int status = 0;
    waitpid(pid, &status, 0);
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    if (tmp) {
        if (captureOut) {
            rewind(tmp);
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), tmp)) > 0)
                captureOut->append(buf, n);
        }
        fclose(tmp);
    }
    return code;
#else
    // Windows: build an escaped command line and use CreateProcess (no cmd.exe)
    std::string cmdline;
    for (size_t i = 0; i < args.size(); i++) {
        if (i > 0) cmdline += " ";
        cmdline += "\"";
        for (char ch : args[i]) {
            if (ch == '"') cmdline += "\\\"";
            else cmdline += ch;
        }
        cmdline += "\"";
    }
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    HANDLE outHandle = INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    std::string tmpCapturePath;
    if (!outFile.empty()) {
        outHandle = CreateFileA(outFile.c_str(), GENERIC_WRITE, 0, &sa,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (outHandle == INVALID_HANDLE_VALUE) return -1;
    } else if (captureOut) {
        char tmpPath[MAX_PATH];
        char tmpName[MAX_PATH];
        if (!GetTempPathA(MAX_PATH, tmpPath) ||
            !GetTempFileNameA(tmpPath, "fge", 0, tmpName)) return -1;
        tmpCapturePath = tmpName;
        outHandle = CreateFileA(tmpName, GENERIC_WRITE | GENERIC_READ, 0, &sa,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (outHandle == INVALID_HANDLE_VALUE) return -1;
    }
    if (outHandle != INVALID_HANDLE_VALUE) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = outHandle;
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }
    BOOL ok = CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, TRUE,
                             0, nullptr, nullptr, &si, &pi);
    int code = -1;
    if (ok) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD ec2 = 0;
        GetExitCodeProcess(pi.hProcess, &ec2);
        code = (int)ec2;
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    if (outHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(outHandle);
        if (captureOut && !tmpCapturePath.empty()) {
            std::ifstream f(tmpCapturePath, std::ios::binary);
            std::stringstream ss;
            ss << f.rdbuf();
            *captureOut = ss.str();
            DeleteFileA(tmpCapturePath.c_str());
        }
    }
    return code;
#endif
}

static bool curlAvailable() {
    return runProcess({"curl", "--version"}) == 0;
}

static std::string getHomeDir() {
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    return home ? std::string(home) : ".";
}

static std::string getForgeDir() {
    return getHomeDir() + "/.forge";
}

static std::string getCacheDir() { return getForgeDir() + "/cache"; }
static std::string getInstallDir() { return getForgeDir() + "/packages"; }
static std::string getRegistryCache() { return getCacheDir() + "/registry.json"; }
static std::string getInstalledFile() { return getForgeDir() + "/installed.json"; }

static std::string readSmallFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void writeSmallFile(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f.is_open()) {
        std::cerr << "Error: Cannot write to " << path << "\n";
        return;
    }
    f << content;
}

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

// ============================================================
// Minimal JSON parsing helpers
// ============================================================

static std::string jsonGetString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return "";
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
        pos++;
        auto end = json.find('"', pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    }
    auto end = json.find_first_of(",}\n", pos);
    if (end == std::string::npos) return json.substr(pos);
    return trim(json.substr(pos, end - pos));
}

struct PkgInfo {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::string license;
    std::string url;
};

// Extract a JSON object block for a given key from a parent object.
// e.g. given {"packages": { "foo": {...}, "bar": {...} }}, extractPackages() returns
// a vector of {key, json_object_string} pairs.
static std::vector<std::pair<std::string, std::string>> jsonExtractObjects(const std::string& json) {
    std::vector<std::pair<std::string, std::string>> result;
    size_t i = 0;
    while (i < json.size()) {
        // Find a quoted key
        auto q1 = json.find('"', i);
        if (q1 == std::string::npos) break;
        auto q2 = json.find('"', q1 + 1);
        if (q2 == std::string::npos) break;
        std::string key = json.substr(q1 + 1, q2 - q1 - 1);

        // Find the colon
        auto colon = json.find(':', q2 + 1);
        if (colon == std::string::npos) break;

        // Find the opening brace of the value
        auto brace = json.find('{', colon + 1);
        if (brace == std::string::npos) break;

        // Match braces
        int depth = 1;
        size_t j = brace + 1;
        while (j < json.size() && depth > 0) {
            if (json[j] == '{') depth++;
            else if (json[j] == '}') depth--;
            j++;
        }
        if (depth == 0) {
            result.push_back({key, json.substr(brace, j - brace)});
        }
        i = j;
    }
    return result;
}

// Extract the "packages": {...} section from the registry
static std::string jsonExtractSection(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    auto colon = json.find(':', pos + search.size());
    if (colon == std::string::npos) return "";
    auto brace = json.find('{', colon + 1);
    if (brace == std::string::npos) return "";
    int depth = 1;
    size_t j = brace + 1;
    while (j < json.size() && depth > 0) {
        if (json[j] == '{') depth++;
        else if (json[j] == '}') depth--;
        j++;
    }
    if (depth != 0) return "";
    return json.substr(brace, j - brace);
}

// ============================================================
// HTTP fetch using curl
// ============================================================

static std::string curlFetch(const std::string& url) {
    std::string result;
    // Runs curl directly via execvp — no shell, no injection surface
    runProcess({"curl", "-sL", "--connect-timeout", "10", "--max-time", "30", url},
               &result);
    return result;
}

// ============================================================
// Download a file from GitHub raw
// ============================================================

static bool downloadFile(const std::string& url, const std::string& destPath) {
    fs::create_directories(fs::path(destPath).parent_path().string());
    int ret = runProcess({"curl", "-sL", "--connect-timeout", "10",
                          "--max-time", "60", "-o", destPath, url});
    return ret == 0 && fs::exists(destPath);
}

// ============================================================
// Registry operations
// ============================================================

struct Registry {
    std::string rawJson;
    std::vector<PkgInfo> packages;

    bool loadFromCache() {
        rawJson = readSmallFile(getRegistryCache());
        return !rawJson.empty();
    }

    bool fetchFromServer() {
        std::cerr << "Fetching registry from server...\n";
        rawJson = curlFetch(REGISTRY_URL);
        if (rawJson.empty()) {
            std::cerr << "Error: Could not fetch registry.\n";
            return false;
        }
        // Cache it
        fs::create_directories(getCacheDir());
        writeSmallFile(getRegistryCache(), rawJson);
        return true;
    }

    bool refresh() {
        if (!fetchFromServer()) {
            std::cerr << "Trying cache...\n";
            return loadFromCache();
        }
        return true;
    }

    void parse() {
        packages.clear();
        std::string pkgsSection = jsonExtractSection(rawJson, "packages");
        if (pkgsSection.empty()) return;
        auto entries = jsonExtractObjects(pkgsSection);
        for (auto& [name, obj] : entries) {
            PkgInfo pkg;
            pkg.name = name;
            pkg.version = jsonGetString(obj, "version");
            pkg.description = jsonGetString(obj, "description");
            pkg.author = jsonGetString(obj, "author");
            pkg.license = jsonGetString(obj, "license");
            pkg.url = jsonGetString(obj, "url");
            packages.push_back(pkg);
        }
    }

    PkgInfo* find(const std::string& name) {
        for (auto& p : packages) {
            if (p.name == name) return &p;
        }
        return nullptr;
    }
};

// ============================================================
// Installed packages tracking
// ============================================================

struct InstalledPkg {
    std::string name;
    std::string version;
    std::string installedAt;
};

static std::vector<InstalledPkg> loadInstalled() {
    std::vector<InstalledPkg> result;
    std::string json = readSmallFile(getInstalledFile());
    if (json.empty()) return result;
    auto entries = jsonExtractObjects(json);
    for (auto& [name, obj] : entries) {
        InstalledPkg pkg;
        pkg.name = name;
        pkg.version = jsonGetString(obj, "version");
        pkg.installedAt = jsonGetString(obj, "installedAt");
        result.push_back(pkg);
    }
    return result;
}

static void saveInstalled(const std::vector<InstalledPkg>& pkgs) {
    std::ostringstream ss;
    ss << "{\n";
    for (size_t i = 0; i < pkgs.size(); i++) {
        ss << "  \"" << pkgs[i].name << "\": {\n";
        ss << "    \"version\": \"" << pkgs[i].version << "\",\n";
        ss << "    \"installedAt\": \"" << pkgs[i].installedAt << "\"\n";
        ss << "  }";
        if (i + 1 < pkgs.size()) ss << ",";
        ss << "\n";
    }
    ss << "}\n";
    fs::create_directories(getForgeDir());
    writeSmallFile(getInstalledFile(), ss.str());
}

static bool isInstalled(const std::string& name) {
    auto pkgs = loadInstalled();
    for (auto& p : pkgs) {
        if (p.name == name) return true;
    }
    return false;
}

// ============================================================
// Package file download and installation
// ============================================================

static bool installPkgFiles(const PkgInfo& pkg) {
    // Defense in depth: registry-supplied names are untrusted input
    if (!isValidPkgName(pkg.name)) {
        std::cerr << "Error: Invalid package name '" << pkg.name << "'.\n";
        return false;
    }
    const fs::path baseDir = getInstallDir();
    fs::path pkgPath = baseDir / pkg.name;
    if (!isPathInsideDir(baseDir, pkgPath)) {
        std::cerr << "Error: Refusing to install outside " << baseDir.string() << "\n";
        return false;
    }
    std::string pkgDir = pkgPath.string();
    fs::create_directories(pkgDir);

    std::string baseUrl = "https://raw.githubusercontent.com/Nohat1337/FORGE/main/";

    // If the package has explicit files, download them
    // For now, try to download known file patterns
    // The registry lists files relative to the repo root
    // We'll download each one
    std::string filesJson = jsonExtractSection(
        readSmallFile(getRegistryCache()), pkg.name);
    // Actually, we need to re-parse the registry for the file list
    // Let's just download the whole package directory structure

    std::cerr << "Installing " << pkg.name << "@" << pkg.version << "...\n";

    // For packages from the repo, we know the structure:
    // lib/<name>/ contains .fge files
    std::string libUrl = baseUrl + "lib/" + pkg.name + "/";

    // Try common file names for the package
    // First, try to get a listing by attempting known patterns
    std::string listingUrl = baseUrl + "lib/" + pkg.name + "/";

    // Download each known file from the registry
    Registry reg;
    reg.loadFromCache();
    reg.parse();
    PkgInfo* info = reg.find(pkg.name);
    if (info && !info->url.empty()) {
        // We have file info — but files field might not be populated in registry
        // Fall back to downloading the directory
    }

    // Smart download: try to download common Forge library files
    std::vector<std::string> commonFiles = {
        "mod.fge", pkg.name + ".fge", "index.fge", "main.fge"
    };

    bool anyDownloaded = false;
    for (auto& file : commonFiles) {
        std::string fileUrl = baseUrl + "lib/" + pkg.name + "/" + file;
        std::string dest = pkgDir + "/" + file;

        // Check if file exists at URL (HEAD-style request)
        std::string httpCode;
        runProcess({"curl", "-sL", "--connect-timeout", "5", "--max-time", "10",
                    "-o", "/dev/null", "-w", "%{http_code}", fileUrl},
                   &httpCode);
        // Trim whitespace
        while (!httpCode.empty() &&
               (httpCode.back() == '\n' || httpCode.back() == '\r' || httpCode.back() == ' '))
            httpCode.pop_back();

        if (httpCode == "200") {
            if (downloadFile(fileUrl, dest)) {
                std::cerr << "  Downloaded lib/" << pkg.name << "/" << file << "\n";
                anyDownloaded = true;
            }
        }
    }

    if (!anyDownloaded) {
        // Try the .fge file directly
        std::string fgeUrl = baseUrl + "lib/" + pkg.name + ".fge";
        std::string dest = pkgDir + "/" + pkg.name + ".fge";
        if (downloadFile(fgeUrl, dest)) {
            std::cerr << "  Downloaded lib/" << pkg.name << ".fge\n";
            anyDownloaded = true;
        }
    }

    return anyDownloaded;
}

// ============================================================
// Commands
// ============================================================

static void printUsage() {
    std::cout << "Forge Package Manager\n";
    std::cout << "Usage: forge pkg <command> [args]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  init [name] [version]    Create a new forge.json\n";
    std::cout << "  search <query>           Search packages in registry\n";
    std::cout << "  install <pkg>[@version]  Install a package\n";
    std::cout << "  remove <pkg>             Remove an installed package\n";
    std::cout << "  list                     List installed packages\n";
    std::cout << "  update                   Update registry cache\n";
    std::cout << "  add <pkg>[@version]      Add a dependency to forge.json\n";
    std::cout << "  add --dev <pkg>[@ver]    Add a dev dependency\n";
    std::cout << "  publish                  Publish package to registry\n";
    std::cout << "  cache clean              Clean package cache\n";
}

static std::string getForgeRoot() {
    fs::path cwd = fs::current_path();
    while (!cwd.empty()) {
        if (fs::exists(cwd / "forge.json")) return cwd.string();
        auto parent = cwd.parent_path();
        if (parent == cwd) break;
        cwd = parent;
    }
    return "";
}

static std::string prettyJson(const std::string& name, const std::string& version, const std::string& description,
                               const std::string& author, const std::string& license,
                               const std::string& mainFile) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << name << "\",\n";
    ss << "  \"version\": \"" << version << "\",\n";
    ss << "  \"description\": \"" << description << "\",\n";
    ss << "  \"author\": \"" << author << "\",\n";
    ss << "  \"license\": \"" << license << "\",\n";
    ss << "  \"main\": \"" << mainFile << "\",\n";
    ss << "  \"dependencies\": {},\n";
    ss << "  \"devDependencies\": {}\n";
    ss << "}\n";
    return ss.str();
}

static std::string addToJsonObject(const std::string& obj, const std::string& key, const std::string& val) {
    if (obj == "{}" || obj.empty()) {
        return "{\n    \"" + key + "\": \"" + val + "\"\n  }";
    }
    auto lastBrace = obj.rfind('}');
    if (lastBrace == std::string::npos) return obj;
    std::string prefix = obj.substr(0, lastBrace);
    bool hasEntries = false;
    for (auto c : prefix) if (c == '"') { hasEntries = true; break; }
    if (hasEntries) prefix += ",\n";
    prefix += "    \"" + key + "\": \"" + val + "\"\n  }";
    return prefix;
}

static std::string getOrCreateEmptyObject(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "{}";
    auto braceStart = json.find('{', pos);
    if (braceStart == std::string::npos) return "{}";
    int depth = 1;
    size_t i = braceStart + 1;
    while (i < json.size() && depth > 0) {
        if (json[i] == '{') depth++;
        else if (json[i] == '}') depth--;
        i++;
    }
    if (depth != 0) return "{}";
    return json.substr(braceStart, i - braceStart);
}

static std::string removeFromJsonObject(const std::string& obj, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = obj.find(search);
    if (pos == std::string::npos) return obj;
    auto lineStart = obj.rfind('\n', pos);
    if (lineStart == std::string::npos) lineStart = 0;
    else lineStart++;
    auto lineEnd = obj.find('\n', pos);
    if (lineEnd == std::string::npos) lineEnd = obj.size();
    else lineEnd++;
    return obj.substr(0, lineStart) + obj.substr(lineEnd);
}

// ---- Commands ----

static int cmdInit(int argc, char* argv[]) {
    std::string name = "my-forge-project";
    std::string version = "0.1.0";
    if (argc > 0) name = argv[0];
    if (argc > 1) version = argv[1];
    if (fs::exists("forge.json")) {
        std::cerr << "Error: forge.json already exists\n";
        return 1;
    }
    std::string author = "unknown";
    const char* user = std::getenv("USER");
    if (user) author = user;
    writeSmallFile("forge.json", prettyJson(name, version, "", author, "MIT", "main.fge"));
    fs::create_directories("src");
    fs::create_directories("lib");
    fs::create_directories("tests");
    std::cout << "Created forge.json\n";
    std::cout << "Project: " << name << " v" << version << "\n";
    std::cout << "Created directories: src/, lib/, tests/\n";
    return 0;
}

static int cmdSearch(int argc, char* argv[]) {
    if (argc < 1) {
        std::cerr << "Error: Search query required\n";
        return 1;
    }

    std::string query = argv[0];

    Registry reg;
    if (!reg.loadFromCache()) {
        if (!reg.refresh()) {
            std::cerr << "Error: Could not load registry. Try 'forge pkg update' first.\n";
            return 1;
        }
    }
    reg.parse();

    // Case-insensitive search
    std::string queryLower = query;
    std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(), ::tolower);

    std::cout << "Search results for \"" << query << "\":\n\n";

    int found = 0;
    for (auto& pkg : reg.packages) {
        std::string nameLower = pkg.name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        std::string descLower = pkg.description;
        std::transform(descLower.begin(), descLower.end(), descLower.begin(), ::tolower);

        if (nameLower.find(queryLower) != std::string::npos ||
            descLower.find(queryLower) != std::string::npos) {
            std::cout << "  " << pkg.name << " v" << pkg.version;
            if (isInstalled(pkg.name)) std::cout << " [installed]";
            std::cout << "\n";
            std::cout << "    " << pkg.description << "\n";
            std::cout << "    by " << pkg.author << " | " << pkg.url << "\n\n";
            found++;
        }
    }

    if (found == 0) {
        std::cout << "  No packages found.\n";
    } else {
        std::cout << found << " package(s) found.\n";
    }
    return 0;
}

static int cmdInstall(int argc, char* argv[]) {
    if (argc < 1) {
        std::cerr << "Error: Package name required\n";
        return 1;
    }

    std::string pkgName = argv[0];
    std::string pkgVersion = "*";

    auto at = pkgName.find('@');
    if (at != std::string::npos) {
        pkgVersion = pkgName.substr(at + 1);
        pkgName = pkgName.substr(0, at);
    }

    if (!isValidPkgName(pkgName)) {
        std::cerr << "Error: Invalid package name '" << pkgName << "'.\n";
        std::cerr << "Names may contain only letters, digits, '.', '_' and '-'.\n";
        return 1;
    }

    // Check if already installed
    if (isInstalled(pkgName)) {
        std::cout << pkgName << " is already installed.\n";
        return 0;
    }

    // Load registry
    Registry reg;
    if (!reg.loadFromCache()) {
        if (!reg.refresh()) {
            std::cerr << "Error: Could not load registry. Try 'forge pkg update' first.\n";
            return 1;
        }
    }
    reg.parse();

    PkgInfo* pkg = reg.find(pkgName);
    if (!pkg) {
        std::cerr << "Error: Package '" << pkgName << "' not found in registry.\n";
        std::cerr << "Run 'forge pkg search " << pkgName << "' to find packages.\n";
        return 1;
    }

    // Download and install
    if (!installPkgFiles(*pkg)) {
        std::cerr << "Warning: No files downloaded for " << pkgName << ".\n";
        std::cerr << "Package may not have downloadable files yet.\n";
    }

    // Record as installed
    auto installed = loadInstalled();
    InstalledPkg newPkg;
    newPkg.name = pkg->name;
    newPkg.version = pkg->version;

    // Get current time string
    time_t now = time(nullptr);
    char timeBuf[64];
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    newPkg.installedAt = timeBuf;

    installed.push_back(newPkg);
    saveInstalled(installed);

    std::cout << "Installed " << pkg->name << "@" << pkg->version << "\n";
    std::cout << "Location: " << getInstallDir() << "/" << pkg->name << "/\n";
    return 0;
}

static int cmdRemove(int argc, char* argv[]) {
    if (argc < 1) {
        std::cerr << "Error: Package name required\n";
        return 1;
    }

    std::string pkgName = argv[0];
    if (!isValidPkgName(pkgName)) {
        std::cerr << "Error: Invalid package name '" << pkgName << "'.\n";
        return 1;
    }
    auto installed = loadInstalled();
    bool found = false;

    for (auto it = installed.begin(); it != installed.end(); ++it) {
        if (it->name == pkgName) {
            installed.erase(it);
            found = true;
            break;
        }
    }

    if (!found) {
        std::cerr << "Error: " << pkgName << " is not installed.\n";
        return 1;
    }

    saveInstalled(installed);

    // Remove downloaded files — with containment check so a tampered
    // installed.json can never delete arbitrary directories
    fs::path baseDir = getInstallDir();
    fs::path pkgPath = baseDir / pkgName;
    if (!isPathInsideDir(baseDir, pkgPath)) {
        std::cerr << "Error: Refusing to remove path outside " << baseDir.string() << "\n";
        return 1;
    }
    std::string pkgDir = pkgPath.string();
    if (fs::exists(pkgDir)) {
        fs::remove_all(pkgDir);
        std::cout << "Removed " << pkgDir << "\n";
    }

    std::cout << "Removed " << pkgName << "\n";
    return 0;
}

static int cmdList() {
    auto installed = loadInstalled();
    if (installed.empty()) {
        std::cout << "No packages installed.\n";
        std::cout << "Use 'forge pkg search <query>' to find packages.\n";
        return 0;
    }

    std::cout << "Installed packages:\n\n";
    for (auto& pkg : installed) {
        std::cout << "  " << pkg.name << " v" << pkg.version;
        if (!pkg.installedAt.empty()) std::cout << " (installed " << pkg.installedAt << ")";
        std::cout << "\n";
    }
    std::cout << "\n" << installed.size() << " package(s) installed.\n";
    std::cout << "Location: " << getInstallDir() << "/\n";
    return 0;
}

static int cmdUpdate() {
    Registry reg;
    if (!reg.refresh()) {
        return 1;
    }
    reg.parse();
    std::cout << "Registry updated. " << reg.packages.size() << " packages available.\n";
    std::cout << "Cache: " << getRegistryCache() << "\n";
    return 0;
}

static int cmdAdd(int argc, char* argv[]) {
    bool dev = false;
    std::vector<std::string> pkgs;
    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--dev") dev = true;
        else pkgs.push_back(a);
    }
    if (pkgs.empty()) {
        std::cerr << "Error: No packages specified\n";
        return 1;
    }
    std::string json = readSmallFile("forge.json");
    if (json.empty()) {
        std::cerr << "Error: No forge.json found. Run 'forge pkg init' first.\n";
        return 1;
    }
    std::string section = dev ? "devDependencies" : "dependencies";
    for (auto& pkg : pkgs) {
        std::string pkgName = pkg;
        std::string pkgVersion = "*";
        auto at = pkg.find('@');
        if (at != std::string::npos) {
            pkgName = pkg.substr(0, at);
            pkgVersion = pkg.substr(at + 1);
        }
        std::string depsObj = getOrCreateEmptyObject(json, section);
        std::string updated = addToJsonObject(depsObj, pkgName, pkgVersion);
        auto search = "\"" + section + "\"";
        auto pos = json.find(search);
        if (pos != std::string::npos) {
            auto braceStart = json.find('{', pos);
            if (braceStart != std::string::npos) {
                int depth = 1;
                size_t i = braceStart + 1;
                while (i < json.size() && depth > 0) {
                    if (json[i] == '{') depth++;
                    else if (json[i] == '}') depth--;
                    i++;
                }
                if (depth == 0) {
                    json = json.substr(0, braceStart) + updated + json.substr(i);
                }
            }
        }
        std::cout << "Added " << pkgName << "@" << pkgVersion << " to " << section << "\n";
    }
    writeSmallFile("forge.json", json);
    return 0;
}

static int cmdPublish() {
    std::string json = readSmallFile("forge.json");
    if (json.empty()) {
        std::cerr << "Error: No forge.json found.\n";
        return 1;
    }
    const char* token = std::getenv("FORGE_TOKEN");
    if (!token) {
        std::cerr << "Error: FORGE_TOKEN environment variable required.\n";
        std::cerr << "Run 'forge pkg login' to authenticate.\n";
        return 1;
    }
    std::cout << "Publishing to registry...\n";
    std::cout << "Note: Publish not yet available.\n";
    return 0;
}

static int cmdCacheClean() {
    std::string cacheDir = getCacheDir();
    if (fs::exists(cacheDir)) {
        fs::remove_all(cacheDir);
        std::cout << "Cache cleaned: " << cacheDir << "\n";
    } else {
        std::cout << "Cache is already clean.\n";
    }
    return 0;
}

static int cmdInstallAll() {
    std::string json = readSmallFile("forge.json");
    if (json.empty()) {
        std::cerr << "Error: No forge.json found. Run 'forge pkg init' first.\n";
        return 1;
    }

    std::string deps = getOrCreateEmptyObject(json, "dependencies");
    auto entries = jsonExtractObjects(deps);

    if (entries.empty()) {
        std::cout << "No dependencies to install.\n";
        return 0;
    }

    std::cout << "Installing dependencies from forge.json...\n\n";

    int installed = 0, failed = 0;
    for (auto& [name, _] : entries) {
        // Check if already installed
        if (isInstalled(name)) {
            std::cout << "  " << name << " (already installed)\n";
            continue;
        }

        // Simulate install for each dependency
        Registry reg;
        if (!reg.loadFromCache()) reg.refresh();
        reg.parse();

        PkgInfo* pkg = reg.find(name);
        if (pkg) {
            installPkgFiles(*pkg);
            InstalledPkg newPkg;
            newPkg.name = pkg->name;
            newPkg.version = pkg->version;
            time_t now = time(nullptr);
            char timeBuf[64];
            strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%S", localtime(&now));
            newPkg.installedAt = timeBuf;

            auto installedPkgs = loadInstalled();
            installedPkgs.push_back(newPkg);
            saveInstalled(installedPkgs);
            installed++;
        } else {
            std::cerr << "  " << name << " not found in registry\n";
            failed++;
        }
    }

    std::cout << "\nDone. " << installed << " installed, " << failed << " failed.\n";
    return 0;
}

// ============================================================
// Main entry
// ============================================================

int pkg_main(int argc, char* argv[]) {
    if (argc < 1) {
        printUsage();
        return 0;
    }

    std::string cmd = argv[0];

    if (cmd == "init") {
        return cmdInit(argc - 1, argv + 1);
    } else if (cmd == "search" || cmd == "s") {
        return cmdSearch(argc - 1, argv + 1);
    } else if (cmd == "install" || cmd == "i") {
        if (argc < 2) {
            // No package specified — install from forge.json
            return cmdInstallAll();
        }
        return cmdInstall(argc - 1, argv + 1);
    } else if (cmd == "remove" || cmd == "rm") {
        return cmdRemove(argc - 1, argv + 1);
    } else if (cmd == "list" || cmd == "ls") {
        return cmdList();
    } else if (cmd == "update" || cmd == "up") {
        return cmdUpdate();
    } else if (cmd == "add") {
        return cmdAdd(argc - 1, argv + 1);
    } else if (cmd == "publish") {
        return cmdPublish();
    } else if (cmd == "cache") {
        if (argc > 1 && std::string(argv[1]) == "clean") {
            return cmdCacheClean();
        }
        std::cerr << "Usage: forge pkg cache clean\n";
        return 1;
    } else if (cmd == "login") {
        std::cout << "Login to Forge registry\n";
        std::cout << "Enter token: ";
        std::string token;
        std::getline(std::cin, token);
        if (token.empty()) {
            std::cerr << "Token required.\n";
            return 1;
        }
        std::string tokenDir = getForgeDir();
        fs::create_directories(tokenDir);
        // Write with owner-only permissions (0600) — never world-readable
        std::string tokenPath = tokenDir + "/token";
#ifndef _WIN32
        int fd = open(tokenPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd < 0) {
            std::cerr << "Error: Cannot write to " << tokenPath << "\n";
            return 1;
        }
        ssize_t written = write(fd, token.data(), token.size());
        close(fd);
        if (written < 0) {
            std::cerr << "Error: Cannot write to " << tokenPath << "\n";
            return 1;
        }
#else
        writeSmallFile(tokenPath, token);
#endif
        std::cout << "Login successful.\n";
        return 0;
    } else if (cmd == "logout") {
        std::string tokenFile = getForgeDir() + "/token";
        if (fs::exists(tokenFile)) fs::remove(tokenFile);
        std::cout << "Logged out.\n";
        return 0;
    } else if (cmd == "whoami") {
        std::string token = readSmallFile(getForgeDir() + "/token");
        if (!token.empty()) {
            std::cout << "Logged in (token found).\n";
        } else {
            std::cout << "Not logged in.\n";
        }
        return 0;
    } else {
        std::cerr << "Unknown command: " << cmd << "\n\n";
        printUsage();
        return 1;
    }
}
