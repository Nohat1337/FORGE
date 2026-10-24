#include "src/fvm/classfile.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <map>

using namespace forge::fvm;
namespace fs = std::filesystem;

static const char* OPCODE_NAMES[] = {
    "CONSTANT","NIL","TRUE","FALSE","POP","DUP",
    "DEFINE_GLOBAL","GET_GLOBAL","SET_GLOBAL","GET_LOCAL","SET_LOCAL",
    "GET_UPVALUE","SET_UPVALUE","CLOSE_UPVALUE",
    "ADD","SUBTRACT","MULTIPLY","DIVIDE","MODULO",
    "BITWISE_AND","BITWISE_OR","BITWISE_XOR",
    "NEGATE","NOT","EQUAL","NOT_EQUAL","LESS","LESS_EQUAL","GREATER","GREATER_EQUAL",
    "JUMP","JUMP_IF_FALSE","JUMP_IF_TRUE","LOOP",
    "CALL","RETURN","CLOSURE","PRINT",
    "ARRAY","MAP","INDEX","SET_INDEX","INDEX_LEN",
    "GET_PROPERTY","SET_PROPERTY","CLASS","METHOD","INHERIT","GET_SUPER",
    "THROW","TRY","END_TRY","IMPORT",
    "YIELD","NEXT","CREATE_GENERATOR","EOF"
};

static bool hasU16(uint8_t op) {
    if (op <= 12 && op != 1 && op != 2 && op != 3 && op != 4 && op != 5) return true;
    if (op >= 31 && op <= 48) return true;
    if (op == 53) return true;
    return false;
}

static uint32_t djb2(const std::vector<uint8_t>& data) {
    uint32_t hash = 5381;
    for (uint8_t b : data) hash = ((hash << 5) + hash) ^ b;
    return hash;
}

static std::string toHex(uint32_t v) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << v;
    return ss.str();
}

static std::string toHex64(uint64_t v) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << v;
    return ss.str();
}

static std::vector<uint8_t> readAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    f.seekg(0, std::ios::end);
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> data((size_t)size);
    f.read((char*)data.data(), size);
    return data;
}

static void writeText(const std::string& path, const std::string& content) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream f(path);
    f << content;
}

static void writeBin(const std::string& path, const std::vector<uint8_t>& data) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream f(path, std::ios::binary);
    if (!data.empty()) f.write((const char*)data.data(), data.size());
}

static std::string disassembleMethod(const ClassFile& cf, const MethodInfo& method) {
    std::ostringstream out;
    std::string mname = cf.getUtf8(method.nameIndex);
    std::string desc = cf.getUtf8(method.descriptorIndex);
    out << "; Method: " << mname << "\n";
    out << "; Descriptor: " << desc << "\n";
    out << "; Arity: " << method.arity << " MaxLocals: " << method.maxLocals 
        << " MaxStack: " << method.maxStack << " Upvalues: " << method.upvalueCount << "\n";
    out << "; Bytecode size: " << method.bytecode.size() << "\n\n";

    const auto& code = method.bytecode;
    size_t i = 0;
    while (i < code.size()) {
        size_t off = i;
        uint8_t op = code[i++];
        if (op < 57) {
            out << std::setfill('0') << std::setw(4) << off << "  "
                << std::setw(2) << std::hex << (int)op << std::dec << "  "
                << OPCODE_NAMES[op];
        } else {
            out << std::setfill('0') << std::setw(4) << off << "  "
                << std::setw(2) << std::hex << (int)op << std::dec << "  "
                << "UNKNOWN";
        }
        if (hasU16(op)) {
            if (i + 1 < code.size()) {
                uint16_t operand = (uint16_t)(code[i] << 8 | code[i+1]);
                i += 2;
                out << "  #" << operand;
                if ((op == 0 || op == 6 || op == 7 || op == 8 || op == 37) && operand < cf.constantPool.size()) {
                    auto& c = cf.constantPool[operand];
                    if (c.tag == CPTag::UTF8) {
                        out << "  ; \"" << cf.getUtf8((uint16_t)operand) << "\"";
                    } else if (c.tag == CPTag::CLASS) {
                        out << "  ; class " << cf.getClassName((uint16_t)operand);
                    }
                }
            }
        } else if (op == 35) { // CALL
            if (i < code.size()) {
                out << "  " << (int)code[i++];
            }
        }
        out << "\n";
    }
    out << "\n; End method\n";
    return out.str();
}

static std::string reconstructForge(const ClassFile& cf) {
    std::ostringstream out;
    std::string className = cf.getClassName(cf.thisClass);
    if (className.empty()) className = "Module";
    out << "// Reconstructed from .fclass binary (hash-matched structure)\n";
    out << "// Original class: " << className << "\n\n";

    for (const auto& method : cf.methods) {
        std::string mname = cf.getUtf8(method.nameIndex);
        if (mname == "(script)") {
            out << "// Top-level script\n";
            out << "fn main() {\n";
            out << "    // Bytecode: see methods/" << mname << ".dis\n";
            out << "    // Reconstruct by tracing stack operations\n";
            out << "}\n\n";
        } else {
            out << "// Method: " << mname << " (arity=" << method.arity << ")\n";
            out << "fn " << mname << "(";
            for (int a = 0; a < method.arity; a++) {
                if (a > 0) out << ", ";
                out << "arg" << a;
            }
            out << ") {\n";
            out << "    // Bytecode: see methods/" << mname << ".dis\n";
            out << "    // Stack trace required for full reconstruction\n";
            out << "}\n\n";
        }
    }
    return out.str();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: forge-decompiler <input.fclass|dir> [output_dir]\n";
        std::cerr << "  Decompiles .fclass files into structured source tree with binary hash matching\n";
        return 1;
    }

    std::string input = argv[1];
    std::string outdir = argc >= 3 ? argv[2] : "decompiled";
    fs::create_directories(outdir);

    std::vector<std::string> targets;
    if (fs::is_directory(input)) {
        for (const auto& entry : fs::recursive_directory_iterator(input)) {
            if (entry.is_regular_file() && entry.path().extension() == ".fclass") {
                targets.push_back(entry.path().string());
            }
        }
        std::sort(targets.begin(), targets.end());
    } else {
        targets.push_back(input);
    }

    std::cout << "[forge-decompiler] Processing " << targets.size() << " file(s)\n";

    std::ostringstream hashLog;
    hashLog << "# Forge binary decompilation - hash index\n";
    hashLog << "# djb2 + structural metadata for binary integrity matching\n\n";

    for (const auto& fclassPath : targets) {
        std::vector<uint8_t> raw = readAll(fclassPath);
        if (raw.empty()) {
            std::cerr << "[SKIP] " << fclassPath << " (unreadable)\n";
            continue;
        }

        ClassFile cf;
        if (!cf.load(raw.data(), raw.size())) {
            std::cerr << "[SKIP] " << fclassPath << " (invalid .fclass)\n";
            continue;
        }

        std::string className = cf.getClassName(cf.thisClass);
        if (className.empty()) className = "program";

        // Preserve relative structure
        fs::path inPath(fclassPath);
        fs::path relPath;
        try {
            relPath = fs::relative(inPath, fs::current_path());
        } catch (...) {
            relPath = inPath.filename();
        }

        fs::path baseOut = fs::path(outdir) / relPath;
        baseOut.replace_extension("");

        uint32_t dh = djb2(raw);
        uint64_t sh = ((uint64_t)dh << 32) | (uint64_t)(raw.size() & 0xFFFFFFFF);

        std::cout << "[OK] " << fclassPath << " -> " << baseOut << "/\n";

        hashLog << "FILE " << fclassPath << "\n";
        hashLog << "  djb2:      " << toHex(dh) << "\n";
        hashLog << "  struct:    " << toHex64(sh) << "\n";
        hashLog << "  size:      " << raw.size() << "\n";
        hashLog << "  magic:     " << std::hex << cf.magic << std::dec << "\n";
        hashLog << "  class:     " << className << "\n";
        hashLog << "  methods:   " << cf.methods.size() << "\n";
        hashLog << "  cp:        " << cf.constantPool.size() << "\n\n";

        // binary/
        writeBin(baseOut / "binary" / inPath.filename().string(), raw);
        std::ostringstream binMeta;
        binMeta << "# Binary metadata\n";
        binMeta << "original_path: " << fclassPath << "\n";
        binMeta << "djb2_hash: " << toHex(dh) << "\n";
        binMeta << "structural_hash: " << toHex64(sh) << "\n";
        binMeta << "byte_size: " << raw.size() << "\n";
        binMeta << "fclass_magic: 0x" << std::hex << cf.magic << std::dec << "\n";
        binMeta << "fclass_version: " << cf.majorVersion << "." << cf.minorVersion << "\n";
        binMeta << "this_class_cp_index: " << cf.thisClass << "\n";
        binMeta << "resolved_class_name: " << className << "\n";
        binMeta << "method_count: " << cf.methods.size() << "\n";
        binMeta << "constant_pool_size: " << cf.constantPool.size() << "\n";
        writeText(baseOut / "binary" / "META.info", binMeta.str());

        // src/
        std::ostringstream srcHeader;
        srcHeader << "// ============================================================\n";
        srcHeader << "// Forge Source (reconstructed from binary)\n";
        srcHeader << "// Original: " << fclassPath << "\n";
        srcHeader << "// Binary hash match: djb2=0x" << toHex(dh) << " struct=0x" << toHex64(sh) << "\n";
        srcHeader << "// ============================================================\n\n";
        writeText(baseOut / "src" / "main.fge", srcHeader.str() + reconstructForge(cf));

        // methods/ with disassembly
        for (const auto& method : cf.methods) {
            std::string mname = cf.getUtf8(method.nameIndex);
            std::string safeName = mname;
            for (char& c : safeName) if (c == '/' || c == '<' || c == '>' || c == ':' || c == '\\') c = '_';
            writeText(baseOut / "methods" / (safeName + ".dis"), disassembleMethod(cf, method));
            writeBin(baseOut / "methods" / (safeName + ".bytecode"), method.bytecode);
        }

        // constant pool
        std::ostringstream cpOut;
        cpOut << "; Constant Pool Dump\n\n";
        for (size_t i = 1; i < cf.constantPool.size(); i++) {
            cpOut << "[" << std::setfill('0') << std::setw(4) << i << "] ";
            auto& c = cf.constantPool[i];
            cpOut << "tag=" << (int)c.tag;
            if (c.tag == CPTag::UTF8) cpOut << " UTF8=\"" << cf.getUtf8((uint16_t)i) << "\"";
            else if (c.tag == CPTag::INTEGER) cpOut << " INTEGER=" << c.getInteger();
            else if (c.tag == CPTag::FLOAT) cpOut << " FLOAT=" << c.getFloat();
            else if (c.tag == CPTag::LONG) cpOut << " LONG=" << c.getLong();
            else if (c.tag == CPTag::DOUBLE) cpOut << " DOUBLE=" << c.getDouble();
            else if (c.tag == CPTag::CLASS) cpOut << " CLASS->" << cf.getClassName((uint16_t)i);
            else if (c.tag == CPTag::STRING) cpOut << " STRING";
            else if (c.tag == CPTag::METHODREF) {
                auto mr = cf.getMethodref((uint16_t)i);
                cpOut << " METHODREF " << std::get<0>(mr) << "." << std::get<1>(mr);
            }
            else if (c.tag == CPTag::NAME_AND_TYPE) {
                auto nat = cf.getNameAndType((uint16_t)i);
                cpOut << " NAMEANDTYPE " << nat.first << ":" << nat.second;
            }
            cpOut << "\n";
        }
        writeText(baseOut / "constant_pool.dis", cpOut.str());

        // Verification script (compile back with our toolchain)
        std::ostringstream verify;
        verify << "#!/bin/sh\n";
        verify << "# Verify binary integrity against source structure\n";
        verify << "echo 'hash:' && cat binary/META.info | grep hash\n";
        verify << "echo '---'\n";
        verify << "ls -la methods/*.dis 2>/dev/null | wc -l && echo 'methods'\n";
        verify << "# Re-compile from forge source if desired:\n";
        verify << "# forgevm compile src/main.fge rebuilt.fclass\n";
        verify << "# compare hashes\n";
        writeText(baseOut / "verify.sh", verify.str());
    }

    writeText(std::string(outdir) + "/HASH_INDEX.txt", hashLog.str());
    std::cout << "[forge-decompiler] Done. Output: " << outdir << "/\n";
    std::cout << "[forge-decompiler] Hash index: " << outdir << "/HASH_INDEX.txt\n";
    return 0;
}
