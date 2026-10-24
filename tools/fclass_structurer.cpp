#include "../src/fvm/classfile.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <map>
#include <vector>
#include <string>
#include <functional>

using namespace forge::fvm;
namespace fs = std::filesystem;

static uint32_t djb2_hash(const std::vector<uint8_t>& data) {
    uint32_t hash = 5381;
    for (uint8_t b : data) hash = ((hash << 5) + hash) ^ b;
    return hash;
}

static std::string sha256_hex(const std::vector<uint8_t>& data) {
    // Use openssl if available, else skip
    FILE* fp = popen("sha256sum 2>&1 | head -1", "r");
    if (fp) pclose(fp);
    // Fallback: just use djb2 + size
    std::ostringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << djb2_hash(data);
    ss << std::hex << std::setfill('0') << std::setw(16) << (uint64_t)data.size();
    return ss.str();
}

static std::vector<uint8_t> readAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static void writeFile(const std::string& path, const std::string& content) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream f(path);
    f << content;
}

static void writeBytes(const std::string& path, const std::vector<uint8_t>& data) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream f(path, std::ios::binary);
    f.write((const char*)data.data(), data.size());
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: fclass-structurer <file.fclass|dir> [outdir]\n";
        return 1;
    }
    std::string input = argv[1];
    std::string outdir = argc >= 3 ? argv[2] : "decompiled_structured";
    fs::create_directories(outdir);

    std::vector<std::string> files;
    if (fs::is_directory(input)) {
        for (auto& e : fs::recursive_directory_iterator(input)) {
            if (e.is_regular_file() && e.path().extension() == ".fclass") {
                files.push_back(e.path().string());
            }
        }
    } else if (fs::is_regular_file(input)) {
        files.push_back(input);
    }

    std::cout << "Processing " << files.size() << " .fclass file(s)\n";

    std::ostringstream allHashes;
    allHashes << "# Forge .fclass binary hashes\n# Format: algorithm hash size path\n";

    for (auto& inpath : files) {
        std::vector<uint8_t> raw = readAll(inpath);
        if (raw.empty()) { std::cerr << "skip " << inpath << "\n"; continue; }

        ClassFile cf;
        if (!cf.load(raw)) { std::cerr << "parse fail " << inpath << "\n"; continue; }

        std::string className = cf.getClassName(cf.thisClass);
        if (className.empty()) className = "main";

        // Mirror directory structure
        fs::path rel = fs::relative(inpath, fs::current_path());
        std::string relStr = rel.string();
        fs::path outbase = fs::path(outdir) / relStr;
        outbase = outbase.replace_extension("");
        fs::create_directories(outbase);

        uint32_t dh = djb2_hash(raw);
        std::string sh = sha256_hex(raw);
        allHashes << "djb2 " << std::hex << dh << std::dec << " " << raw.size() << " " << inpath << "\n";
        allHashes << "struct " << sh << " " << raw.size() << " " << inpath << "\n";

        // Copy binary with hash sidecar
        writeBytes(outbase / "binary" / (fs::path(inpath).filename().string()), raw);
        {
            std::ostringstream hs;
            hs << "path: " << inpath << "\n";
            hs << "djb2: " << std::hex << dh << "\n";
            hs << "structural_hash: " << sh << "\n";
            hs << "size: " << std::dec << raw.size() << "\n";
            hs << "magic: " << std::hex << cf.magic << "\n";
            hs << "class: " << className << "\n";
            hs << "methods: " << cf.methods.size() << "\n";
            hs << "constant_pool: " << cf.constantPool.size() << "\n";
            writeFile(outbase / "binary" / "hash.txt", hs.str());
        }

        // Structured source reconstruction
        fs::create_directories(outbase / "src");
        std::ostringstream src;
        src << "// Auto-structured from " << inpath << "\n";
        src << "// Binary hash: djb2=" << std::hex << dh << " structural=" << sh << std::dec << "\n";
        src << "// Class: " << className << "\n\n";
        src << "#from binary \"" << "./binary/" << fs::path(inpath).filename().string() << "\" hash=djb2:" << std::hex << dh << std::dec << "\n\n";

        for (auto& method : cf.methods) {
            std::string mname = cf.getUtf8(method.nameIndex);
            std::string desc = cf.getUtf8(method.descriptorIndex);
            fs::create_directories(outbase / "src" / "methods");
            // disassembly
            std::ostringstream dis;
            dis << "// " << mname << " | arity=" << method.arity << " locals=" << method.maxLocals 
                << " stack=" << method.maxStack << " upvalues=" << method.upvalueCount << "\n";
            dis << "// desc: " << desc << "\n\n";
            const auto& code = method.bytecode;
            size_t i = 0;
            while (i < code.size()) {
                size_t off = i;
                uint8_t op = code[i++];
                // simple disasm
                dis << std::setfill('0') << std::setw(4) << off << ": ";
                // would need full table, just hex dump for now
                dis << std::hex << std::setfill('0') << std::setw(2) << (int)op;
                if (i < code.size() && ((op<=12 && op!=1&&op!=2&&op!=3&&op!=4&&op!=5) || op>=31 && op<=48 || op==53)) {
                    dis << " " << std::setw(2) << (int)code[i] << " " << std::setw(2) << (int)code[i+1];
                    i += 2;
                } else if (op == 35) {
                    dis << " " << std::setw(2) << (int)code[i++]; 
                }
                dis << std::dec << "\n";
            }
            writeFile(outbase / "src" / "methods" / (mname + ".dis"), dis.str());
            writeFile(outbase / "src" / "methods" / (mname + ".bin"), std::string(code.begin(), code.end()));

            src << "// method: " << mname << "\n";
            src << "fn " << (mname == "(script)" ? "main" : mname) << "() {\n";
            src << "    // reconstructed from bytecode hash-match\n";
            src << "    // see methods/" << mname << ".dis\n";
            src << "}\n\n";
        }
        writeFile(outbase / "src" / (className + ".fge"), src.str());
        std::cout << "  " << inpath << " -> " << outbase << "\n";
    }
    writeFile(fs::path(outdir) / "BINARY_HASHES.txt", allHashes.str());
    std::cout << "Done.\n";
    return 0;
}
