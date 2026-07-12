#pragma once

#include "chunk.hpp"
#include "value.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <cstdint>

class VM {
public:
    VM();
    bool interpret(std::shared_ptr<ObjFunction> function);

    void defineNative(const std::string& name, std::function<Value(const std::vector<Value>&)> fn, int arity = -1);

    void setMaxHeap(size_t max) { maxHeapAllocations_ = max; }
    size_t getHeapAllocations() const { return heapAllocations_; }

    static constexpr int STACK_MAX = 256 * 256;
    static constexpr int MAX_CALL_FRAMES = 256;

    struct CallFrame {
        ObjClosure* closure = nullptr;
        uint8_t* ip = nullptr;
        Value* slots = nullptr;
    };

    std::vector<Value> stack;
    std::vector<CallFrame> frames;
    std::unordered_map<std::string, Value> globals;
    int frameCount = 0;
    std::shared_ptr<ObjUpvalue> openUpvalues;

private:
    struct TryFrame {
        int stackSize;
        int frameCount;
        int handlerOffset;
    };
    std::vector<TryFrame> tryFrames;
    size_t heapAllocations_ = 0;
    size_t maxHeapAllocations_ = 10000000;

    void reset();
    void push(const Value& val);
    Value pop();
    Value peek(int distance = 0);

    void runtimeError(const std::string& msg);

    bool call(ObjClosure* closure, int argCount);
    bool callValue(const Value& callee, int argCount);
    Value invoke(ObjClass* klass, const std::string& name, int argCount);

    void defineBuiltins();
    bool loadLibrary(const std::string& name, const std::string& funcName, int arity);

    std::shared_ptr<ObjUpvalue> captureUpvalue(Value* slot);
    void closeUpvalues(Value* slot);
    bool handleError(const std::string& msg);
};
