#pragma once

#include "chunk.hpp"

class Optimizer {
public:
    static void optimize(Chunk& chunk);
private:
    static void passRemoveRedundantPushPop(Chunk& chunk);
    static void passSimplifyBooleanJumps(Chunk& chunk);
    static void compactNops(Chunk& chunk);
};
