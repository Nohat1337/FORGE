#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "lexer.hpp"
#include "parser.hpp"
#include "compiler.hpp"
#include "vm.hpp"

#define FORGE_VERSION "0.2.0"

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) throw std::runtime_error("Cannot open file: " + path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void repl() {
    Compiler compiler;
    compiler.resetForRepl();
    VM vm;

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
            Lexer lexer(line);
            auto tokens = lexer.tokenize();
            Parser parser(tokens);
            auto program = parser.parse();
            auto fn = compiler.compile(program);
            if (compiler.hasError()) {
                std::cerr << "Compile Error: " << compiler.errorMessage() << "\n";
                continue;
            }
            vm.interpret(fn);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        repl();
        return 0;
    }

    try {
        std::string source = readFile(argv[1]);
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Compiler compiler;
        auto fn = compiler.compile(program);
        if (compiler.hasError()) {
            std::cerr << "Compile Error: " << compiler.errorMessage() << "\n";
            return 1;
        }
        VM vm;
        if (!vm.interpret(fn)) {
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
