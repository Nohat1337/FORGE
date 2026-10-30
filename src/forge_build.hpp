#pragma once
#include <string>
#include <vector>

namespace forge::build {

struct BuildConfig {
    std::string projectName = "app";
    std::string version = "1.0.0";
    std::vector<std::string> sourceDirs = {"src"};
    std::vector<std::string> sourceFiles;  // explicit file list (overrides sourceDirs)
    std::string outputName = "app";
    std::string arch = "x64";
    std::string compiler = "g++";
    std::vector<std::string> extraFlags;
    std::string buildDir = "build";
    bool release = true;
    bool verbose = false;
    bool shared = false;   // export as a shared library (.so) with a C ABI

    // Forgefile (single-file project) fields
    std::vector<std::string> modules;      // forge runtime module names (io, json, ...)
    std::vector<std::string> nativeLibs;   // native libs to link (uselib)
    std::string libsDir;                   // where relative uselib paths resolve (default /usr/forge/libs)
    std::string fvmBin;                    // path to forgevm binary driving per-file compiles
    enum class Flavor { Legacy, Forgefile } flavor = Flavor::Legacy;

    // Session control (the single file REALIZEs which pipeline to run)
    enum class Session { Compile, Decompile, C2Forge, Forge2C } session = Session::Compile;
    std::vector<std::string> inputs;       // artifacts to work on (binary to decompile, sources to convert)
    std::string destination;               // where session products are placed
    bool autoBuild = true;                 // decompile/c2forge/forge2c sessions chain into a compile
};

int build_main(int argc, char* argv[]);

} // namespace forge::build
