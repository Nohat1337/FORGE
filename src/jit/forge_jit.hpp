#pragma once

#ifdef FORGE_HAS_LLVM

#include "../chunk.hpp"
#include <memory>
#include <unordered_map>
#include <string>

namespace llvm {
    class Module;
    class ExecutionEngine;
    class Function;
    class Value;
    class IRBuilder;
    class LLVMContext;
    class Type;
    class BasicBlock;
}

namespace forge::jit {

class ForgeJIT {
public:
    ForgeJIT();
    ~ForgeJIT();

    using NativeFunction = long long(*)(long long* stack, int* stackTop);
    NativeFunction compile(const Chunk& chunk, const std::string& name = "main");

    static bool isAvailable();

    int getCompiledCount() const { return compiledCount_; }
    int getFailedCount() const { return failedCount_; }

private:
    struct JittedFunction {
        void* nativeCode;
        std::string name;
    };

    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::Module> module_;
    std::unique_ptr<llvm::ExecutionEngine> engine_;

    std::unordered_map<std::string, JittedFunction> cache_;
    int compiledCount_ = 0;
    int failedCount_ = 0;

    bool initializeLLVM();
    llvm::Function* compileChunk(const Chunk& chunk, const std::string& name);
    llvm::Value* emitOp(llvm::IRBuilder<>& builder, uint8_t op, const Chunk& chunk,
                        llvm::Value* stack, llvm::Value* stackTop, int& offset);
};

} // namespace forge::jit

#else

namespace forge::jit {
class ForgeJIT {
public:
    static bool isAvailable() { return false; }
    int getCompiledCount() const { return 0; }
    int getFailedCount() const { return 0; }
};
} // namespace forge::jit

#endif
