#include "fvm/runtime.hpp"
#include "fvm/classfile.hpp"
#include "fvm/sdl2_ui.hpp"
#include "fvm/forge_ide.hpp"
#include "pkg_manager.hpp"
#include "forge_build.hpp"
#include "compiler.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <cstring>

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
        if (arg == "--sdl" || arg == "gui") {
            // Launch the FVM's SDL2 windowed UI. The FVM and its UI are
            // one package, so the same binary both runs .fge headless
            // and opens the cross-platform SDL2 window.
            return forge::fvm::runSdlGui();
        }
        if (arg == "--ide") {
            return forge::fvm::runForgeIDE();
        }
        if (arg == "--screenshot") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --screenshot requires a file path\n";
                return 1;
            }
            return forge::fvm::runSdlGui(argv[++i]);
        }
        if (arg == "--version" || arg == "-v") {
            std::cout << "Forge VM v" << FORGEVM_VERSION << " (FVM)\n";
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: forgevm [options] [file.fge | file.fclass]\n";
            std::cout << "Forge Virtual Machine - Production bytecode interpreter\n\n";
            std::cout << "Options:\n";
            std::cout << "  -v, --version    Show version\n";
            std::cout << "  -h, --help       Show this help\n";
            std::cout << "  -e <code>        Execute string as code\n";
            std::cout << "  compile <f> [o]  Compile .fge to .fclass bytecode\n";
            std::cout << "  pkg <cmd>        Package manager commands\n";
            std::cout << "  build            Build project from ForgeLists.txt\n";
            std::cout << "  --sdl, gui       Launch the SDL2 windowed UI for the FVM\n";
            std::cout << "  --ide            Launch the Forge IDE\n";
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
        } else if (arg == "build") {
            int buildArgc = argc - i - 1;
            char** buildArgv = argv + i + 1;
            return forge::build::build_main(buildArgc, buildArgv);
        } else if (arg == "compile") {
            if (i + 1 >= argc) {
                std::cerr << "Error: compile requires a .fge file\n";
                return 1;
            }
            std::string srcFile = argv[++i];
            std::string outFile;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                outFile = argv[++i];
            } else {
                outFile = srcFile;
                auto dot = outFile.rfind('.');
                if (dot != std::string::npos) outFile = outFile.substr(0, dot);
                outFile += ".fclass";
            }
            try {
                std::string source = readFile(srcFile);
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

                // Helper to convert a Value to a CPInfo
                auto valueToCP = [](const Value& vc, const std::vector<std::pair<const Chunk*, uint16_t>>& fnMap) -> forge::fvm::CPInfo {
                    forge::fvm::CPInfo cp;
                    switch (vc.type) {
                        case ValueType::VAL_NIL:
                            cp.tag = forge::fvm::CPTag::UTF8;
                            cp.data = {'n','i','l'};
                            break;
                        case ValueType::VAL_BOOL: {
                            cp.tag = forge::fvm::CPTag::BOOLEAN;
                            cp.data = {vc.as.boolean ? (uint8_t)1 : (uint8_t)0};
                            break;
                        }
                        case ValueType::VAL_INT: {
                            cp.tag = forge::fvm::CPTag::INTEGER;
                            int32_t v = (int32_t)vc.as.integer;
                            cp.data = {(uint8_t)((v>>24)&0xFF),(uint8_t)((v>>16)&0xFF),(uint8_t)((v>>8)&0xFF),(uint8_t)(v&0xFF)};
                            break;
                        }
                        case ValueType::VAL_FLOAT: {
                            cp.tag = forge::fvm::CPTag::DOUBLE;
                            cp.data.resize(8);
                            std::memcpy(cp.data.data(), &vc.as.floating, 8);
                            break;
                        }
                        case ValueType::VAL_OBJ: {
                            if (vc.as.obj && vc.as.obj->type == ObjType::STRING) {
                                auto* s = static_cast<ObjString*>(vc.as.obj);
                                cp.tag = forge::fvm::CPTag::UTF8;
                                cp.data = std::vector<uint8_t>(s->value.begin(), s->value.end());
                            } else if (vc.as.obj && vc.as.obj->type == ObjType::FUNCTION) {
                                auto* objFn = static_cast<ObjFunction*>(vc.as.obj);
                                uint16_t midx = 0;
                                for (auto& [chunk_ptr, idx] : fnMap) {
                                    if (chunk_ptr == objFn->chunk) { midx = idx; break; }
                                }
                                cp.tag = forge::fvm::CPTag::FORGE_METHOD;
                                cp.data = {(uint8_t)((midx >> 8) & 0xFF), (uint8_t)(midx & 0xFF)};
                            } else {
                                cp.tag = forge::fvm::CPTag::UTF8;
                                cp.data = {'?'};
                            }
                            break;
                        }
                        default:
                            cp.tag = forge::fvm::CPTag::UTF8;
                            cp.data = {'?'};
                            break;
                    }
                    return cp;
                };

                // Phase 1: Collect all functions (main + nested) in order
                struct FnEntry {
                    std::shared_ptr<ObjFunction> fn;
                    uint16_t methodIdx;
                };
                std::vector<FnEntry> allFns;
                std::vector<forge::fvm::MethodInfo> allMethods;

                // Add main function as method 0
                allFns.push_back({fn, 0});
                allMethods.resize(1);

                // Recursively discover nested functions
                std::function<void(const std::shared_ptr<ObjFunction>&)> discover =
                    [&](const std::shared_ptr<ObjFunction>& f) {
                        for (auto& vc : f->chunk->constants) {
                            if (vc.type == ValueType::VAL_OBJ && vc.as.obj &&
                                vc.as.obj->type == ObjType::FUNCTION) {
                                auto* inner = static_cast<ObjFunction*>(vc.as.obj);
                                auto innerShared = std::make_shared<ObjFunction>();
                                innerShared->name = inner->name;
                                innerShared->arity = inner->arity;
                                innerShared->localCount = inner->localCount;
                                innerShared->upvalueCount = inner->upvalueCount;
                                innerShared->chunk = inner->chunk;
                                innerShared->chunkPtr = inner->chunkPtr;
                                uint16_t idx = (uint16_t)allFns.size();
                                allFns.push_back({innerShared, idx});
                                allMethods.emplace_back();
                                discover(innerShared);
                            }
                        }
                    };
                discover(fn);

                // Build fn chunk → method index map (compare by chunk pointer)
                std::vector<std::pair<const Chunk*, uint16_t>> fnMap;
                for (auto& entry : allFns) {
                    fnMap.push_back({entry.fn->chunk, entry.methodIdx});
                }

                // Phase 2: Build the constant pool
                // Layout: CP[1..N] = main constants, then each nested fn's constants, then headers
                forge::fvm::ClassFile cf;
                cf.magic = forge::fvm::FCLASS_MAGIC;
                cf.minorVersion = forge::fvm::FCLASS_VERSION_MINOR;
                cf.majorVersion = forge::fvm::FCLASS_VERSION_MAJOR;
                cf.accessFlags = 0;
                cf.superClass = 0;
                cf.thisClass = 0;
                cf.constantPool.resize(1); // index 0 unused

                // Each method records where its constants start/end in the CP
                for (auto& entry : allFns) {
                    uint16_t cpOffset = (uint16_t)cf.constantPool.size();
                    for (auto& vc : entry.fn->chunk->constants) {
                        cf.constantPool.push_back(valueToCP(vc, fnMap));
                    }
                    entry.fn->chunk; // just accessing to track
                    allMethods[entry.methodIdx].constantPoolOffset = cpOffset;
                    allMethods[entry.methodIdx].constantPoolCount =
                        (uint16_t)(cf.constantPool.size() - cpOffset);
                }

                // Header entries at the end
                cf.constantPool.push_back({forge::fvm::CPTag::UTF8,
                    std::vector<uint8_t>(srcFile.begin(), srcFile.end())});
                cf.thisClass = (uint16_t)cf.constantPool.size() - 1;

                cf.constantPool.push_back({forge::fvm::CPTag::UTF8,
                    std::vector<uint8_t>({'C','o','d','e'})});
                uint16_t codeAttrNameIdx = (uint16_t)cf.constantPool.size() - 1;

                // Phase 3: Fill in method fields
                for (auto& entry : allFns) {
                    auto& method = allMethods[entry.methodIdx];
                    method.accessFlags = 0;
                    method.descriptorIndex = 0;
                    method.maxStack = 256;
                    method.maxLocals = entry.fn->localCount;
                    method.arity = entry.fn->arity;
                    method.upvalueCount = entry.fn->upvalueCount;
                    method.bytecode = entry.fn->chunk->code;

                    // Add method name as CP entry
                    cf.constantPool.push_back({forge::fvm::CPTag::UTF8,
                        std::vector<uint8_t>(entry.fn->name.begin(), entry.fn->name.end())});
                    method.nameIndex = (uint16_t)cf.constantPool.size() - 1;

                    // Add Code attribute
                    forge::fvm::AttributeInfo codeAttr;
                    codeAttr.nameIndex = codeAttrNameIdx;
                    codeAttr.type = forge::fvm::AttributeInfo::Type::CODE;
                    codeAttr.data.clear();
                    method.attributes.push_back(codeAttr);
                }

                cf.methods = allMethods;

                // Write the file
                std::vector<uint8_t> data;
                cf.save(data);
                std::ofstream out(outFile, std::ios::binary);
                if (!out.is_open()) {
                    std::cerr << "Error: Cannot write to " << outFile << "\n";
                    return 1;
                }
                out.write(reinterpret_cast<const char*>(data.data()), data.size());
                out.close();
                std::cout << "Compiled " << srcFile << " -> " << outFile << "\n";
                std::cout << "  " << data.size() << " bytes, " << cf.methods.size() << " method(s)\n";
                return 0;
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << "\n";
                return 1;
            }
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
        vm.setArgs(argc, argv);
        if (filename.size() >= 7 && filename.substr(filename.size() - 7) == ".fclass") {
            if (!vm.interpretClassFile(filename)) {
                return 1;
            }
        } else {
            if (!vm.interpretSource(readFile(filename), filename)) {
                return 1;
            }
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
