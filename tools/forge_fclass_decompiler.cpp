#include "../src/fvm/classfile.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>

using namespace forge::fvm;
namespace fs = std::filesystem;

static const char* OPCODE_NAMES[] = {
    "CONSTANT","NIL","TRUE","FALSE","POP","DUP","DEFINE_GLOBAL","GET_GLOBAL","SET_GLOBAL","GET_LOCAL","SET_LOCAL","GET_UPVALUE","SET_UPVALUE","CLOSE_UPVALUE","ADD","SUBTRACT","MULTIPLY","DIVIDE","MODULO","BITWISE_AND","BITWISE_OR","BITWISE_XOR","NEGATE","NOT","EQUAL","NOT_EQUAL","LESS","LESS_EQUAL","GREATER","GREATER_EQUAL","JUMP","JUMP_IF_FALSE","JUMP_IF_TRUE","LOOP","CALL","RETURN","CLOSURE","PRINT","ARRAY","MAP","INDEX","SET_INDEX","INDEX_LEN","GET_PROPERTY","SET_PROPERTY","CLASS","METHOD","INHERIT","GET_SUPER","THROW","TRY","END_TRY","IMPORT","YIELD","NEXT","CREATE_GENERATOR","EOF"
};

static bool hasU16Operand(uint8_t op) {
    return op == 0 || (op >= 6 && op <= 12) || op == 31 || op == 32 || op == 33 || op == 34 || op == 37 || (op >= 39 && op <= 40) || op == 45 || op == 46 || op == 47 || op == 48 || op == 53;
}

static uint32_t djb2Hash(const std::vector<uint8_t>& data) {
    uint32_t hash = 5381;
    for (uint8_t b : data) hash = ((hash << 5) + hash) ^ b;
    return hash;
}

static uint64_t structuralHash(const std::vector<uint8_t>& data) {
    uint32_t h = djb2Hash(data);
    return ((uint64_t)h << 32) | (uint64_t)(data.size() & 0xFFFFFFFF);
}

static std::vector<uint8_t> readFileBytes(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return {};
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> data((size_t)size);
    if (size > 0) f.read((char*)data.data(), size);
    return data;
}

static void writeFileText(const std::string& path, const std::string& content) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream f(path);
    if (f.is_open()) f << content;
}

static void writeFileBinary(const std::string& path, const std::vector<uint8_t>& data) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream f(path, std::ios::binary);
    if (f.is_open() && !data.empty()) f.write((const char*)data.data(), data.size());
}

static std::string disassembleMethod(const ClassFile& cf, const MethodInfo& method) {
    std::ostringstream out;
    std::string mname = cf.getUtf8(method.nameIndex);
    std::string desc = cf.getUtf8(method.descriptorIndex);
    out << "; Forge FVM Disassembly\n; Method: " << mname << "\n; Descriptor: " << (desc.empty()?"(none)":desc) << "\n";
    out << "; Arity: " << method.arity << " MaxLocals: " << method.maxLocals << " MaxStack: " << method.maxStack << " Upvalues: " << method.upvalueCount << "\n";
    out << "; BytecodeLength: " << method.bytecode.size() << " ExceptionHandlers: " << method.exceptionTable.size() << "\n\n";
    const auto& code = method.bytecode;
    size_t i = 0;
    while (i < code.size()) {
        size_t offset = i;
        uint8_t op = code[i++];
        out << std::setfill('0') << std::setw(4) << std::hex << offset << "  " << std::setw(2) << (int)op << std::dec << "  ";
        out << (op < 57 ? OPCODE_NAMES[op] : std::string("OP_") + std::to_string(op));
        if (hasU16Operand(op) && i + 1 <= code.size()) {
            uint16_t operand = (uint16_t)(code[i] << 8) | code[i + 1];
            i += 2;
            out << "  " << operand;
            if ((op == 0 || (op >= 6 && op <= 8) || op == 37) && operand < cf.constantPool.size()) {
                if (cf.constantPool[operand].tag == CPTag::UTF8) out << "  ; \"" << cf.getUtf8((uint16_t)operand) << "\"";
                else if (cf.constantPool[operand].tag == CPTag::CLASS) out << "  ; class " << cf.getClassName((uint16_t)operand);
            }
        } else if (op == 35 && i < code.size()) {
            out << "  " << (int)code[i++];
        }
        out << "\n";
    }
    out << "\n; Disassembly complete\n";
    return out.str();
}

static std::string reconstructSource(const ClassFile& cf) {
    std::ostringstream out;
    std::string className = cf.getClassName(cf.thisClass);
    if (className.empty()) className = "program";
    out << "// Reconstructed from .fclass binary with hash matching\n// Class: " << className << " Methods: " << cf.methods.size() << " CP: " << cf.constantPool.size() << "\n\n";
    for (const auto& method : cf.methods) {
        std::string mname = cf.getUtf8(method.nameIndex);
        if (mname == "(script)") {
            out << "// top-level\nfn main() {\n    // see methods/(script).dis\n}\n\n";
        } else {
            out << "// method " << mname << " arity=" << method.arity << "\nfn " << mname << "(";
            for (uint16_t a = 0; a < method.arity; a++) { if (a>0) out << ", "; out << "arg" << a; }
            out << ") {\n    // see methods/" << mname << ".dis\n}\n\n";
        }
    }
    return out.str();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: forge-fclass-decompiler <input.fclass|dir> [outdir]\n";
        return 1;
    }
    std::string input = argv[1];
    std::string outputDir = argc >= 3 ? argv[2] : "decompiled";
    fs::create_directories(outputDir);
    std::vector<std::string> files;
    if (fs::is_directory(input)) {
        for (const auto& e : fs::recursive_directory_iterator(input)) if (e.is_regular_file() && e.path().extension() == ".fclass") files.push_back(e.path().string());
        std::sort(files.begin(), files.end());
        if (files.empty()) {
            std::cerr << "[forge-fclass-decompiler] no .fclass files found under " << input << "\n";
            return 1;
        }
    } else if (!fs::exists(input)) {
        std::cerr << "[forge-fclass-decompiler] input not found: " << input << "\n";
        return 1;
    } else if (fs::path(input).extension() != ".fclass") {
        std::cerr << "[forge-fclass-decompiler] not a .fclass file: " << input << "\n";
        std::cerr << "(point me at a .fclass file or a directory containing them)\n";
        return 1;
    } else files.push_back(input);

    std::cout << "[forge-fclass-decompiler] " << files.size() << " file(s)\n";
    std::ostringstream hashIndex;
    hashIndex << "# Forge .fclass Binary Hash Index (C++)\n\n";
    std::vector<std::string> components;  // collected for the generated Forgefile
    for (const auto& fpath : files) {
        std::vector<uint8_t> raw = readFileBytes(fpath);
        if (raw.empty()) continue;
        ClassFile cf;
        if (!cf.load(raw.data(), raw.size())) continue;
        std::string className = cf.getClassName(cf.thisClass);
        if (className.empty()) className = "program";
        fs::path inPath(fpath);
        fs::path relPath = fs::relative(inPath, fs::current_path());
        fs::path baseOut = fs::path(outputDir) / relPath;
        baseOut.replace_extension("");
        uint32_t djb = djb2Hash(raw);
        uint64_t sh = structuralHash(raw);
        std::cout << "[DECOMPILE] " << fpath << "\n";
        hashIndex << "FILE " << fpath << " djb2=0x" << std::hex << djb << " struct=0x" << sh << " size=" << std::dec << raw.size() << "\n";
        writeFileBinary((baseOut / "binary" / inPath.filename()).string(), raw);
        std::ostringstream meta;
        meta << "original_path=" << fpath << "\ndjb2_hash=0x" << std::hex << djb << "\nstructural_hash=0x" << sh << "\nbyte_size=" << std::dec << raw.size() << "\nfclass_magic=0x" << std::hex << cf.magic << "\nclass_name=" << className << "\nmethod_count=" << cf.methods.size() << "\nconstant_pool_size=" << cf.constantPool.size() << "\n";
        writeFileText((baseOut / "binary" / "META.info").string(), meta.str());
        writeFileText((baseOut / "src" / (className + ".fge")).string(), reconstructSource(cf));
        // register the reconstructed source as a component of the project file;
        // the path must work when the Forgefile (sitting in outputDir) builds
        fs::path compRel = fs::relative(baseOut / "src" / (className + ".fge"), fs::path(outputDir));
        components.push_back(compRel.string());
        for (const auto& method : cf.methods) {
            std::string mname = cf.getUtf8(method.nameIndex);
            std::string safe = mname;
            for (char& c : safe) if (c=='/'||c=='<'||c=='>'||c==':'||c=='\\'||c=='*'||c=='?'||c=='"'||c=='|') c='_';
            writeFileText((baseOut / "methods" / (safe + ".dis")).string(), disassembleMethod(cf, method));
            writeFileBinary((baseOut / "methods" / (safe + ".bytecode")).string(), method.bytecode);
        }
        std::ostringstream cpOut; cpOut << "; Constant Pool\n\n";
        for (size_t i = 1; i < cf.constantPool.size(); i++) {
            cpOut << "[" << i << "] tag=" << (int)cf.constantPool[i].tag;
            if (cf.constantPool[i].tag == CPTag::UTF8) cpOut << " UTF8=\"" << cf.getUtf8((uint16_t)i) << "\"";
            else if (cf.constantPool[i].tag == CPTag::CLASS) cpOut << " CLASS=" << cf.getClassName((uint16_t)i);
            else if (cf.constantPool[i].tag == CPTag::METHODREF) { auto mr = cf.getMethodref((uint16_t)i); cpOut << " METHODREF " << std::get<0>(mr) << "." << std::get<1>(mr); }
            else if (cf.constantPool[i].tag == CPTag::NAME_AND_TYPE) { auto nat = cf.getNameAndType((uint16_t)i); cpOut << " NAT " << nat.first << ":" << nat.second; }
            cpOut << "\n";
        }
        writeFileText((baseOut / "constant_pool.dis").string(), cpOut.str());
    }
    writeFileText(std::string(outputDir) + "/HASH_INDEX.txt", hashIndex.str());

    // Vice versa: emit the single hand-written Forgefile prefilled with
    // every reconstructed component so the project recompiles with
    // `forgevm build`.
    if (!components.empty()) {
        std::ostringstream forgefile;
        std::string outName = fs::path(outputDir).filename().string();
        if (outName.empty() || outName == ".") outName = "decompiled";
        forgefile << "# " << outName << " - FORGE single-file project (generated by forge-fclass-decompiler)\n";
        forgefile << "# ONE file for ONE binary: hand-tune components below.\n\n";
        forgefile << "project(" << outName << ")\nversion(1.0.0)\narch(x64)\ntype(executable)\n";
        forgefile << "output(" << outName << ")\nbuilddir(build-releases)\nrelease(true)\n\n";
        forgefile << "# --- components (hand-written, in link order) ---\n";
        for (auto& c : components) forgefile << "component(" << c << ")\n";
        forgefile << "\n# --- forge modules ---\nmodule(io)\nmodule(json)\n";
        writeFileText(fs::path(outputDir) / "Forgefile", forgefile.str());
        std::cout << "[FORGEFILE] " << outputDir << "/Forgefile (" << components.size() << " components)\n";
    }

    std::cout << "[DONE] " << outputDir << "/HASH_INDEX.txt\n";
    return 0;
}
