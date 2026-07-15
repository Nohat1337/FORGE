#include "forge_jit.hpp"

#ifdef FORGE_HAS_LLVM

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include <iostream>
#include <cstring>

namespace forge::jit {

// Helper C functions called by JIT-compiled code
extern "C" {
    void forge_jit_print(long long val) {
        std::cout << val << "\n";
    }

    long long forge_jit_interpret_op(uint8_t op, long long* stack, int* stackTop,
                                      const uint8_t** ip) {
        (void)op; (void)stack; (void)stackTop; (void)ip;
        return 0;
    }

    void forge_jit_define_global_c(const char* name, long long val) {
        (void)name; (void)val;
    }

    long long forge_jit_get_global_c(const char* name) {
        (void)name;
        return 0;
    }
}

static bool llvmInitialized = false;

ForgeJIT::ForgeJIT() {
    if (!llvmInitialized) {
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();
        llvmInitialized = true;
    }
    initializeLLVM();
}

ForgeJIT::~ForgeJIT() = default;

bool ForgeJIT::isAvailable() {
    return true;
}

bool ForgeJIT::initializeLLVM() {
    context_ = std::make_unique<llvm::LLVMContext>();
    module_ = std::make_unique<llvm::Module>("forge_jit", *context_);
    module_->setTargetTriple(llvm::sys::getDefaultTargetTriple());

    auto JITBuilder = llvm::orc::LLJITBuilder();
    auto jitOrErr = JITBuilder.create();
    if (!jitOrErr) {
        std::cerr << "JIT: Failed to create LLJIT\n";
        return false;
    }
    engine_ = std::move(*jitOrErr);
    return true;
}

llvm::Function* ForgeJIT::compileChunk(const Chunk& chunk, const std::string& name) {
    auto& ctx = *context_;

    llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);
    llvm::Type* i32 = llvm::Type::getInt32Ty(ctx);
    llvm::Type* i8 = llvm::Type::getInt8Ty(ctx);
    llvm::Type* i8Ptr = llvm::Type::getInt8PtrTy(ctx);

    llvm::FunctionType* fnType = llvm::FunctionType::get(i64, {i8Ptr, i8Ptr}, false);
    llvm::Function* func = llvm::Function::Create(
        fnType, llvm::Function::ExternalLinkage, name, *module_);

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx, "entry", func);
    llvm::IRBuilder<> builder(entry);

    auto args = func->args();
    llvm::Value* stack = &*args;
    llvm::Value* stackTopPtr = &*(++args);

    std::vector<llvm::BasicBlock*> opBlocks;
    std::vector<llvm::Value*> opValues;

    for (int i = 0; i < (int)chunk.code.size(); i++) {
        opBlocks.push_back(llvm::BasicBlock::Create(ctx, "op" + std::to_string(i), func));
        opValues.push_back(nullptr);
    }
    llvm::BasicBlock* exitBlock = llvm::BasicBlock::Create(ctx, "exit", func);

    builder.CreateBr(opBlocks.empty() ? exitBlock : opBlocks[0]);

    for (int i = 0; i < (int)chunk.code.size(); i++) {
        builder.SetInsertPoint(opBlocks[i]);
        uint8_t op = chunk.code[i];

        switch (op) {
            case 0x00: { // OP_CONSTANT
                if (i + 2 < (int)chunk.code.size()) {
                    uint16_t idx = ((uint16_t)chunk.code[i+1] << 8) | chunk.code[i+2];
                    if (idx < chunk.constants.size()) {
                        auto& c = chunk.constants[idx];
                        long long val = 0;
                        if (c.isInteger()) val = c.asInteger();
                        else if (c.isFloating()) val = (long long)c.asFloating();
                        else if (c.isTruthy()) val = 1;
                        opValues[i] = llvm::ConstantInt::get(i64, (uint64_t)val);
                    }
                    i += 2;
                }
                break;
            }
            case 0x01: { // OP_NIL
                opValues[i] = llvm::ConstantInt::get(i64, 0);
                break;
            }
            case 0x02: { // OP_TRUE
                opValues[i] = llvm::ConstantInt::get(i64, 1);
                break;
            }
            case 0x03: { // OP_FALSE
                opValues[i] = llvm::ConstantInt::get(i64, 0);
                break;
            }
            case 0x0A: { // OP_ADD
                llvm::Value* a = i > 0 ? opValues[i-1] : llvm::ConstantInt::get(i64, 0);
                llvm::Value* b = (i > 1 && opValues[i-2]) ? opValues[i-2] : llvm::ConstantInt::get(i64, 0);
                if (a && b) opValues[i] = builder.CreateAdd(b, a);
                else opValues[i] = llvm::ConstantInt::get(i64, 0);
                break;
            }
            case 0x0B: { // OP_SUBTRACT
                llvm::Value* a = i > 0 ? opValues[i-1] : llvm::ConstantInt::get(i64, 0);
                llvm::Value* b = (i > 1 && opValues[i-2]) ? opValues[i-2] : llvm::ConstantInt::get(i64, 0);
                if (a && b) opValues[i] = builder.CreateSub(b, a);
                else opValues[i] = llvm::ConstantInt::get(i64, 0);
                break;
            }
            case 0x0C: { // OP_MULTIPLY
                llvm::Value* a = i > 0 ? opValues[i-1] : llvm::ConstantInt::get(i64, 0);
                llvm::Value* b = (i > 1 && opValues[i-2]) ? opValues[i-2] : llvm::ConstantInt::get(i64, 0);
                if (a && b) opValues[i] = builder.CreateMul(b, a);
                else opValues[i] = llvm::ConstantInt::get(i64, 0);
                break;
            }
            case 0x0D: { // OP_DIVIDE
                llvm::Value* a = i > 0 ? opValues[i-1] : llvm::ConstantInt::get(i64, 1);
                llvm::Value* b = (i > 1 && opValues[i-2]) ? opValues[i-2] : llvm::ConstantInt::get(i64, 0);
                if (a && b) opValues[i] = builder.CreateSDiv(b, a);
                else opValues[i] = llvm::ConstantInt::get(i64, 0);
                break;
            }
            case 0x14: { // OP_PRINT
                llvm::Value* val = i > 0 ? opValues[i-1] : llvm::ConstantInt::get(i64, 0);
                if (val) {
                    llvm::FunctionCallee printFn = module_->getOrInsertFunction(
                        "forge_jit_print", llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {i64}, false));
                    builder.CreateCall(printFn, {val});
                }
                opValues[i] = llvm::ConstantInt::get(i64, 0);
                break;
            }
            default: {
                opValues[i] = llvm::ConstantInt::get(i64, 0);
                break;
            }
        }

        if (i + 1 < (int)chunk.code.size()) {
            builder.CreateBr(opBlocks[i + 1]);
        } else {
            builder.CreateBr(exitBlock);
        }
    }

    builder.SetInsertPoint(exitBlock);
    llvm::Value* result = llvm::ConstantInt::get(i64, 0);
    if (!opValues.empty() && opValues.back()) {
        result = opValues.back();
    }
    builder.CreateRet(result);

    std::string errMsg;
    llvm::raw_string_ostream errStream(errMsg);
    if (llvm::verifyFunction(*func, &errStream)) {
        std::cerr << "JIT: Verification failed: " << errMsg << "\n";
        func->eraseFromParent();
        failedCount_++;
        return nullptr;
    }

    compiledCount_++;
    return func;
}

ForgeJIT::NativeFunction ForgeJIT::compile(const Chunk& chunk, const std::string& name) {
    if (cache_.count(name)) {
        return reinterpret_cast<NativeFunction>(cache_[name].nativeCode);
    }

    llvm::Function* func = compileChunk(chunk, name);
    if (!func) return nullptr;

    auto& execCtx = engine_->getExecutionSession();
    auto tsm = llvm::orc::ThreadSafeModule(
        std::make_unique<llvm::Module>(std::move(*module_)),
        std::move(context_));

    module_ = std::make_unique<llvm::Module>("forge_jit", llvm::LLVMContext());
    context_ = std::make_unique<llvm::LLVMContext>();

    auto addrOrErr = engine_->lookup(name);
    if (!addrOrErr) {
        return nullptr;
    }

    auto addr = addrOrErr->getAddress();
    auto fnPtr = reinterpret_cast<NativeFunction>(addr.getValue());

    cache_[name] = {reinterpret_cast<void*>(fnPtr), name};
    return fnPtr;
}

} // namespace forge::jit

#else

namespace forge::jit {
// Empty implementation when LLVM is not available
} // namespace forge::jit

#endif
