#pragma once

#include "../chunk.hpp"
#include "../value.hpp"
#include "classfile.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <queue>
#include <random>

namespace forge::fvm {

// Process-wide trusted-mode flag. Set by ForgeVM::setTrustedMode so that
// subsystems without a VM reference (FAW agent tools, etc.) can check it
// lazily at call time.
extern bool g_globalTrustedMode;

// Forward declarations
struct FValue;
class ForgeVM;
struct GCString;
struct GCFunction;
struct GCNative;
struct GCUpvalue;
struct GCClosure;
struct GCArray;
struct GCMap;
struct GCClass;
struct GCInstance;
struct GCBoundMethod;
struct FVMTryFrame;
struct FVMThread;

// ============================================================
// GC Object types
// ============================================================

enum class GCObjType {
    STRING, FUNCTION, NATIVE, CLOSURE, UPVALUE, ARRAY, MAP,
    CLASS, INSTANCE, BOUND_METHOD, GENERATOR
};

struct GCObject {
    GCObjType type;
    bool marked;
    GCObject* next;

    GCObject() : type(GCObjType::STRING), marked(false), next(nullptr) {}
    GCObject(GCObjType t) : type(t), marked(false), next(nullptr) {}
    virtual ~GCObject() = default;
    template<typename T> T* as() { return static_cast<T*>(this); }
    template<typename T> const T* as() const { return static_cast<const T*>(this); }
};

// ============================================================
// FValue - FVM's value type (uses GCObject* for heap objects)
// ============================================================

enum class FValueType : uint8_t {
    VAL_NIL, VAL_BOOL, VAL_INT, VAL_FLOAT, VAL_OBJ
};

struct FValue {
    FValueType type = FValueType::VAL_NIL;
    union {
        bool boolean;
        long long integer;
        double floating;
        GCObject* obj;
    } as = {};

    FValue() : type(FValueType::VAL_NIL) {}
    static FValue nil() { return FValue(); }
    static FValue boolean(bool v) { FValue f; f.type = FValueType::VAL_BOOL; f.as.boolean = v; return f; }
    static FValue integer(long long v) { FValue f; f.type = FValueType::VAL_INT; f.as.integer = v; return f; }
    static FValue floating(double v) { FValue f; f.type = FValueType::VAL_FLOAT; f.as.floating = v; return f; }
    static FValue obj(GCObject* o) { FValue f; f.type = FValueType::VAL_OBJ; f.as.obj = o; return f; }

    bool isTruthy() const;
    bool equals(const FValue& other) const;
    std::string toString() const;
    bool isNumber() const { return type == FValueType::VAL_INT || type == FValueType::VAL_FLOAT; }
    double asNumber() const;
    long long asInteger() const;
    bool isString() const;
    bool isFunction() const;
    bool isClosure() const;
    bool isNative() const;
    bool isArray() const;
    bool isMap() const;
    bool isClass() const;
    bool isInstance() const;
    bool isBoundMethod() const;
    bool isObj() const { return type == FValueType::VAL_OBJ; }

    GCString* asString() const;
    GCFunction* asFunction() const;
    GCClosure* asClosure() const;
    GCNative* asNative() const;
    GCArray* asArray() const;
    GCMap* asMap() const;
    GCClass* asClass() const;
    GCInstance* asInstance() const;
    GCBoundMethod* asBoundMethod() const;

    bool operator==(const FValue& other) const { return equals(other); }
};

// Conversion between old Value and FValue
FValue valueToFValue(const Value& v);

// ============================================================
// GC managed objects (use FValue)
// ============================================================

struct GCString : GCObject {
    std::string value;
    uint32_t hash = 0;
    GCString(const std::string& s) : GCObject(GCObjType::STRING), value(s) {
        hash = 2166136261u;
        for (char c : s) { hash ^= (uint8_t)c; hash *= 16777619u; }
    }
};

struct GCFunction : GCObject {
    Chunk* chunk = nullptr;
    std::unique_ptr<Chunk> ownedChunk;    // Pre-converted FValue view of chunk->constants. Populated once at
    // function creation so OP_CONSTANT / OP_CLOSURE never re-convert
    // (previously every OP_CONSTANT with a string allocated a fresh
    // GCString, and every OP_CLOSURE deep-copied the whole Chunk).
    std::vector<FValue> fconstants;
    int arity = 0;
    int upvalueCount = 0;
    int localCount = 0;
    bool isGenerator = false;
    std::string name;
    GCFunction() : GCObject(GCObjType::FUNCTION) {}
};

struct GCNative : GCObject {
    using NativeFn = std::function<FValue(const std::vector<FValue>&)>;
    NativeFn function;
    std::string name;
    int arity = 0;
    GCNative(NativeFn fn, const std::string& n)
        : GCObject(GCObjType::NATIVE), function(std::move(fn)), name(n) {}
    GCNative(NativeFn fn, const std::string& n, int a)
        : GCObject(GCObjType::NATIVE), function(std::move(fn)), name(n), arity(a) {}
};

struct GCUpvalue : GCObject {
    FValue* location = nullptr;
    FValue closed;
    GCUpvalue* next = nullptr;
    GCUpvalue(FValue* slot) : GCObject(GCObjType::UPVALUE), location(slot) {}
};

struct GCClosure : GCObject {
    GCFunction* function = nullptr;
    std::vector<GCUpvalue*> upvalues;
    GCClosure(GCFunction* fn) : GCObject(GCObjType::CLOSURE), function(fn) {
        upvalues.resize(fn->upvalueCount, nullptr);
    }
};

struct GCArray : GCObject {
    std::vector<FValue> elements;
    GCArray() : GCObject(GCObjType::ARRAY) {}
};

struct GCMap : GCObject {
    std::unordered_map<std::string, FValue> entries;
    GCMap() : GCObject(GCObjType::MAP) {}
};

struct GCClass : GCObject {
    std::string name;
    GCClass* superclass = nullptr;
    std::unordered_map<std::string, struct GCMethod> methods;
    GCClass(const std::string& n) : GCObject(GCObjType::CLASS), name(n) {}
};

struct GCInstance : GCObject {
    GCClass* klass = nullptr;
    std::unordered_map<std::string, FValue> fields;
    GCInstance(GCClass* k) : GCObject(GCObjType::INSTANCE), klass(k) {}
};

struct GCBoundMethod : GCObject {
    FValue receiver;
    GCClosure* method = nullptr;
    GCBoundMethod(const FValue& r, GCClosure* m)
        : GCObject(GCObjType::BOUND_METHOD), receiver(r), method(m) {}
};

// Paused generator execution state. Owns a dedicated FVMThread whose stack,
// frames and ip survive across next() calls.
struct GCGenerator : GCObject {
    GCClosure* closure = nullptr;
    std::unique_ptr<FVMThread> thread;
    std::vector<FValue> args;   // call arguments (kept for first resume)
    FValue lastValue;           // most recent yielded value
    bool started = false;
    bool done = false;
    GCGenerator(GCClosure* c) : GCObject(GCObjType::GENERATOR), closure(c) {}
    ~GCGenerator() override; // out-of-line: FVMThread is incomplete here
};

struct GCMethod {
    GCClosure* closure = nullptr;
    bool isStatic = false;
};

// ============================================================
// FVM Thread & Frames
// ============================================================

class FVMGC;  // Forward declaration

struct FVMFrame {
    GCClosure* closure = nullptr;
    uint8_t* ip = nullptr;
    FValue* slots = nullptr;
    int slotCount = 0;
};

// try/catch frame: records the thread state to restore when an exception
// unwinds into the matching handler.
struct FVMTryFrame {
    int stackSize;
    int frameCount;
    int handlerOffset;
};

struct FVMThread {
    enum class State { NEW, RUNNABLE, BLOCKED, WAITING, TERMINATED };

    std::atomic<State> state{State::NEW};
    std::string name;
    size_t threadId = 0;
    std::thread nativeThread;

    static constexpr int MAX_FRAMES = 256;
    std::vector<FVMFrame> frames;
    int frameCount = 0;

    static constexpr int STACK_SIZE = 1024 * 1024;
    std::vector<FValue> stack;
    int stackTop = 0;

    // Per-thread state (previously shared ForgeVM members — interleaved
    // execution of generator threads corrupted these).
    std::vector<FVMTryFrame> tryFrames;
    GCUpvalue* openUpvalues = nullptr;

    FVMGC* gc = nullptr;  // Reference to GC for object allocation

    FVMFrame& currentFrame() { return frames[frameCount - 1]; }
    bool hasFrames() const { return frameCount > 0; }

    void pushFrame(GCClosure* closure, FValue* slots, int argCount);
    void popFrame(FValue& result);
    bool callValue(const FValue& callee, int argCount);

    void push(const FValue& v) {
        if (stackTop >= (int)stack.size()) {
            throw std::runtime_error("Stack overflow: VM value stack exhausted");
        }
        stack[stackTop++] = v;
    }
    FValue pop() { return stack[--stackTop]; }
    FValue& peek(int dist = 0) { return stack[stackTop - 1 - dist]; }
};

// ============================================================
// FVM Garbage Collector
// ============================================================

class FVMGC {
public:
    static constexpr size_t DEFAULT_HEAP_SIZE = 64 * 1024 * 1024;

    FVMGC(size_t heapSize = DEFAULT_HEAP_SIZE);

    GCObject* allocateRaw(size_t size);
    void trackObject(GCObject* obj);
    void collect();

    template<typename T, typename... Args>
    T* newObj(Args&&... args) {
        T* obj = reinterpret_cast<T*>(allocateRaw(sizeof(T)));
        new (obj) T(std::forward<Args>(args)...);
        trackObject(obj);
        return obj;
    }
    void markObject(GCObject* obj);
    void markValue(const FValue& val);
    void registerThread(FVMThread* thread);
    void unregisterThread(FVMThread* thread);
    size_t getAllocated() const { return allocated_; }
    size_t getHeapSize() const { return heapSize_; }

private:
    void markRoots();
    void traceReferences();
    void sweep();
    void freeObject(GCObject* obj);

    GCObject* objectList_ = nullptr;
    size_t allocated_ = 0;
    size_t heapSize_;
    size_t nextGC_;
    std::vector<FVMThread*> threads_;
    std::mutex threadsMutex_;
};

// ============================================================
// FVM Main Class
// ============================================================

class ForgeVM {
public:
    ForgeVM();
    ~ForgeVM();

    bool interpret(std::shared_ptr<GCFunction> function);
    bool interpretSource(const std::string& source, const std::string& filename = "<script>");

    // Call a top-level Forge function from C++ (embedding / export API).
    bool callGlobal(const std::string& name, std::vector<FValue> args, FValue& result);
    bool interpretClassFile(const std::string& path);
    bool interpretClassFileData(const ClassFile& cf, const std::string& filename = "");

    void setArgs(int argc, char** argv) { savedArgc_ = argc; savedArgv_ = argv; }

    // Trusted mode unlocks dangerous capabilities (FFI native calls, raw
    // memory access, unrestricted os.execute). Must be enabled explicitly
    // by the embedder/CLI — NEVER default-on for untrusted scripts.
    void setTrustedMode(bool t) { trustedMode_ = t; g_globalTrustedMode = t; }
    bool isTrusted() const { return trustedMode_; }

    void defineNative(const std::string& name, GCNative::NativeFn fn, int arity = -1);
    void defineModule(const std::string& name, GCMap* module);

    FVMGC& gc() { return gc_; }
    static constexpr int MAX_CALL_FRAMES = FVMThread::MAX_FRAMES;

    FVMThread mainThread_;
    FVMThread* currentThread_ = &mainThread_;
    std::unordered_map<std::string, FValue> globals_;
    std::unordered_map<std::string, FValue> modules_;
    void setMaxHeap(size_t max) { maxHeap_ = max; }

private:
    FVMGC gc_;
    size_t maxHeap_ = 10000000;
    bool trustedMode_ = false;
    // Generator currently executing (for OP_YIELD bookkeeping); nested
    // next() calls save/restore this.
    GCGenerator* activeGenerator_ = nullptr;

    void reset();
    bool runLoop();
    // Runs the dispatch loop on an arbitrary thread (generators). Sets
    // *yieldedOut when the loop stopped at OP_YIELD rather than completing.
    bool runLoopOn(FVMThread& thread, bool* yieldedOut = nullptr);
    FValue runGenerator(GCGenerator* gen);
    void runtimeError(const std::string& msg);
    bool handleError(FVMThread& thread, const std::string& msg);
    bool call(GCClosure* closure, int argCount);
    bool callValue(const FValue& callee, int argCount);
    void defineBuiltins();
    void defineModules();
    void defineComputeModule();

    void closeUpvalues(FVMThread& thread, FValue* last);
    GCUpvalue* captureUpvalue(FVMThread& thread, FValue* slot);

    std::vector<std::unique_ptr<FVMThread>> threads_;
    std::mutex threadsMutex_;
    size_t nextThreadId_ = 1;
    int savedArgc_ = 0;
    char** savedArgv_ = nullptr;
};

} // namespace forge::fvm
