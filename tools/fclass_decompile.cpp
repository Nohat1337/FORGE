#include "../src/fvm/classfile.hpp"
#include "../src/chunk.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>
#include <vector>
#include <string>

using namespace forge::fvm;

// Map opcodes back to names (same as Chunk/VM)
static const char* opName(uint8_t op) {
    switch ((Op)op) {
        case OP_CONSTANT: return "CONSTANT";
        case OP_NIL: return "NIL";
        case OP_TRUE: return "TRUE";
        case OP_FALSE: return "FALSE";
        case OP_POP: return "POP";
        case OP_DUP: return "DUP";
        case OP_DEFINE_GLOBAL: return "DEFINE_GLOBAL";
        case OP_GET_GLOBAL: return "GET_GLOBAL";
        case OP_SET_GLOBAL: return "SET_GLOBAL";
        case OP_GET_LOCAL: return "GET_LOCAL";
        case OP_SET_LOCAL: return "SET_LOCAL";
        case OP_GET_UPVALUE: return "GET_UPVALUE";
        case OP_SET_UPVALUE: return "SET_UPVALUE";
        case OP_CLOSE_UPVALUE: return "CLOSE_UPVALUE";
        case OP_ADD: return "ADD";
        case OP_SUBTRACT: return "SUBTRACT";
        case OP_MULTIPLY: return "MULTIPLY";
        case OP_DIVIDE: return "DIVIDE";
        case OP_MODULO: return "MODULO";
        case OP_BITWISE_AND: return "BITWISE_AND";
        case OP_BITWISE_OR: return "BITWISE_OR";
        case OP_BITWISE_XOR: return "BITWISE_XOR";
        case OP_NEGATE: return "NEGATE";
        case OP_NOT: return "NOT";
        case OP_EQUAL: return "EQUAL";
        case OP_NOT_EQUAL: return "NOT_EQUAL";
        case OP_LESS: return "LESS";
        case OP_LESS_EQUAL: return "LESS_EQUAL";
        case OP_GREATER: return "GREATER";
        case OP_GREATER_EQUAL: return "GREATER_EQUAL";
        case OP_JUMP: return "JUMP";
        case OP_JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OP_JUMP_IF_TRUE: return "JUMP_IF_TRUE";
        case OP_LOOP: return "LOOP";
        case OP_CALL: return "CALL";
        case OP_RETURN: return "RETURN";
        case OP_CLOSURE: return "CLOSURE";
        case OP_PRINT: return "PRINT";
        case OP_ARRAY: return "ARRAY";
        case OP_MAP: return "MAP";
        case OP_INDEX: return "INDEX";
        case OP_SET_INDEX: return "SET_INDEX";
        case OP_INDEX_LEN: return "INDEX_LEN";
        case OP_GET_PROPERTY: return "GET_PROPERTY";
        case OP_SET_PROPERTY: return "SET_PROPERTY";
        case OP_CLASS: return "CLASS";
        case OP_METHOD: return "METHOD";
        case OP_INHERIT: return "INHERIT";
        case OP_GET_SUPER: return "GET_SUPER";
        case OP_THROW: return "THROW";
        case OP_TRY: return "TRY";
        case OP_END_TRY: return "END_TRY";
        case OP_IMPORT: return "IMPORT";
        case OP_YIELD: return "YIELD";
        case OP_NEXT: return "NEXT";
        case OP_CREATE_GENERATOR: return "CREATE_GENERATOR";
        case OP_EOF: return "EOF";
        default: return "UNKNOWN";
    }
}

// Ops that take a 16-bit operand
static bool hasU16(uint8_t op) {
    switch ((Op)op) {
        case OP_CONSTANT: case OP_DEFINE_GLOBAL: case OP_GET_GLOBAL: case OP_SET_GLOBAL:
        case OP_GET_LOCAL: case OP_SET_LOCAL: case OP_GET_UPVALUE: case OP_SET_UPVALUE:
        case OP_JUMP: case OP_JUMP_IF_FALSE: case OP_JUMP_IF_TRUE: case OP_LOOP:
        case OP_CALL: case OP_CLOSURE: case OP_ARRAY: case OP_MAP: case OP_CLASS: case OP_METHOD:
        case OP_IMPORT: case OP_TRY:
            return true;
        default: return false;
    }
}

static std::vector<uint8_t> readAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return data;
}

static std::string hashBytes(const std::vector<uint8_t>& data) {
    // simple djb2 for a quick fingerprint; the requirement says "match the hashes of binary data"
    unsigned long hash = 5381;
    for (uint8_t b : data) hash = ((hash << 5) + hash) + b;
    std::ostringstream ss;
    ss << std::hex << hash;
    return ss.str();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: fclass-decompile <file.fclass> [outdir]\n";
        return 1;
    }

    std::string in = argv[1];
    std::string outdir = argc >= 3 ? argv[2] : "decompiled";
    std::vector<uint8_t> raw = readAll(in);
    if (raw.empty()) {
        std::cerr << "Cannot read " << in << "\n";
        return 1;
    }

    ClassFile cf;
    if (!cf.load(raw)) {
        std::cerr << "Failed to parse " << in << "\n";
        return 1;
    }

    std::string className = cf.getClassName(cf.thisClass);
    if (className.empty()) className = "Unknown";

    std::cout << "Magic: " << std::hex << cf.magic << std::dec << "\n";
    std::cout << "Version: " << cf.majorVersion << "." << cf.minorVersion << "\n";
    std::cout << "This class: " << className << "\n";
    std::cout << "Constant pool entries: " << cf.constantPool.size() << "\n";
    std::cout << "Methods: " << cf.methods.size() << "\n";
    std::cout << "Raw hash (djb2): " << hashBytes(raw) << "\n";

    std::string cmd = "mkdir -p " + outdir;
    system(cmd.c_str());

    // Write metadata / hash map
    {
        std::ofstream m(outdir + "/metadata.txt");
        m << "source: " << in << "\n";
        m << "raw_hash_djb2: " << hashBytes(raw) << "\n";
        m << "raw_size: " << raw.size() << "\n";
        m << "magic: " << std::hex << cf.magic << std::dec << "\n";
        m << "class: " << className << "\n";
        m << "cp_count: " << cf.constantPool.size() << "\n";
        m << "methods: " << cf.methods.size() << "\n";
    }
    {
        std::ofstream h(outdir + "/hashes.txt");
        h << "djb2 " << hashBytes(raw) << " " << in << "\n";
        h << "size " << raw.size() << " " << in << "\n";
    }

    // Constant pool dump
    {
        std::ofstream cp(outdir + "/constant_pool.txt");
        for (size_t i = 1; i < cf.constantPool.size(); i++) {
            auto& c = cf.constantPool[i];
            cp << i << ": tag=" << (int)c.tag;
            if (c.tag == CPTag::UTF8) cp << " utf8=\"" << cf.getUtf8((uint16_t)i) << "\"";
            else if (c.tag == CPTag::INTEGER) cp << " int=" << c.getInteger();
            else if (c.tag == CPTag::FLOAT) cp << " float=" << c.getFloat();
            else if (c.tag == CPTag::LONG) cp << " long=" << c.getLong();
            else if (c.tag == CPTag::DOUBLE) cp << " double=" << c.getDouble();
            else if (c.tag == CPTag::CLASS) cp << " class=" << cf.getClassName((uint16_t)i);
            else if (c.tag == CPTag::STRING) cp << " string->" << c.getStringIndex();
            else if (c.tag == CPTag::METHODREF) {
                auto mref = cf.getMethodref((uint16_t)i);
                cp << " methodref " << std::get<0>(mref) << "." << std::get<1>(mref) << ":" << std::get<2>(mref);
            }
            else if (c.tag == CPTag::NAME_AND_TYPE) {
                auto nat = cf.getNameAndType((uint16_t)i);
                cp << " nat " << nat.first << ":" << nat.second;
            }
            else if (c.tag == CPTag::FORGE_METHOD) cp << " forge_method";
            cp << "\n";
        }
    }

    // Disassemble each method into a reconstructed source-like file
    std::ostringstream src;
    src << "# Decompiled from " << in << "\n";
    src << "# class: " << className << "\n\n";

    for (auto& method : cf.methods) {
        std::string mname = cf.getUtf8(method.nameIndex);
        std::string desc = cf.getUtf8(method.descriptorIndex);
        src << "# method " << mname << " desc=" << desc << " arity=" << method.arity
            << " locals=" << method.maxLocals << " stack=" << method.maxStack << "\n";

        std::ofstream dis(outdir + "/" + mname + ".dis");
        dis << "# " << mname << "\n";
        dis << "# arity=" << method.arity << " upvalues=" << method.upvalueCount << "\n";
        dis << "# bytes: " << method.bytecode.size() << "\n\n";

        const auto& code = method.bytecode;
        size_t i = 0;
        while (i < code.size()) {
            size_t off = i;
            uint8_t op = code[i++];
            dis << std::setfill('0') << std::setw(4) << off << "  " << std::setw(8) << (int)op << "  " << opName(op);
            if (hasU16(op)) {
                if (i + 1 >= code.size()) break;
                uint16_t operand = (uint16_t)(code[i] << 8 | code[i + 1]);
                i += 2;
                dis << " " << operand;
                if (op == OP_CONSTANT || op == OP_DEFINE_GLOBAL || op == OP_GET_GLOBAL || op == OP_SET_GLOBAL || op == OP_CLOSURE || op == OP_CLASS || op == OP_METHOD) {
                    if (operand < cf.constantPool.size() && cf.constantPool[operand].tag == CPTag::UTF8) {
                        dis << "  ; \"" << cf.getUtf8((uint16_t)operand) << "\"";
                    } else if (operand < cf.constantPool.size() && cf.constantPool[operand].tag == CPTag::STRING) {
                        dis << "  ; string";
                    }
                }
            } else if (op == OP_CALL) {
                if (i >= code.size()) break;
                dis << " " << (int)code[i++];
            }
            dis << "\n";
        }

        // Heuristic reconstruction for simple cases
        src << "# --- reconstructed (heuristic) ---\n";
        src << "fn " << mname << "(...) {\n";
        src << "    # bytecode-based reconstruction requires full CFG analysis\n";
        src << "    # see " << mname << ".dis for exact instructions\n";
        src << "}\n\n";
    }

    {
        std::ofstream rs(outdir + "/" + className + ".fge.reconstructed");
        rs << src.str();
    }

    std::cout << "Decompiled to " << outdir << "/\n";
    return 0;
}
