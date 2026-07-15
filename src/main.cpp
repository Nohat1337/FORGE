#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "lexer.hpp"
#include "parser.hpp"
#include "compiler.hpp"
#include "fvm/runtime.hpp"
#include "fvm/classfile.hpp"
#include "fvm/sdl2_ui.hpp"
#include "fvm/forge_ide.hpp"
#include "pkg_manager.hpp"

#define FORGE_VERSION "1.0.0"

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) throw std::runtime_error("Cannot open file: " + path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void repl() {
    forge::fvm::ForgeVM vm;

    std::cout << "Forge v" << FORGE_VERSION << " (REPL)\n";
    std::cout << "Type 'exit' or Ctrl+D to quit.\n\n";

    std::string line;
    while (true) {
        std::cout << ">> ";
        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            break;
        }

        if (line.empty()) continue;
        if (line == "exit" || line == "quit") break;

        try {
            if (!vm.interpretSource(line, "<repl>")) {
                return;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
        }
    }
}

void runString(const std::string& code) {
    forge::fvm::ForgeVM vm;
    vm.interpretSource(code, "<string>");
}

int main(int argc, char* argv[]) {
    // Handle command line flags
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--version" || arg == "-v") {
            std::cout << "Forge Programming Language v" << FORGE_VERSION << "\n";
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: forge [options] [file.fge | file.fclass]\n";
            std::cout << "Options:\n";
            std::cout << "  -v, --version    Show version\n";
            std::cout << "  -h, --help       Show this help\n";
            std::cout << "  -e <code>        Execute string as code\n";
            std::cout << "  --sdl            Launch SDL2 window\n";
            std::cout << "  --ide            Launch Forge IDE\n";
            std::cout << "  pkg <cmd>        Package manager commands\n";
            std::cout << "  (no args)        Start REPL\n";
            return 0;
        } else if (arg == "-e") {
            if (i + 1 >= argc) {
                std::cerr << "Error: -e requires code argument\n";
                return 1;
            }
            runString(argv[++i]);
            return 0;
        } else if (arg == "--sdl" || arg == "gui") {
            return forge::fvm::runSdlGui();
        } else if (arg == "--ide") {
            return forge::fvm::runForgeIDE();
        } else if (arg == "--screenshot") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --screenshot requires a file path\n";
                return 1;
            }
            return forge::fvm::runSdlGui(argv[++i]);
        } else if (arg == "pkg") {
            int pkgArgc = argc - i - 1;
            char** pkgArgv = argv + i + 1;
            return pkg_main(pkgArgc, pkgArgv);
        }
    }
    
    if (argc < 2) {
        repl();
        return 0;
    }

    try {
        forge::fvm::ForgeVM vm;
        std::string filename = argv[1];
        if (filename.size() >= 7 && filename.substr(filename.size() - 7) == ".fclass") {
            if (!vm.interpretClassFile(filename)) {
                return 1;
            }
        } else {
            std::string source = readFile(filename);
            if (!vm.interpretSource(source, filename)) {
                return 1;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
