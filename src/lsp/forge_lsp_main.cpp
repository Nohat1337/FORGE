#include "forge_lsp.hpp"
#include <iostream>

int main(int argc, char** argv) {
    // Check for --stdio flag
    bool useStdio = false;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--stdio") {
            useStdio = true;
            break;
        }
    }
    
    forge::lsp::ForgeLSP server;
    
    if (useStdio) {
        server.run();
    } else {
        // TCP mode - not implemented yet
        std::cerr << "TCP mode not implemented. Use --stdio for stdin/stdout mode.\n";
        return 1;
    }
    
    return 0;
}