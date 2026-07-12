#include "chunk.hpp"

int Chunk::addConstant(const Value& value) {
    constants.push_back(value);
    return (int)constants.size() - 1;
}

void Chunk::writeByte(uint8_t byte, int line) {
    code.push_back(byte);
    lines.push_back(line);
}

void Chunk::writeOp(uint8_t op, int line) {
    writeByte(op, line);
}

void Chunk::writeOpOperand16(uint8_t op, uint16_t operand, int line) {
    writeByte(op, line);
    writeByte((operand >> 8) & 0xFF, line);
    writeByte(operand & 0xFF, line);
}

int Chunk::writeJump(uint8_t op, int line) {
    writeByte(op, line);
    writeByte(0xFF, line);
    writeByte(0xFF, line);
    return (int)code.size() - 2;
}

void Chunk::writeLoop(int loopStart, int line) {
    writeByte(OP_LOOP, line);
    int offset = (int)code.size() - loopStart + 2;
    writeByte((offset >> 8) & 0xFF, line);
    writeByte(offset & 0xFF, line);
}

void Chunk::patchJump(int offset) {
    int jump = (int)code.size() - offset - 2;
    code[offset] = (jump >> 8) & 0xFF;
    code[offset + 1] = jump & 0xFF;
}

int Chunk::lineAt(int offset) const {
    if (offset < (int)lines.size()) return lines[offset];
    return -1;
}
