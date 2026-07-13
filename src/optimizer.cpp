#include "optimizer.hpp"
#include <algorithm>
#include <cstring>

static const uint8_t OP_NOP = 0xFF;

static bool isPushOp(uint8_t op) {
    return op == OP_CONSTANT || op == OP_NIL || op == OP_TRUE || op == OP_FALSE;
}

static int instructionSize(const Chunk& chunk, size_t offset) {
    uint8_t op = chunk.code[offset];
    switch (op) {
        case OP_CONSTANT:
        case OP_DEFINE_GLOBAL:
        case OP_SET_GLOBAL:
        case OP_GET_GLOBAL:
        case OP_GET_PROPERTY:
        case OP_SET_PROPERTY:
        case OP_ARRAY:
        case OP_MAP:
        case OP_METHOD:
        case OP_IMPORT:
        case OP_CLASS:
            return 3;
        case OP_GET_LOCAL:
        case OP_SET_LOCAL:
        case OP_JUMP:
        case OP_JUMP_IF_FALSE:
        case OP_JUMP_IF_TRUE:
        case OP_LOOP:
        case OP_CLOSE_UPVALUE:
            return 3;
        case OP_CALL:
            return 2;
        case OP_GET_UPVALUE:
        case OP_SET_UPVALUE:
            return 2;
        case OP_NIL:
        case OP_TRUE:
        case OP_FALSE:
        case OP_POP:
        case OP_DUP:
        case OP_ADD:
        case OP_SUBTRACT:
        case OP_MULTIPLY:
        case OP_DIVIDE:
        case OP_MODULO:
        case OP_NEGATE:
        case OP_NOT:
        case OP_EQUAL:
        case OP_NOT_EQUAL:
        case OP_LESS:
        case OP_LESS_EQUAL:
        case OP_GREATER:
        case OP_GREATER_EQUAL:
        case OP_RETURN:
        case OP_PRINT:
        case OP_INDEX:
        case OP_SET_INDEX:
        case OP_INDEX_LEN:
        case OP_INHERIT:
        case OP_THROW:
        case OP_END_TRY:
        case OP_YIELD:
        case OP_NEXT:
        case OP_EOF:
            return 1;
        case OP_CLOSURE: {
            int size = 3;
            uint16_t fnIdx = (uint16_t)((chunk.code[offset + 1] << 8) | chunk.code[offset + 2]);
            for (int i = 0; i < (int)chunk.constants[fnIdx].asFunction()->upvalueCount; i++) {
                size += 2;
            }
            return size;
        }
        default:
            return 1;
    }
}

void Optimizer::optimize(Chunk& chunk) {
    passRemoveRedundantPushPop(chunk);
    passSimplifyBooleanJumps(chunk);
    compactNops(chunk);
}

void Optimizer::passRemoveRedundantPushPop(Chunk& chunk) {
    bool changed = true;
    while (changed) {
        changed = false;
        size_t i = 0;
        while (i + 1 < chunk.code.size()) {
            uint8_t op = chunk.code[i];
            if (op == OP_NOP) { i++; continue; }
            int sz = instructionSize(chunk, i);
            if (sz == 1 && isPushOp(op) && i + 1 < chunk.code.size()) {
                uint8_t next = chunk.code[i + 1];
                if (next == OP_POP) {
                    chunk.code[i] = OP_NOP;
                    chunk.code[i + 1] = OP_NOP;
                    changed = true;
                }
            }
            i += sz;
        }
        if (changed) compactNops(chunk);
    }
}

void Optimizer::passSimplifyBooleanJumps(Chunk& chunk) {
    size_t i = 0;
    while (i + 1 < chunk.code.size()) {
        if (chunk.code[i] == OP_NOP) { i++; continue; }
        int sz = instructionSize(chunk, i);
        if (sz == 1 && isPushOp(chunk.code[i]) && i + sz < chunk.code.size()) {
            uint8_t next = chunk.code[i + sz];
            if (next == OP_NOP) { i += sz; continue; }
            int nextSz = instructionSize(chunk, i + sz);
            if (nextSz == 3) {
                uint8_t nextOp = next;
                if ((chunk.code[i] == OP_TRUE && nextOp == OP_JUMP_IF_FALSE) ||
                    (chunk.code[i] == OP_FALSE && nextOp == OP_JUMP_IF_TRUE)) {
                    chunk.code[i] = OP_JUMP;
                    chunk.code[i + 1] = chunk.code[i + sz + 1];
                    chunk.code[i + 2] = chunk.code[i + sz + 2];
                    chunk.code[i + sz] = OP_NOP;
                    chunk.code[i + sz + 1] = OP_NOP;
                    chunk.code[i + sz + 2] = OP_NOP;
                    i += 3;
                    continue;
                }
            }
        }
        i += sz;
    }
    compactNops(chunk);
}

void Optimizer::compactNops(Chunk& chunk) {
    std::vector<int> oldToNew(chunk.code.size(), -1);
    size_t newPos = 0;
    for (size_t i = 0; i < chunk.code.size(); ) {
        if (chunk.code[i] == OP_NOP) {
            oldToNew[i] = (int)newPos;
            i++;
        } else {
            int sz = instructionSize(chunk, i);
            for (int j = 0; j < sz; j++) {
                if (i + j < oldToNew.size()) oldToNew[i + j] = (int)newPos + j;
            }
            newPos += sz;
            i += sz;
        }
    }

    std::vector<uint8_t> newCode;
    std::vector<int> newLines;
    newCode.reserve(newPos);
    newLines.reserve(newPos);

    for (size_t i = 0; i < chunk.code.size(); ) {
        if (chunk.code[i] == OP_NOP) {
            i++;
        } else {
            int sz = instructionSize(chunk, i);
            for (int j = 0; j < sz; j++) {
                newCode.push_back(chunk.code[i + j]);
                if (i + j < chunk.lines.size())
                    newLines.push_back(chunk.lines[i + j]);
                else
                    newLines.push_back(0);
            }
            i += sz;
        }
    }

    chunk.code = std::move(newCode);
    chunk.lines = std::move(newLines);

    for (size_t i = 0; i < chunk.code.size(); ) {
        uint8_t op = chunk.code[i];
        int sz = instructionSize(chunk, i);
        if (op == OP_JUMP || op == OP_JUMP_IF_FALSE || op == OP_JUMP_IF_TRUE) {
            uint16_t oldOffset = (uint16_t)((chunk.code[i + 1] << 8) | chunk.code[i + 2]);
            size_t oldTarget = i + 3 + oldOffset;
            if (oldTarget < oldToNew.size() && oldToNew[oldTarget] >= 0) {
                uint16_t newOffset = (uint16_t)(oldToNew[oldTarget] - (i + 3));
                chunk.code[i + 1] = (newOffset >> 8) & 0xFF;
                chunk.code[i + 2] = newOffset & 0xFF;
            }
        } else if (op == OP_LOOP) {
            uint16_t oldOffset = (uint16_t)((chunk.code[i + 1] << 8) | chunk.code[i + 2]);
            size_t oldTarget = i + 3 - oldOffset;
            if (oldTarget < oldToNew.size() && oldToNew[oldTarget] >= 0) {
                uint16_t newOffset = (uint16_t)((i + 3) - oldToNew[oldTarget]);
                chunk.code[i + 1] = (newOffset >> 8) & 0xFF;
                chunk.code[i + 2] = newOffset & 0xFF;
            }
        } else if (op == OP_TRY) {
            uint16_t oldOffset = (uint16_t)((chunk.code[i + 1] << 8) | chunk.code[i + 2]);
            size_t oldTarget = i + 3 + oldOffset;
            if (oldTarget < oldToNew.size() && oldToNew[oldTarget] >= 0) {
                uint16_t newOffset = (uint16_t)(oldToNew[oldTarget] - (i + 3));
                chunk.code[i + 1] = (newOffset >> 8) & 0xFF;
                chunk.code[i + 2] = newOffset & 0xFF;
            }
        }
        i += sz;
    }
}
