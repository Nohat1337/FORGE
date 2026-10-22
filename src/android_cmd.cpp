#include "android_cmd.hpp"

#include "java_backend.hpp"
#include "lexer.hpp"
#include "parser.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "android_templates.h"

namespace fs = std::filesystem;

namespace forge {
namespace androidcmd {

namespace {

// ---------------------------------------------------------------
// helpers
// ---------------------------------------------------------------

std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

void writeFile(const fs::path& p, const std::string& content) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p);
    if (!out) {
        std::cerr << "Error: cannot write " << p.string() << "\n";
        return;
    }
    out << content;
}

bool writeTemplate(const fs::path& p, const std::string& tpl,
                   const std::string& package, const std::string& appName) {
    std::string body = replaceAll(tpl, "{{PACKAGE}}", package);
    body = replaceAll(body, "{{APP_NAME}}", appName);
    writeFile(p, body);
    return true;
}

std::string pkgToPath(const std::string& pkg) {
    std::string out;
    for (char c : pkg) {
        if (c == '.') out += '/';
        else out += c;
    }
    return out;
}

bool validPackage(const std::string& p) {
    if (p.empty() || p.size() > 120) return false;
    for (char c : p)
        if (!std::isalnum((unsigned char)c) && c != '.' && c != '_') return false;
    if (p.front() == '.' || p.back() == '.') return false;
    if (p.find("..") != std::string::npos) return false;
    return true;
}

std::string detectSdk() {
    const char* envs[] = { "ANDROID_HOME", "ANDROID_SDK_ROOT" };
    for (const char* e : envs) {
        const char* v = std::getenv(e);
        if (v && *v && fs::exists(fs::path(v) / "platforms")) return v;
    }
    const char* home = std::getenv("HOME");
    if (home) {
        fs::path p = fs::path(home) / "Android" / "sdk";
        if (fs::exists(p / "platforms")) return p.string();
        p = fs::path(home) / "Android" / "Sdk";
        if (fs::exists(p / "platforms")) return p.string();
    }
    for (const char* c : { "/opt/android-sdk", "/usr/lib/android-sdk" }) {
        if (fs::exists(fs::path(c) / "platforms")) return c;
    }
    return "";
}

std::string shellQuote(const std::string& s) {
    if (s.empty()) return "''";
    bool safe = true;
    for (char c : s)
        if (!(std::isalnum((unsigned char)c) || std::strchr("_-./=:@,+", c))) { safe = false; break; }
    if (safe) return s;
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

int runCommand(const std::vector<std::string>& argv, const std::string& cwd = "") {
    if (argv.empty()) return -1;
    std::string line;
    for (size_t i = 0; i < argv.size(); i++) {
        if (i) line += " ";
        line += shellQuote(argv[i]);
    }

    std::cout << "$ " << line << std::endl;

    int rc = -1;
    if (!cwd.empty()) {
        fs::path prev = fs::current_path();
        fs::current_path(cwd);
        rc = ::system(line.c_str());
        fs::current_path(prev);
    } else {
        rc = ::system(line.c_str());
    }
    return rc;
}

std::string gradleCommand() {
    if (std::getenv("FORGE_GRADLE")) return std::getenv("FORGE_GRADLE");
    // prefer the wrapper if the project has one, else the system gradle
    return "gradle";
}

std::string adbCommand() {
    if (std::getenv("FORGE_ADB")) return std::getenv("FORGE_ADB");
    return "adb";
}

// ---------------------------------------------------------------
// project generation
// ---------------------------------------------------------------

std::string transpileForge(const fs::path& src, const std::string& packageName) {
    std::ifstream in(src);
    if (!in.is_open()) return "";
    std::stringstream buf;
    buf << in.rdbuf();

    Lexer lexer(buf.str());
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto program = parser.parse();

    std::string java = javabackend::transpileToJava(program, "ForgeMain");
    // insert the package declaration (the generator emits a default-package class)
    size_t imp = java.find("import java.util.*;");
    if (imp != std::string::npos)
        java.insert(imp, "package " + packageName + ";\n\n");
    else
        java = "package " + packageName + ";\n\n" + java;
    return java;
}

int cmdNew(const std::string& dir, const std::string& packageName,
           const std::string& appName, const std::string& source) {
    if (!validPackage(packageName)) {
        std::cerr << "Error: invalid package name '" << packageName << "'\n";
        return 1;
    }
    fs::path root(dir);
    fs::path appDir = root / "app";
    fs::path srcDir = appDir / "src" / "main" / "java" / pkgToPath(packageName);
    fs::path resDir = appDir / "src" / "main" / "res" / "values";

    fs::create_directories(srcDir);
    fs::create_directories(resDir);

    // gradle project
    writeTemplate(root / "settings.gradle.kts", TPL_settings_gradle_kts, packageName, appName);
    writeTemplate(root / "build.gradle.kts", TPL_build_gradle_kts, packageName, appName);
    writeTemplate(root / "gradle.properties", TPL_gradle_properties, packageName, appName);
    writeTemplate(appDir / "build.gradle.kts", TPL_app_build_gradle_kts, packageName, appName);

    // manifest + resources
    writeTemplate(appDir / "src" / "main" / "AndroidManifest.xml", TPL_AndroidManifest,
                  packageName, appName);
    writeTemplate(resDir / "strings.xml", TPL_strings_xml, packageName, appName);
    writeTemplate(resDir / "colors.xml", TPL_colors_xml, packageName, appName);
    writeTemplate(resDir / "themes.xml", TPL_themes_xml, packageName, appName);

    // java + kotlin sources
    writeTemplate(srcDir / "ForgeGui.kt", TPL_ForgeGui_kt, packageName, appName);
    writeTemplate(srcDir / "MainActivity.kt", TPL_MainActivity_kt, packageName, appName);
    writeTemplate(srcDir / "ForgeWallpaper.kt", TPL_ForgeWallpaper_kt, packageName, appName);
    writeTemplate(srcDir / "LiquidWidgets.kt", TPL_LiquidWidgets_kt, packageName, appName);
    writeTemplate(srcDir / "LiquidButton.kt", TPL_LiquidButton_kt, packageName, appName);
    writeTemplate(srcDir / "LiquidToggle.kt", TPL_LiquidToggle_kt, packageName, appName);
    writeTemplate(srcDir / "LiquidSlider.kt", TPL_LiquidSlider_kt, packageName, appName);
    writeTemplate(srcDir / "LiquidBottomTabs.kt", TPL_LiquidBottomTabs_kt, packageName, appName);
    writeTemplate(srcDir / "LiquidBottomTab.kt", TPL_LiquidBottomTab_kt, packageName, appName);
    writeTemplate(srcDir / "LiquidHighlight.kt", TPL_LiquidHighlight_kt, packageName, appName);
    writeTemplate(srcDir / "LiquidDampedDrag.kt", TPL_LiquidDampedDrag_kt, packageName, appName);
    writeTemplate(srcDir / "LiquidDragInspector.kt", TPL_LiquidDragInspector_kt, packageName, appName);

    // the Forge program
    if (!source.empty()) {
        fs::path s(source);
        if (!fs::exists(s)) {
            std::cerr << "Error: no such file " << source << "\n";
            return 1;
        }
        try {
            std::string java = transpileForge(s, packageName);
            if (java.empty()) {
                std::cerr << "Error: failed to transpile " << source << "\n";
                return 1;
            }
            writeFile(srcDir / "ForgeMain.java", java);
            std::cout << "Transpiled " << source << " -> "
                      << (srcDir / "ForgeMain.java").string() << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Forge -> Java error: " << e.what() << "\n";
            return 1;
        }
    } else {
        writeFile(srcDir / "ForgeMain.java",
                  "package " + packageName + ";\n\n"
                  "public final class ForgeMain {\n"
                  "    public static void main(String[] args) {\n"
                  "        // replace this with your Forge program:\n"
                  "        //   forgevm android new <dir> --source app.fge\n"
                  "    }\n"
                  "}\n");
    }

    // local.properties -> the SDK the build uses
    std::string sdk = detectSdk();
    if (!sdk.empty())
        writeFile(root / "local.properties", "sdk.dir=" + sdk + "\n");

    std::cout << "\nAndroid project created: " << root.string() << "\n"
              << "  package: " << packageName << "\n";
    if (!sdk.empty()) std::cout << "  sdk:     " << sdk << "\n";
    std::cout << "\nNext:\n"
              << "  forgevm android build " << dir << "     # gradle assembleDebug\n"
              << "  forgevm android install " << dir << "   # adb install\n"
              << "  forgevm android run " << dir << "       # install + launch\n";
    return 0;
}

// ---------------------------------------------------------------
// build / install / run
// ---------------------------------------------------------------

int cmdBuild(const std::string& dir, bool release) {
    fs::path root(dir);
    if (!fs::exists(root / "settings.gradle.kts")) {
        std::cerr << "Error: " << dir << " is not a Forge Android project "
                     "(run `forgevm android new` first)\n";
        return 1;
    }
    std::string sdk = detectSdk();
    std::vector<std::string> cmd = { gradleCommand(), "--console=plain" };
    if (!sdk.empty()) {
        cmd.push_back("-Dandroid.sdk.dir=" + sdk);
    }
    cmd.push_back(release ? "assembleRelease" : "assembleDebug");

    int rc = runCommand(cmd, root.string());
    if (rc != 0) {
        std::cerr << "\nBuild failed.\n";
        return 1;
    }
    fs::path apk = root / "app" / "build" / "outputs" / "apk" /
                   (release ? "release" : "debug") /
                   (release ? "app-release-unsigned.apk" : "app-debug.apk");
    if (fs::exists(apk))
        std::cout << "\nAPK: " << apk.string() << "\n";
    else
        std::cout << "\nBuild finished (apk not found at " << apk.string() << ")\n";
    return 0;
}

int cmdInstall(const std::string& dir, bool release) {
    fs::path apk = fs::path(dir) / "app" / "build" / "outputs" / "apk" /
                   (release ? "release" : "debug") /
                   (release ? "app-release-unsigned.apk" : "app-debug.apk");
    if (!fs::exists(apk)) {
        std::cerr << "Error: no APK at " << apk.string() << " (build it first)\n";
        return 1;
    }
    return runCommand({ adbCommand(), "install", "-r", apk.string() }) == 0 ? 0 : 1;
}

int cmdRun(const std::string& dir, const std::string& packageName) {
    int rc = cmdInstall(dir, false);
    if (rc != 0) return rc;
    std::string pkg = packageName;
    if (pkg.empty()) {
        // read the namespace from the app build file
        std::ifstream f(fs::path(dir) / "app" / "build.gradle.kts");
        std::string line;
        while (std::getline(f, line)) {
            size_t p = line.find("namespace");
            if (p != std::string::npos) {
                size_t q = line.find('"', p);
                size_t e = line.find('"', q + 1);
                if (q != std::string::npos && e != std::string::npos)
                    pkg = line.substr(q + 1, e - q - 1);
                break;
            }
        }
    }
    if (pkg.empty()) {
        std::cerr << "Error: cannot determine the application id\n";
        return 1;
    }
    return runCommand({ adbCommand(), "shell", "am", "start", "-n", pkg + "/.MainActivity" }) == 0 ? 0 : 1;
}

int cmdClean(const std::string& dir) {
    return runCommand({ gradleCommand(), "--console=plain", "clean" }, dir) == 0 ? 0 : 1;
}

void usage() {
    std::cout <<
        "Usage: forgevm android <command> [options]\n\n"
        "Commands:\n"
        "  new <dir> [--package p] [--name N] [--source file.fge]\n"
        "        Scaffold an Android project (Gradle + AGP) whose widgets are\n"
        "        rendered with the liquid glass library. --source transpiles a\n"
        "        Forge program into the app.\n"
        "  build [dir]        Build the APK (gradle assembleDebug)\n"
        "  install [dir]      Install the debug APK on the connected device\n"
        "  run [dir]          Install and launch the app\n"
        "  clean [dir]        Remove the build output\n\n"
        "Options:\n"
        "  --package <p>      Android application id (default com.forge.app)\n"
        "  --name <N>         App name (default: the directory name)\n"
        "  --source <file>    Forge program to embed in the app\n";
}

} // namespace

int android_main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 1;
    }
    std::string cmd = argv[1];
    std::string dir;
    std::string packageName;
    std::string appName;
    std::string source;
    bool release = false;

    for (int i = 2; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--package" && i + 1 < argc) { packageName = argv[++i]; }
        else if (a == "--name" && i + 1 < argc) { appName = argv[++i]; }
        else if (a == "--source" && i + 1 < argc) { source = argv[++i]; }
        else if (a == "--release") { release = true; }
        else if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (dir.empty()) { dir = a; }
        else { std::cerr << "Error: unexpected argument '" << a << "'\n"; return 1; }
    }

    if (cmd == "new") {
        if (dir.empty()) { std::cerr << "Error: `new` needs a project directory\n"; return 1; }
        if (packageName.empty()) packageName = "com.forge.app";
        if (appName.empty()) {
            appName = fs::path(dir).filename().string();
            if (appName.empty()) appName = "ForgeApp";
        }
        // keep the app name a valid android resource string
        for (char& c : appName)
            if (c == '\'' || c == '"' || c == '\\') c = '_';
        return cmdNew(dir, packageName, appName, source);
    }
    if (cmd == "build") { if (dir.empty()) dir = "."; return cmdBuild(dir, release); }
    if (cmd == "install") { if (dir.empty()) dir = "."; return cmdInstall(dir, release); }
    if (cmd == "run") { if (dir.empty()) dir = "."; return cmdRun(dir, packageName); }
    if (cmd == "clean") { if (dir.empty()) dir = "."; return cmdClean(dir); }

    std::cerr << "Error: unknown command '" << cmd << "'\n\n";
    usage();
    return 1;
}

} // namespace androidcmd
} // namespace forge
