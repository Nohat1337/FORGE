#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace forge::wasm {

struct WasmModule {
    std::vector<uint8_t> bytes;
    std::string name;
};

class ForgeWASM {
public:
    // Compile Forge source to WASM bytecode
    static WasmModule compileToWasm(const std::string& source, const std::string& name = "main");
    
    // Compile a .fclass to WASM bytecode  
    static WasmModule classFileToWasm(const std::string& classfilePath);
    
    // Generate WASM text format (WAT) for debugging
    static std::string toWat(const WasmModule& mod);
    
    // Validate WASM module
    static bool validate(const WasmModule& mod);
    
    // Save WASM to file
    static bool saveToFile(const WasmModule& mod, const std::string& path);
    
    // WASM section types
    enum SectionType : uint8_t {
        SECTION_CUSTOM = 0,
        SECTION_TYPE = 1,
        SECTION_IMPORT = 2,
        SECTION_FUNCTION = 3,
        SECTION_TABLE = 4,
        SECTION_MEMORY = 5,
        SECTION_GLOBAL = 6,
        SECTION_EXPORT = 7,
        SECTION_START = 8,
        SECTION_ELEMENT = 9,
        SECTION_CODE = 10,
        SECTION_DATA = 11
    };
    
    // WASM opcodes (subset)
    enum WasmOp : uint8_t {
        OP_LOCAL_GET = 0x20,
        OP_LOCAL_SET = 0x21,
        OP_I32_CONST = 0x41,
        OP_I64_CONST = 0x42,
        OP_F64_CONST = 0x44,
        OP_I32_ADD = 0x6A,
        OP_I32_SUB = 0x6B,
        OP_I32_MUL = 0x6C,
        OP_I32_DIV_S = 0x6D,
        OP_I64_ADD = 0x7C,
        OP_I64_SUB = 0x7D,
        OP_I64_MUL = 0x7E,
        OP_F64_ADD = 0xA0,
        OP_F64_SUB = 0xA1,
        OP_F64_MUL = 0xA2,
        OP_F64_DIV = 0xA3,
        OP_CALL = 0x10,
        OP_RETURN = 0x0F,
        OP_DROP = 0x1A,
        OP_END = 0x0B,
        OP_BLOCK = 0x02,
        OP_LOOP = 0x03,
        OP_IF = 0x04,
        OP_ELSE = 0x05,
        OP_BR = 0x0C,
        OP_BR_IF = 0x0D,
        OP_GLOBAL_GET = 0x23,
        OP_GLOBAL_SET = 0x24,
        OP_I32_EQZ = 0x45,
        OP_I32_EQ = 0x46,
        OP_I32_NE = 0x47,
        OP_I32_LT_S = 0x48,
        OP_I32_GT_S = 0x4A,
        OP_MEMORY_SIZE = 0x3F,
        OP_MEMORY_GROW = 0x40,
        OP_NOP = 0x01,
        OP_UNREACHABLE = 0x00,
        OP_SELECT = 0x1B,
        OP_LOCAL_TEE = 0x22,
    };
    
private:
    static void writeSection(std::vector<uint8_t>& bytes, uint8_t type, const std::vector<uint8_t>& data);
    static void writeU32(std::vector<uint8_t>& bytes, uint32_t val);
    static void writeU64(std::vector<uint8_t>& bytes, uint64_t val);
    static void writeString(std::vector<uint8_t>& bytes, const std::string& s);
};

} // namespace forge::wasm
