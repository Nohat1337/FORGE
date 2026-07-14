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
};

int build_main(int argc, char* argv[]);

} // namespace forge::build
