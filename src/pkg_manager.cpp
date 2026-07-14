#include "pkg_manager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

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

static std::string getFieldValue(const std::string& json, const std::string& key) {
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
    std::string result = obj.substr(0, lineStart) + obj.substr(lineEnd);
    return result;
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

static void printUsage() {
    std::cout << "Forge Package Manager\n";
    std::cout << "Usage: forge pkg <command> [args]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  init [name] [version]    Create a new forge.json\n";
    std::cout << "  add <pkg>[@version]      Add a dependency\n";
    std::cout << "  add --dev <pkg>[@ver]    Add a dev dependency\n";
    std::cout << "  remove <pkg>             Remove a dependency\n";
    std::cout << "  install                  Install all dependencies\n";
    std::cout << "  search <query>           Search packages (registry)\n";
    std::cout << "  publish                  Publish package (registry)\n";
    std::cout << "  cache clean              Clean package cache\n";
}

static std::string getHomeDir() {
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    return home ? std::string(home) : ".";
}

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

static int cmdAdd(int argc, char* argv[]) {
    bool dev = false;
    std::vector<std::string> pkgs;

    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--dev") { dev = true; }
        else { pkgs.push_back(a); }
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

static int cmdRemove(int argc, char* argv[]) {
    if (argc < 1) {
        std::cerr << "Error: Package name required\n";
        return 1;
    }
    std::string pkgName = argv[0];

    std::string json = readSmallFile("forge.json");
    if (json.empty()) {
        std::cerr << "Error: No forge.json found.\n";
        return 1;
    }

    json = removeFromJsonObject(json, pkgName);
    writeSmallFile("forge.json", json);
    std::cout << "Removed " << pkgName << "\n";
    return 0;
}

static int cmdInstall() {
    std::string json = readSmallFile("forge.json");
    if (json.empty()) {
        std::cerr << "Error: No forge.json found. Run 'forge pkg init' first.\n";
        return 1;
    }

    std::cout << "Reading forge.json...\n";

    std::string deps = getOrCreateEmptyObject(json, "dependencies");
    std::string devDeps = getOrCreateEmptyObject(json, "devDependencies");

    int depCount = 0;
    for (size_t i = 0; i < deps.size(); i++) {
        if (deps[i] == '"') depCount++;
    }
    depCount /= 2;

    int devCount = 0;
    for (size_t i = 0; i < devDeps.size(); i++) {
        if (devDeps[i] == '"') devCount++;
    }
    devCount /= 2;

    if (depCount == 0 && devCount == 0) {
        std::cout << "No dependencies to install.\n";
        return 0;
    }

    std::cout << "Found " << depCount << " dependencies, " << devCount << " dev dependencies\n";

    std::cout << "\nNote: Remote registry not yet available.\n";
    std::cout << "Dependencies listed in forge.json:\n";
    if (deps != "{}" && deps != "{\n  }") {
        std::cout << "  [dependencies]\n" << deps << "\n";
    }
    if (devDeps != "{}" && devDeps != "{\n  }") {
        std::cout << "  [devDependencies]\n" << devDeps << "\n";
    }

    fs::create_directories(getHomeDir() + "/.forge/cache");
    std::cout << "Package cache: " << getHomeDir() << "/.forge/cache/\n";
    std::cout << "Done.\n";
    return 0;
}

static int cmdSearch(int argc, char* argv[]) {
    if (argc < 1) {
        std::cerr << "Error: Search query required\n";
        return 1;
    }
    std::cout << "Searching for: " << argv[0] << "\n";
    std::cout << "Registry: https://registry.forge-lang.org\n";
    std::cout << "Note: Remote registry not yet available.\n";
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
    std::cout << "Note: Remote registry not yet available.\n";
    return 0;
}

static int cmdCacheClean() {
    std::string cacheDir = getHomeDir() + "/.forge/cache";
    if (fs::exists(cacheDir)) {
        fs::remove_all(cacheDir);
        std::cout << "Cache cleaned: " << cacheDir << "\n";
    } else {
        std::cout << "Cache is already clean.\n";
    }
    return 0;
}

int pkg_main(int argc, char* argv[]) {
    if (argc < 1) {
        printUsage();
        return 0;
    }

    std::string cmd = argv[0];

    if (cmd == "init") {
        return cmdInit(argc - 1, argv + 1);
    } else if (cmd == "add") {
        return cmdAdd(argc - 1, argv + 1);
    } else if (cmd == "remove" || cmd == "rm") {
        return cmdRemove(argc - 1, argv + 1);
    } else if (cmd == "install" || cmd == "i") {
        return cmdInstall();
    } else if (cmd == "search") {
        return cmdSearch(argc - 1, argv + 1);
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
        std::string tokenDir = getHomeDir() + "/.forge";
        fs::create_directories(tokenDir);
        writeSmallFile(tokenDir + "/token", token);
        std::cout << "Login successful.\n";
        return 0;
    } else if (cmd == "logout") {
        std::string tokenFile = getHomeDir() + "/.forge/token";
        if (fs::exists(tokenFile)) {
            fs::remove(tokenFile);
        }
        std::cout << "Logged out.\n";
        return 0;
    } else if (cmd == "whoami") {
        std::string tokenFile = getHomeDir() + "/.forge/token";
        std::string token = readSmallFile(tokenFile);
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
