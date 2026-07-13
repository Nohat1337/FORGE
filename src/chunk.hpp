#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <sstream>
#include "value.hpp"

enum Op : uint8_t {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_DUP,
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_CLOSE_UPVALUE,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_MODULO,
    OP_NEGATE,
    OP_NOT,
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_LESS,
    OP_LESS_EQUAL,
    OP_GREATER,
    OP_GREATER_EQUAL,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_JUMP_IF_TRUE,
    OP_LOOP,
    OP_CALL,
    OP_RETURN,
    OP_CLOSURE,
    OP_PRINT,
    OP_ARRAY,
    OP_MAP,
    OP_INDEX,
    OP_SET_INDEX,
    OP_INDEX_LEN,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_CLASS,
    OP_METHOD,
    OP_INHERIT,
    OP_GET_SUPER,
    OP_THROW,
    OP_TRY,
    OP_END_TRY,
    OP_IMPORT,
    OP_YIELD,
    OP_NEXT,
    OP_CREATE_GENERATOR,
    OP_EOF,
};

class Chunk {
public:
    std::vector<uint8_t> code;
    std::vector<Value> constants;
    std::vector<int> lines;
    std::string name;

    Chunk() = default;

    int addConstant(const Value& value);

    void writeByte(uint8_t byte, int line);
    void writeOp(uint8_t op, int line);
    void writeOpOperand16(uint8_t op, uint16_t operand, int line);
    int writeJump(uint8_t op, int line);
    void writeLoop(int loopStart, int line);
    void patchJump(int offset);

    uint8_t readByte(int offset) const { return code[offset]; }
    uint16_t readUint16(int offset) const {
        return (uint16_t)(code[offset] << 8 | code[offset + 1]);
    }

    int lineAt(int offset) const;
};
