#include "fvm/runtime.hpp"
#include "pkg_manager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#define FORGEVM_VERSION "2.0.0"

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) throw std::runtime_error("Cannot open file: " + path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static void repl() {
    forge::fvm::ForgeVM vm;

    std::cout << "Forge VM v" << FORGEVM_VERSION << " (FVM REPL)\n";
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

        vm.interpretSource(line, "<repl>");
    }
}

static void runString(const std::string& code) {
    forge::fvm::ForgeVM vm;
    vm.interpretSource(code, "<string>");
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--version" || arg == "-v") {
            std::cout << "Forge VM v" << FORGEVM_VERSION << " (FVM)\n";
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: forgevm [options] [file.fge]\n";
            std::cout << "Forge Virtual Machine - Production bytecode interpreter\n\n";
            std::cout << "Options:\n";
            std::cout << "  -v, --version    Show version\n";
            std::cout << "  -h, --help       Show this help\n";
            std::cout << "  -e <code>        Execute string as code\n";
            std::cout << "  pkg <cmd>        Package manager commands\n";
            std::cout << "  --gc-stats       Show GC statistics after execution\n";
            std::cout << "  (no args)        Start REPL\n";
            return 0;
        } else if (arg == "-e") {
            if (i + 1 >= argc) {
                std::cerr << "Error: -e requires code argument\n";
                return 1;
            }
            runString(argv[++i]);
            return 0;
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

    bool showGcStats = false;
    std::string filename;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--gc-stats") {
            showGcStats = true;
        } else if (filename.empty()) {
            filename = arg;
        }
    }

    if (filename.empty()) {
        repl();
        return 0;
    }

    try {
        forge::fvm::ForgeVM vm;
        if (!vm.interpretSource(readFile(filename), filename)) {
            return 1;
        }
        if (showGcStats) {
            auto& gc = vm.gc();
            std::cout << "\n--- GC Stats ---\n";
            std::cout << "Heap allocated: " << gc.getAllocated() << " bytes\n";
            std::cout << "Heap size: " << gc.getHeapSize() << " bytes\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
