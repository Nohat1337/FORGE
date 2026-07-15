#include "forge_wasm.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace forge::wasm {

static void writeSection(std::vector<uint8_t>& bytes, uint8_t type, const std::vector<uint8_t>& data) {
    bytes.push_back(type);
    // LEB128 encoding for section size
    uint32_t size = data.size();
    do {
        uint8_t b = size & 0x7F;
        size >>= 7;
        if (size) b |= 0x80;
        bytes.push_back(b);
    } while (size);
    bytes.insert(bytes.end(), data.begin(), data.end());
}

static void writeU32(std::vector<uint8_t>& bytes, uint32_t val) {
    do {
        uint8_t b = val & 0x7F;
        val >>= 7;
        if (val) b |= 0x80;
        bytes.push_back(b);
    } while (val);
}

static void writeU64(std::vector<uint8_t>& bytes, uint64_t val) {
    do {
        uint8_t b = val & 0x7F;
        val >>= 7;
        if (val) b |= 0x80;
        bytes.push_back(b);
    } while (val);
}

static void writeString(std::vector<uint8_t>& bytes, const std::string& s) {
    writeU32(bytes, s.size());
    bytes.insert(bytes.end(), s.begin(), s.end());
}

static void writeI32Const(std::vector<uint8_t>& code, int32_t val) {
    code.push_back(WasmOp::OP_I32_CONST);
    writeU32(code, (uint32_t)val);
}

static void writeI64Const(std::vector<uint8_t>& code, int64_t val) {
    code.push_back(WasmOp::OP_I64_CONST);
    writeU64(code, (uint64_t)val);
}

static void writeF64Const(std::vector<uint8_t>& code, double val) {
    code.push_back(WasmOp::OP_F64_CONST);
    uint64_t bits;
    std::memcpy(&bits, &val, 8);
    writeU64(code, bits);
}

WasmModule ForgeWASM::compileToWasm(const std::string& source, const std::string& name) {
    WasmModule mod;
    mod.name = name;
    std::vector<uint8_t>& bytes = mod.bytes;

    // WASM magic: \0asm
    bytes = {0x00, 0x61, 0x73, 0x6D};
    // Version: 1
    bytes.insert(bytes.end(), {0x01, 0x00, 0x00, 0x00});

    // ===== Type Section =====
    // 1 function type: (i64* stack, i32* stackTop) -> i64
    std::vector<uint8_t> typeSec;
    writeU32(typeSec, 1);  // 1 type
    // func type
    typeSec.push_back(0x60);  // func
    writeU32(typeSec, 2);  // 2 params
    typeSec.push_back(0x70);  // i64 (stack ptr)
    typeSec.push_back(0x70);  // i64 (stackTop ptr - use i64 for pointer)
    writeU32(typeSec, 1);  // 1 result
    typeSec.push_back(0x7E);  // i64
    writeSection(bytes, SECTION_TYPE, typeSec);

    // ===== Import Section =====
    // Import console.log for printing
    std::vector<uint8_t> importSec;
    writeU32(importSec, 1);  // 1 import
    writeString(importSec, "env");  // module
    writeString(importSec, "print");  // name
    importSec.push_back(0x00);  // func
    writeU32(importSec, 0);  // type index 0 (func (i64) -> ())
    writeSection(bytes, SECTION_IMPORT, importSec);

    // ===== Function Section =====
    // 1 function (index 0 = imported print, index 1 = main)
    std::vector<uint8_t> funcSec;
    writeU32(funcSec, 1);  // 1 function
    writeU32(funcSec, 1);  // type index 1 (our custom func type)
    writeSection(bytes, SECTION_FUNCTION, funcSec);

    // ===== Export Section =====
    // Export "main" function and "memory"
    std::vector<uint8_t> exportSec;
    writeU32(exportSec, 2);  // 2 exports
    // Export main
    writeString(exportSec, "main");
    exportSec.push_back(0x00);  // func
    writeU32(exportSec, 1);  // func index 1 (0 is import)
    // Export memory
    writeString(exportSec, "memory");
    exportSec.push_back(0x02);  // memory
    writeU32(exportSec, 0);  // memory index 0
    writeSection(bytes, SECTION_EXPORT, exportSec);

    // ===== Memory Section =====
    std::vector<uint8_t> memSec;
    writeU32(memSec, 1);  // 1 memory
    memSec.push_back(0x00);  // no max
    writeU32(memSec, 1);  // initial 1 page (64KB)
    writeSection(bytes, SECTION_MEMORY, memSec);

    // ===== Global Section (for global variables) =====
    std::vector<uint8_t> globalSec;
    writeU32(globalSec, 0);  // no globals initially
    writeSection(bytes, SECTION_GLOBAL, globalSec);

    // ===== Code Section =====
    // Function body for main
    std::vector<uint8_t> codeSec;
    writeU32(codeSec, 1);  // 1 function body

    std::vector<uint8_t> body;
    
    // Function body header: local count = 0
    writeU32(body, 0);

    // Embed the source as a string constant in data section
    // For now, emit a simple interpreter loop that calls the import
    // This is a minimal WASM that calls the imported print
    
    // Stack-based interpreter for simple arithmetic
    // We'll implement a basic stack machine that can run simple Forge bytecode
    
    // Prologue: set up local variables for stack
    // Stack is in linear memory, stackTop is a local
    
    // Simple implementation: call print with a constant
    // (local.get 0) -- stack pointer
    // (local.get 1) -- stackTop pointer
    // i32.const 42
    // call print
    
    body.push_back(WasmOp::OP_I32_CONST);
    writeU32(body, 42);
    body.push_back(WasmOp::OP_CALL);
    writeU32(body, 0);  // call imported print
    body.push_back(WasmOp::OP_DROP);
    
    // Return 0
    body.push_back(WasmOp::OP_I64_CONST);
    writeU64(body, 0);
    body.push_back(WasmOp::OP_RETURN);
    
    body.push_back(WasmOp::OP_END);

    writeU32(codeSec, body.size());
    codeSec.insert(codeSec.end(), body.begin(), body.end());
    writeSection(bytes, SECTION_CODE, codeSec);

    // ===== Data Section =====
    // Embed the source string for potential debugging
    std::vector<uint8_t> dataSec;
    writeU32(dataSec, 1);  // 1 data segment
    dataSec.push_back(0x00);  // memory index 0
    dataSec.push_back(0x41);  // i32.const (offset)
    writeU32(dataSec, 0);  // offset 0
    dataSec.push_back(0x0B);  // end
    writeString(dataSec, source);
    writeSection(bytes, SECTION_DATA, dataSec);

    return mod;
}

WasmModule ForgeWASM::classFileToWasm(const std::string& classfilePath) {
    (void)classfilePath;
    WasmModule mod;
    mod.name = "classfile";
    return mod;
}

std::string ForgeWASM::toWat(const WasmModule& mod) {
    std::ostringstream out;
    out << "(module\n";
    out << "  ;; Forge WASM module: " << mod.name << "\n";
    out << "  (import \"env\" \"print\" (func $print (param i64)))\n";
    out << "  (memory 1)\n";
    out << "  (export \"memory\" (memory 0))\n";
    out << "  (export \"main\" (func $main))\n";
    out << "  (func $main\n";
    out << "    i32.const 42\n";
    out << "    call $print\n";
    out << "    drop\n";
    out << "    i64.const 0\n";
    out << "  )\n";
    out << ")\n";
    return out.str();
}

bool ForgeWASM::validate(const WasmModule& mod) {
    if (mod.bytes.size() < 8) return false;
    return mod.bytes[0] == 0x00 && mod.bytes[1] == 0x61 && 
           mod.bytes[2] == 0x73 && mod.bytes[3] == 0x6D;
}

bool ForgeWASM::saveToFile(const WasmModule& mod, const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file.write(reinterpret_cast<const char*>(mod.bytes.data()), mod.bytes.size());
    return file.good();
}

} // namespace forge::wasm