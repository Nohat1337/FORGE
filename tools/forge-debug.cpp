#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <functional>
#include "lexer.hpp"
#include "parser.hpp"
#include "compiler.hpp"
#include "chunk.hpp"
#include "value.hpp"
#include "vm.hpp"

struct Breakpoint {
    std::string file;
    int line;
};

struct WatchVar {
    std::string name;
};

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file: " << path << "\n";
        exit(1);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static std::string getSourceLine(const std::string& source, int line) {
    std::istringstream stream(source);
    std::string l;
    int current = 1;
    while (std::getline(stream, l)) {
        if (current == line) return l;
        current++;
    }
    return "";
}

static void showSourceLocation(const std::string& file, const std::string& source, int line, const std::string& func) {
    std::cerr << "[" << file << ":" << line << "] in " << func << "\n";
    std::string srcLine = getSourceLine(source, line);
    if (!srcLine.empty()) {
        std::cerr << "  " << line << " | " << srcLine << "\n";
    }
}

static void printCallStack(VM& vm) {
    std::cerr << "Call stack:\n";
    for (int i = vm.frameCount - 1; i >= 0; i--) {
        auto& frame = vm.frames[i];
        if (!frame.closure || !frame.closure->function) continue;
        auto& fn = frame.closure->function;
        int line = -1;
        if (frame.ip && fn->chunk) {
            int offset = (int)(frame.ip - fn->chunk->code.data()) - 1;
            if (offset >= 0 && offset < (int)fn->chunk->lines.size()) {
                line = fn->chunk->lines[offset];
            }
        }
        std::cerr << "  #" << (vm.frameCount - 1 - i) << " " << fn->name;
        if (line > 0) std::cerr << " (line " << line << ")";
        std::cerr << "\n";
    }
}

static void printLocals(VM& vm) {
    if (vm.frameCount == 0) {
        std::cerr << "No active frame.\n";
        return;
    }
    auto& frame = vm.frames[vm.frameCount - 1];
    if (!frame.closure || !frame.closure->function) return;

    Value* stackBegin = vm.stack.data();
    Value* stackEnd = stackBegin + vm.stack.size();
    if (frame.slots < stackBegin || frame.slots >= stackEnd) {
        std::cerr << "No valid frame slots.\n";
        return;
    }
    int available = (int)(stackEnd - frame.slots);
    if (available <= 1) {
        std::cerr << "No local variables.\n";
        return;
    }
    for (int i = 1; i < available; i++) {
        Value val = frame.slots[i];
        std::cerr << "  [" << i << "] = " << val.toString() << "\n";
    }
}

static void printGlobals(VM& vm) {
    for (auto& [name, val] : vm.globals) {
        std::cerr << "  " << name << " = " << val.toString() << "\n";
    }
}

static void showWatches(VM& vm, const std::vector<WatchVar>& watches) {
    for (auto& w : watches) {
        bool found = false;
        for (auto& [name, val] : vm.globals) {
            if (name == w.name) {
                std::cerr << "  " << w.name << " = " << val.toString() << "\n";
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "  " << w.name << " = <not in global scope>\n";
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: forge-debug [options] <file.fge>\n";
        std::cerr << "Options:\n";
        std::cerr << "  --breakpoint file:line   Set a breakpoint\n";
        std::cerr << "  --step                   Step through line by line\n";
        std::cerr << "  --watch varname          Watch a variable\n";
        std::cerr << "\nInteractive commands (during debug):\n";
        std::cerr << "  s          Step into\n";
        std::cerr << "  n          Step over (next)\n";
        std::cerr << "  c          Continue\n";
        std::cerr << "  p [expr]   Print globals/expression\n";
        std::cerr << "  b [file:l] Set/show breakpoint\n";
        std::cerr << "  w [var]    Set/show watch\n";
        std::cerr << "  l          Show call stack and locals\n";
        std::cerr << "  h          Help\n";
        std::cerr << "  q          Quit\n";
        return 1;
    }

    std::vector<Breakpoint> breakpoints;
    std::vector<WatchVar> watches;
    std::string filename;
    bool stepMode = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--breakpoint" && i + 1 < argc) {
            i++;
            std::string bp = argv[i];
            size_t colonPos = bp.find(':');
            if (colonPos != std::string::npos) {
                std::string file = bp.substr(0, colonPos);
                int line = std::stoi(bp.substr(colonPos + 1));
                breakpoints.push_back({file, line});
            } else {
                int line = std::stoi(bp);
                breakpoints.push_back({".", line});
            }
        } else if (arg == "--step") {
            stepMode = true;
        } else if (arg == "--watch" && i + 1 < argc) {
            i++;
            watches.push_back({argv[i]});
        } else if (arg[0] != '-') {
            filename = arg;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }

    if (filename.empty()) {
        std::cerr << "Error: No file specified.\n";
        return 1;
    }

    std::string source = readFile(filename);

    try {
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

        bool stepping = true;
        bool firstStop = true;
        bool running = true;

        auto debugHook = [&](int line, const std::string& function, VM& vmRef) -> bool {
            bool shouldStop = false;

            if (firstStop) {
                shouldStop = true;
                firstStop = false;
            } else if (stepping) {
                shouldStop = true;
            } else {
                for (auto& bp : breakpoints) {
                    if (bp.line == line) {
                        shouldStop = true;
                        break;
                    }
                }
            }

            if (shouldStop) {
                showSourceLocation(filename, source, line, function);
                showWatches(vmRef, watches);

                while (true) {
                    std::cerr << "(forge-db) ";
                    std::string input;
                    if (!std::getline(std::cin, input)) {
                        running = false;
                        return false;
                    }
                    if (input.empty()) continue;

                    char cmd = input[0];
                    std::string arg;
                    size_t spacePos = input.find(' ');
                    if (spacePos != std::string::npos) {
                        arg = input.substr(spacePos + 1);
                    }

                    switch (cmd) {
                        case 's':
                            stepping = true;
                            return true;
                        case 'n':
                            stepping = false;
                            return true;
                        case 'c':
                            stepping = false;
                            return true;
                        case 'p': {
                            if (arg.empty()) {
                                printLocals(vmRef);
                                std::cerr << "--- Globals ---\n";
                                printGlobals(vmRef);
                            } else {
                                bool found = false;
                                for (auto& [name, val] : vmRef.globals) {
                                    if (name == arg) {
                                        std::cerr << arg << " = " << val.toString() << "\n";
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found) std::cerr << "Cannot resolve '" << arg << "'\n";
                            }
                            break;
                        }
                        case 'b': {
                            if (arg.empty()) {
                                std::cerr << "Breakpoints:\n";
                                for (size_t i = 0; i < breakpoints.size(); i++) {
                                    std::cerr << "  " << (i + 1) << ": " << breakpoints[i].file << ":" << breakpoints[i].line << "\n";
                                }
                            } else {
                                size_t colonPos = arg.find(':');
                                if (colonPos != std::string::npos) {
                                    std::string file = arg.substr(0, colonPos);
                                    int bline = std::stoi(arg.substr(colonPos + 1));
                                    breakpoints.push_back({file, bline});
                                    std::cerr << "Breakpoint set at " << file << ":" << bline << "\n";
                                } else {
                                    int bline = std::stoi(arg);
                                    breakpoints.push_back({filename, bline});
                                    std::cerr << "Breakpoint set at " << filename << ":" << bline << "\n";
                                }
                            }
                            break;
                        }
                        case 'w': {
                            if (arg.empty()) {
                                std::cerr << "Watches:\n";
                                for (auto& w : watches) {
                                    std::cerr << "  " << w.name << "\n";
                                }
                            } else {
                                watches.push_back({arg});
                                std::cerr << "Watching '" << arg << "'\n";
                            }
                            break;
                        }
                        case 'l':
                            printCallStack(vmRef);
                            printLocals(vmRef);
                            break;
                        case 'q':
                            std::cerr << "Quit.\n";
                            return false;
                        case 'h':
                            std::cerr << "Commands:\n";
                            std::cerr << "  s          - Step into\n";
                            std::cerr << "  n          - Step over (next)\n";
                            std::cerr << "  c          - Continue\n";
                            std::cerr << "  p [expr]   - Print expression/globals\n";
                            std::cerr << "  b [file:l] - Set/show breakpoint\n";
                            std::cerr << "  w [var]    - Set/show watch\n";
                            std::cerr << "  l          - Show call stack and locals\n";
                            std::cerr << "  q          - Quit\n";
                            break;
                        default:
                            std::cerr << "Unknown command '" << cmd << "'. Type 'h' for help.\n";
                    }
                }
            }
            return true;
        };

        vm.setDebugHook(debugHook);
        bool success = vm.interpret(fn);
        return success ? 0 : 1;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
