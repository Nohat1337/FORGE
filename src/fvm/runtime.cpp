#include "runtime.hpp"
#include "sdl2_ui.hpp"
#include "../lexer.hpp"
#include "../parser.hpp"
#include "../compiler.hpp"
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>

namespace forge::fvm {

// ============================================================
// Value <-> FValue Conversion
// ============================================================

FValue valueToFValue(const Value& v) {
    switch (v.type) {
        case ValueType::VAL_NIL: return FValue::nil();
        case ValueType::VAL_BOOL: return FValue::boolean(v.as.boolean);
        case ValueType::VAL_INT: return FValue::integer(v.as.integer);
        case ValueType::VAL_FLOAT: return FValue::floating(v.as.floating);
        case ValueType::VAL_OBJ: {
            if (!v.as.obj) return FValue::nil();
            Obj* obj = v.as.obj;
            switch (obj->type) {
                case ObjType::STRING: {
                    auto* src = static_cast<ObjString*>(obj);
                    auto* gcStr = new GCString(src->value);
                    return FValue::obj(gcStr);
                }
                case ObjType::FUNCTION: {
                    auto* src = static_cast<ObjFunction*>(obj);
                    auto* gcFunc = new GCFunction();
                    gcFunc->ownedChunk = std::make_unique<Chunk>();
                    gcFunc->ownedChunk->code = src->chunk->code;
                    gcFunc->ownedChunk->constants = src->chunk->constants;
                    gcFunc->ownedChunk->lines = src->chunk->lines;
                    gcFunc->ownedChunk->name = src->chunk->name;
                    gcFunc->chunk = gcFunc->ownedChunk.get();
                    gcFunc->arity = src->arity;
                    gcFunc->upvalueCount = src->upvalueCount;
                    gcFunc->localCount = src->localCount;
                    gcFunc->name = src->name;
                    return FValue::obj(gcFunc);
                }
                case ObjType::NATIVE: {
                    auto* src = static_cast<ObjNative*>(obj);
                    auto* gcNat = new GCNative(
                        [src](const std::vector<FValue>& args) -> FValue {
                            std::vector<Value> oldArgs;
                            for (auto& a : args) {
                                switch (a.type) {
                                    case FValueType::VAL_NIL: oldArgs.push_back(Value::nil()); break;
                                    case FValueType::VAL_BOOL: oldArgs.push_back(Value::boolean(a.as.boolean)); break;
                                    case FValueType::VAL_INT: oldArgs.push_back(Value::integer(a.as.integer)); break;
                                    case FValueType::VAL_FLOAT: oldArgs.push_back(Value::floating(a.as.floating)); break;
                                    case FValueType::VAL_OBJ: {
                                        if (!a.as.obj) { oldArgs.push_back(Value::nil()); break; }
                                        switch (a.as.obj->type) {
                                            case GCObjType::STRING: {
                                                auto* s = static_cast<GCString*>(a.as.obj);
                                                oldArgs.push_back(Value::obj(std::make_shared<ObjString>(s->value)));
                                                break;
                                            }
                                            default: oldArgs.push_back(Value::nil()); break;
                                        }
                                        break;
                                    }
                                }
                            }
                            Value result = src->function(oldArgs);
                            return valueToFValue(result);
                        },
                        src->name
                    );
                    gcNat->arity = src->arity;
                    return FValue::obj(gcNat);
                }
                case ObjType::ARRAY: {
                    auto* src = static_cast<ObjArray*>(obj);
                    auto* gcArr = new GCArray();
                    for (auto& elem : src->elements)
                        gcArr->elements.push_back(valueToFValue(elem));
                    return FValue::obj(gcArr);
                }
                case ObjType::MAP: {
                    auto* src = static_cast<ObjMap*>(obj);
                    auto* gcMap = new GCMap();
                    for (auto& [k, v] : src->entries)
                        gcMap->entries[k] = valueToFValue(v);
                    return FValue::obj(gcMap);
                }
                case ObjType::CLASS: {
                    auto* src = static_cast<ObjClass*>(obj);
                    auto* gcCls = new GCClass(src->name);
                    if (src->superclass) {
                        auto superFV = valueToFValue(Value::obj(src->superclass));
                        gcCls->superclass = superFV.asClass();
                    }
                    for (auto& [n, m] : src->methods) {
                        if (m.closure) {
                            auto closureFV = valueToFValue(Value::obj(m.closure));
                            gcCls->methods[n] = {closureFV.asClosure(), m.isStatic};
                        }
                    }
                    return FValue::obj(gcCls);
                }
                case ObjType::INSTANCE: {
                    auto* src = static_cast<ObjInstance*>(obj);
                    auto* gcInst = new GCInstance(nullptr);
                    auto classFV = valueToFValue(Value::obj(src->klass));
                    gcInst->klass = classFV.asClass();
                    for (auto& [k, v] : src->fields)
                        gcInst->fields[k] = valueToFValue(v);
                    return FValue::obj(gcInst);
                }
                case ObjType::CLOSURE: {
                    auto* src = static_cast<ObjClosure*>(obj);
                    auto fnFV = valueToFValue(Value::obj(src->function));
                    auto* gcCl = new GCClosure(fnFV.asFunction());
                    for (size_t i = 0; i < src->upvalues.size() && i < gcCl->upvalues.size(); i++) {
                        if (src->upvalues[i]) {
                            auto* gcUv = new GCUpvalue(nullptr);
                            gcUv->closed = valueToFValue(src->upvalues[i]->closed);
                            gcUv->location = &gcUv->closed;
                            gcCl->upvalues[i] = gcUv;
                        }
                    }
                    return FValue::obj(gcCl);
                }
                default: return FValue::nil();
            }
        }
    }
    return FValue::nil();
}

// ============================================================
// FValue Implementation
// ============================================================

bool FValue::isTruthy() const {
    if (type == FValueType::VAL_NIL) return false;
    if (type == FValueType::VAL_BOOL) return as.boolean;
    if (type == FValueType::VAL_INT) return as.integer != 0;
    if (type == FValueType::VAL_FLOAT) return as.floating != 0.0;
    return true;
}

double FValue::asNumber() const {
    if (type == FValueType::VAL_INT) return (double)as.integer;
    if (type == FValueType::VAL_FLOAT) return as.floating;
    return 0.0;
}

long long FValue::asInteger() const {
    if (type == FValueType::VAL_INT) return as.integer;
    if (type == FValueType::VAL_FLOAT) return (long long)as.floating;
    return 0;
}

bool FValue::isString() const { return type == FValueType::VAL_OBJ && as.obj && as.obj->type == GCObjType::STRING; }
bool FValue::isFunction() const { return type == FValueType::VAL_OBJ && as.obj && as.obj->type == GCObjType::FUNCTION; }
bool FValue::isClosure() const { return type == FValueType::VAL_OBJ && as.obj && as.obj->type == GCObjType::CLOSURE; }
bool FValue::isNative() const { return type == FValueType::VAL_OBJ && as.obj && as.obj->type == GCObjType::NATIVE; }
bool FValue::isArray() const { return type == FValueType::VAL_OBJ && as.obj && as.obj->type == GCObjType::ARRAY; }
bool FValue::isMap() const { return type == FValueType::VAL_OBJ && as.obj && as.obj->type == GCObjType::MAP; }
bool FValue::isClass() const { return type == FValueType::VAL_OBJ && as.obj && as.obj->type == GCObjType::CLASS; }
bool FValue::isInstance() const { return type == FValueType::VAL_OBJ && as.obj && as.obj->type == GCObjType::INSTANCE; }
bool FValue::isBoundMethod() const { return type == FValueType::VAL_OBJ && as.obj && as.obj->type == GCObjType::BOUND_METHOD; }

GCString* FValue::asString() const { return isString() ? static_cast<GCString*>(as.obj) : nullptr; }
GCFunction* FValue::asFunction() const { return isFunction() ? static_cast<GCFunction*>(as.obj) : nullptr; }
GCClosure* FValue::asClosure() const { return isClosure() ? static_cast<GCClosure*>(as.obj) : nullptr; }
GCNative* FValue::asNative() const { return isNative() ? static_cast<GCNative*>(as.obj) : nullptr; }
GCArray* FValue::asArray() const { return isArray() ? static_cast<GCArray*>(as.obj) : nullptr; }
GCMap* FValue::asMap() const { return isMap() ? static_cast<GCMap*>(as.obj) : nullptr; }
GCClass* FValue::asClass() const { return isClass() ? static_cast<GCClass*>(as.obj) : nullptr; }
GCInstance* FValue::asInstance() const { return isInstance() ? static_cast<GCInstance*>(as.obj) : nullptr; }
GCBoundMethod* FValue::asBoundMethod() const { return isBoundMethod() ? static_cast<GCBoundMethod*>(as.obj) : nullptr; }

std::string FValue::toString() const {
    switch (type) {
        case FValueType::VAL_NIL: return "nil";
        case FValueType::VAL_BOOL: return as.boolean ? "true" : "false";
        case FValueType::VAL_INT: return std::to_string(as.integer);
        case FValueType::VAL_FLOAT: {
            double v = as.floating;
            if (v == (long long)v && v < 1e15 && v > -1e15) return std::to_string((long long)v);
            return std::to_string(v);
        }
        case FValueType::VAL_OBJ: {
            if (!as.obj) return "null";
            switch (as.obj->type) {
                case GCObjType::STRING: return asString()->value;
                case GCObjType::FUNCTION: return "<fn " + asFunction()->name + ">";
                case GCObjType::NATIVE: return "<native " + asNative()->name + ">";
                case GCObjType::CLOSURE: return "<closure " + asClosure()->function->name + ">";
                case GCObjType::ARRAY: {
                    auto* arr = asArray();
                    std::string result = "[";
                    for (size_t i = 0; i < arr->elements.size(); i++) {
                        if (i > 0) result += ", ";
                        result += arr->elements[i].toString();
                    }
                    result += "]";
                    return result;
                }
                case GCObjType::MAP: {
                    auto* m = asMap();
                    std::string result = "{";
                    bool first = true;
                    for (auto& [k, v] : m->entries) {
                        if (!first) result += ", ";
                        result += "\"" + k + "\": " + v.toString();
                        first = false;
                    }
                    result += "}";
                    return result;
                }
                case GCObjType::CLASS: return "<class " + asClass()->name + ">";
                case GCObjType::INSTANCE: return "<" + asInstance()->klass->name + " instance>";
                case GCObjType::BOUND_METHOD: return "<bound method " + asBoundMethod()->method->function->name + ">";
                default: return "<object>";
            }
        }
    }
    return "<unknown>";
}

bool FValue::equals(const FValue& other) const {
    if (type != other.type) {
        if (isNumber() && other.isNumber()) return asNumber() == other.asNumber();
        return false;
    }
    switch (type) {
        case FValueType::VAL_NIL: return true;
        case FValueType::VAL_BOOL: return as.boolean == other.as.boolean;
        case FValueType::VAL_INT: return as.integer == other.as.integer;
        case FValueType::VAL_FLOAT: return as.floating == other.as.floating;
        case FValueType::VAL_OBJ: {
            if (!as.obj && !other.as.obj) return true;
            if (!as.obj || !other.as.obj) return false;
            if (as.obj->type != other.as.obj->type) return false;
            if (as.obj->type == GCObjType::STRING) return asString()->value == other.asString()->value;
            return as.obj == other.as.obj;
        }
    }
    return false;
}

// ============================================================
// FVMThread Implementation
// ============================================================

void FVMThread::pushFrame(GCClosure* closure, FValue* slots, int argCount) {
    if (frameCount >= MAX_FRAMES) {
        throw std::runtime_error("Stack overflow: too many nested function calls");
    }
    FVMFrame frame;
    frame.closure = closure;
    frame.ip = closure->function->chunk->code.data();
    frame.slots = slots;
    frame.slotCount = closure->function->localCount;
    if (frameCount < (int)frames.size()) {
        frames[frameCount] = frame;
    } else {
        frames.push_back(frame);
    }
    frameCount++;
}

void FVMThread::popFrame(FValue& result) {
    frameCount--;
    if (frameCount > 0) {
        FVMFrame& caller = frames[frameCount - 1];
        stackTop = (int)(caller.slots - stack.data()) + 1;
        caller.slots[0] = result;
    }
}

bool FVMThread::callValue(const FValue& callee, int argCount) {
    if (callee.isClosure()) {
        GCClosure* closure = callee.asClosure();
        if (argCount != closure->function->arity) {
            return false;
        }
        FValue* calleeSlots = stack.data() + stackTop - argCount - 1;
        pushFrame(closure, calleeSlots, argCount);
        return true;
    }
    if (callee.isNative()) {
        GCNative* native = callee.asNative();
        std::vector<FValue> args(stack.data() + stackTop - argCount, stack.data() + stackTop);
        stackTop -= argCount + 1;
        FValue result = native->function(args);
        push(result);
        return true;
    }
if (callee.isClass()) {
        GCClass* klass = callee.asClass();
        auto* instance = new GCInstance(klass);
        stack[stackTop - argCount - 1] = FValue::obj(instance);
        auto it = klass->methods.find("init");
        if (it != klass->methods.end()) {
            GCClosure* initClosure = it->second.closure;
            if (argCount != initClosure->function->arity) return false;
            FValue* calleeSlots = stack.data() + stackTop - argCount - 1;
            calleeSlots[0] = FValue::obj(instance); // slot 0 = receiver (self/this)
            pushFrame(initClosure, calleeSlots, argCount);
        }
        return true;
    }
if (callee.isBoundMethod()) {
        GCBoundMethod* bm = callee.asBoundMethod();
        GCClosure* closure = bm->method;
        // Bound method has receiver bound; closure arity is user args only
        if (argCount != closure->function->arity) return false;
        FValue* calleeSlots = stack.data() + stackTop - argCount - 1;
        // For the closure: slot 0 = receiver (bound method's receiver), slots 1+ = user args
        calleeSlots[0] = bm->receiver; // slot 0 = receiver (self)
        // User args are already at [1...] if any (from original call)
        pushFrame(closure, calleeSlots, argCount + 1);
        return true;
    }
    return false;
}

// ============================================================
// FVMGC Implementation
// ============================================================

FVMGC::FVMGC(size_t heapSize) : heapSize_(heapSize), nextGC_(heapSize) {}

GCObject* FVMGC::allocateRaw(size_t size) {
    if (allocated_ >= nextGC_) {
        collect();
    }
    GCObject* obj = reinterpret_cast<GCObject*>(new uint8_t[size]);
    allocated_ += size;
    return obj;
}

void FVMGC::trackObject(GCObject* obj) {
    obj->next = objectList_;
    objectList_ = obj;
}

void FVMGC::markObject(GCObject* obj) {
    if (!obj || obj->marked) return;
    obj->marked = true;
}

void FVMGC::markValue(const FValue& val) {
    if (val.type == FValueType::VAL_OBJ && val.as.obj) {
        markObject(val.as.obj);
    }
}

void FVMGC::markRoots() {
    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        for (FVMThread* thread : threads_) {
            for (int i = 0; i < thread->stackTop; i++) {
                markValue(thread->stack[i]);
            }
            for (int i = 0; i < thread->frameCount; i++) {
                FVMFrame& frame = thread->frames[i];
                markObject(reinterpret_cast<GCObject*>(frame.closure));
            }
        }
    }
}

void FVMGC::traceReferences() {
    GCObject* current = objectList_;
    while (current) {
        if (!current->marked) { current = current->next; continue; }
        switch (current->type) {
            case GCObjType::CLOSURE: {
                auto* cl = current->as<GCClosure>();
                markObject(cl->function);
                for (auto* uv : cl->upvalues) markObject(uv);
                break;
            }
            case GCObjType::ARRAY: {
                for (auto& v : current->as<GCArray>()->elements) markValue(v);
                break;
            }
            case GCObjType::MAP: {
                for (auto& [k, v] : current->as<GCMap>()->entries) markValue(v);
                break;
            }
            case GCObjType::CLASS: {
                auto* klass = current->as<GCClass>();
                for (auto& [name, method] : klass->methods) {
                    if (method.closure) markObject(method.closure);
                }
                if (klass->superclass) markObject(klass->superclass);
                break;
            }
            case GCObjType::INSTANCE: {
                auto* inst = current->as<GCInstance>();
                markObject(inst->klass);
                for (auto& [k, v] : inst->fields) markValue(v);
                break;
            }
            case GCObjType::BOUND_METHOD: {
                auto* bm = current->as<GCBoundMethod>();
                markValue(bm->receiver);
                if (bm->method) markObject(bm->method);
                break;
            }
            case GCObjType::FUNCTION: {
                break;
            }
            case GCObjType::UPVALUE: {
                auto* uv = current->as<GCUpvalue>();
                markValue(uv->closed);
                break;
            }
            default: break;
        }
        current = current->next;
    }
}

void FVMGC::sweep() {
    GCObject** prev = &objectList_;
    GCObject* current = objectList_;
    while (current) {
        if (!current->marked) {
            GCObject* next = current->next;
            freeObject(current);
            *prev = next;
            current = next;
        } else {
            current->marked = false;
            prev = &current->next;
            current = current->next;
        }
    }
}

void FVMGC::freeObject(GCObject* obj) {
    switch (obj->type) {
        case GCObjType::STRING: obj->as<GCString>()->~GCString(); break;
        case GCObjType::FUNCTION: obj->as<GCFunction>()->~GCFunction(); break;
        case GCObjType::NATIVE: obj->as<GCNative>()->~GCNative(); break;
        case GCObjType::CLOSURE: obj->as<GCClosure>()->~GCClosure(); break;
        case GCObjType::UPVALUE: obj->as<GCUpvalue>()->~GCUpvalue(); break;
        case GCObjType::ARRAY: obj->as<GCArray>()->~GCArray(); break;
        case GCObjType::MAP: obj->as<GCMap>()->~GCMap(); break;
        case GCObjType::CLASS: obj->as<GCClass>()->~GCClass(); break;
        case GCObjType::INSTANCE: obj->as<GCInstance>()->~GCInstance(); break;
        case GCObjType::BOUND_METHOD: obj->as<GCBoundMethod>()->~GCBoundMethod(); break;
    }
    delete[] reinterpret_cast<uint8_t*>(obj);
}

void FVMGC::collect() {
    markRoots();
    traceReferences();
    sweep();
    nextGC_ = allocated_ * 2;
    if (nextGC_ < heapSize_) nextGC_ = heapSize_;
}

void FVMGC::registerThread(FVMThread* thread) {
    std::lock_guard<std::mutex> lock(threadsMutex_);
    threads_.push_back(thread);
}

void FVMGC::unregisterThread(FVMThread* thread) {
    std::lock_guard<std::mutex> lock(threadsMutex_);
    threads_.erase(std::remove(threads_.begin(), threads_.end(), thread), threads_.end());
}

// ============================================================
// ForgeVM Implementation
// ============================================================

ForgeVM::ForgeVM() : gc_(), mainThread_() {
    mainThread_.threadId = 0;
    mainThread_.name = "main";
    mainThread_.state = FVMThread::State::RUNNABLE;
    mainThread_.stack.resize(FVMThread::STACK_SIZE);
    gc_.registerThread(&mainThread_);
    currentThread_ = &mainThread_;
    defineBuiltins();
    defineModules();
}

ForgeVM::~ForgeVM() {
    gc_.unregisterThread(&mainThread_);
}

void ForgeVM::reset() {
    mainThread_.stackTop = 0;
    mainThread_.frameCount = 0;
    tryFrames_.clear();
    openUpvalues_ = nullptr;
}

void ForgeVM::runtimeError(const std::string& msg) {
    std::cerr << "Runtime Error: " << msg;
    for (int i = mainThread_.frameCount - 1; i >= 0; i--) {
        FVMFrame& f = mainThread_.frames[i];
        if (!f.closure || !f.closure->function) continue;
        auto& fn = f.closure->function;
        std::cerr << "\n  " << (i == mainThread_.frameCount - 1 ? "in " : "  called from ");
        std::cerr << fn->name;
        if (f.ip && fn->chunk) {
            int offset = (int)(f.ip - fn->chunk->code.data()) - 1;
            if (offset >= 0 && offset < (int)fn->chunk->code.size()) {
                std::cerr << " (line " << fn->chunk->lineAt(offset) << ")";
            }
        }
    }
    std::cerr << "\n";
}

bool ForgeVM::handleError(const std::string& msg) {
    if (!tryFrames_.empty()) {
        auto tf = tryFrames_.back();
        tryFrames_.pop_back();
        auto& thread = mainThread_;
        thread.frameCount = tf.frameCount;
        thread.stackTop = tf.stackSize;
        FVMFrame& cf = thread.currentFrame();
        cf.ip = cf.closure->function->chunk->code.data() + tf.handlerOffset;
        FValue err;
        err.type = FValueType::VAL_OBJ;
        auto* errStr = gc_.newObj<GCString>(msg);
        err.as.obj = errStr;
        thread.push(err);
        return true;
    }
    runtimeError(msg);
    return false;
}

GCUpvalue* ForgeVM::captureUpvalue(FValue* slot) {
    GCUpvalue* prev = nullptr;
    GCUpvalue* curr = openUpvalues_;
    while (curr && curr->location > slot) {
        prev = curr;
        curr = curr->next;
    }
    if (curr && curr->location == slot) return curr;
    auto* uv = gc_.newObj<GCUpvalue>(slot);
    uv->next = curr;
    if (prev) prev->next = uv;
    else openUpvalues_ = uv;
    return uv;
}

void ForgeVM::closeUpvalues(FValue* last) {
    while (openUpvalues_ && openUpvalues_->location >= last) {
        openUpvalues_->closed = *openUpvalues_->location;
        openUpvalues_->location = &openUpvalues_->closed;
        openUpvalues_ = openUpvalues_->next;
    }
}

bool ForgeVM::interpret(std::shared_ptr<GCFunction> function) {
    reset();

    auto* closure = gc_.newObj<GCClosure>(function.get());
    mainThread_.push(FValue::obj(closure));
    mainThread_.pushFrame(closure, mainThread_.stack.data(), 0);

    FVMThread& thread = mainThread_;
    FVMFrame* framePtr = &thread.currentFrame();

    #define FVM_READ_BYTE() (*framePtr->ip++)
    #define FVM_READ_UINT16() (framePtr->ip += 2, (uint16_t)((framePtr->ip[-2] << 8) | framePtr->ip[-1]))
    #define FVM_READ_CONSTANT() (valueToFValue(framePtr->closure->function->chunk->constants[FVM_READ_UINT16()]))

    while (thread.hasFrames()) {
        framePtr = &thread.currentFrame();

        try {
            uint8_t instruction = FVM_READ_BYTE();
            switch (instruction) {
                case OP_CONSTANT: { thread.push(FVM_READ_CONSTANT()); break; }
                case OP_NIL: thread.push(FValue::nil()); break;
                case OP_TRUE: thread.push(FValue::boolean(true)); break;
                case OP_FALSE: thread.push(FValue::boolean(false)); break;
                case OP_POP: thread.pop(); break;
                case OP_DUP: thread.push(thread.peek()); break;

                case OP_DEFINE_GLOBAL: {
                    FValue name = FVM_READ_CONSTANT();
                    globals_[name.asString()->value] = thread.peek();
                    thread.pop();
                    break;
                }
                case OP_GET_GLOBAL: {
                    FValue name = FVM_READ_CONSTANT();
                    auto it = globals_.find(name.asString()->value);
                    if (it == globals_.end()) {
                        if (!handleError("Undefined variable '" + name.asString()->value + "'")) return false;
                        break;
                    }
                    thread.push(it->second);
                    break;
                }
                case OP_SET_GLOBAL: {
                    FValue name = FVM_READ_CONSTANT();
                    if (globals_.find(name.asString()->value) == globals_.end()) {
                        if (!handleError("Undefined variable '" + name.asString()->value + "'")) return false;
                        break;
                    }
                    globals_[name.asString()->value] = thread.peek();
                    break;
                }
                case OP_GET_LOCAL: {
                    uint16_t slot = FVM_READ_UINT16();
                    thread.push(framePtr->slots[slot]);
                    break;
                }
                case OP_SET_LOCAL: {
                    uint16_t slot = FVM_READ_UINT16();
                    framePtr->slots[slot] = thread.peek();
                    break;
                }
                case OP_GET_UPVALUE: {
                    uint8_t idx = FVM_READ_BYTE();
                    thread.push(*framePtr->closure->upvalues[idx]->location);
                    break;
                }
                case OP_SET_UPVALUE: {
                    uint8_t idx = FVM_READ_BYTE();
                    *framePtr->closure->upvalues[idx]->location = thread.peek();
                    break;
                }
                case OP_CLOSE_UPVALUE: {
                    uint16_t slot = FVM_READ_UINT16();
                    closeUpvalues(framePtr->slots + slot);
                    break;
                }

                case OP_ADD: {
                    FValue b = thread.pop(); FValue a = thread.pop();
                    if (a.isNumber() && b.isNumber()) {
                        if (a.type == FValueType::VAL_INT && b.type == FValueType::VAL_INT)
                            thread.push(FValue::integer(a.as.integer + b.as.integer));
                        else thread.push(FValue::floating(a.asNumber() + b.asNumber()));
                    } else if (a.isString() && b.isString()) {
                        auto* s = gc_.newObj<GCString>(a.asString()->value + b.asString()->value);
                        thread.push(FValue::obj(s));
                    } else {
                        if (!handleError("Invalid operands for '+'")) return false;
                        break;
                    }
                    break;
                }
                case OP_SUBTRACT: {
                    FValue b = thread.pop(); FValue a = thread.pop();
                    if (a.isNumber() && b.isNumber()) {
                        if (a.type == FValueType::VAL_INT && b.type == FValueType::VAL_INT)
                            thread.push(FValue::integer(a.as.integer - b.as.integer));
                        else thread.push(FValue::floating(a.asNumber() - b.asNumber()));
                    } else { if (!handleError("Invalid operands for '-'")) return false; break; }
                    break;
                }
                case OP_MULTIPLY: {
                    FValue b = thread.pop(); FValue a = thread.pop();
                    if (a.isNumber() && b.isNumber()) {
                        if (a.type == FValueType::VAL_INT && b.type == FValueType::VAL_INT)
                            thread.push(FValue::integer(a.as.integer * b.as.integer));
                        else thread.push(FValue::floating(a.asNumber() * b.asNumber()));
                    } else { if (!handleError("Invalid operands for '*'")) return false; break; }
                    break;
                }
                case OP_DIVIDE: {
                    FValue b = thread.pop(); FValue a = thread.pop();
                    if (a.isNumber() && b.isNumber()) {
                        if (b.asNumber() == 0) { if (!handleError("Division by zero")) return false; break; }
                        if (a.type == FValueType::VAL_INT && b.type == FValueType::VAL_INT)
                            thread.push(FValue::integer(a.as.integer / b.as.integer));
                        else thread.push(FValue::floating(a.asNumber() / b.asNumber()));
                    } else { if (!handleError("Invalid operands for '/'")) return false; break; }
                    break;
                }
                case OP_MODULO: {
                    FValue b = thread.pop(); FValue a = thread.pop();
                    if (a.isNumber() && b.isNumber()) {
                        if (b.asNumber() == 0) { if (!handleError("Division by zero")) return false; break; }
                        if (a.type == FValueType::VAL_INT && b.type == FValueType::VAL_INT)
                            thread.push(FValue::integer(a.as.integer % b.as.integer));
                        else thread.push(FValue::floating(std::fmod(a.asNumber(), b.asNumber())));
                    } else { if (!handleError("Invalid operands for '%'")) return false; break; }
                    break;
                }
                case OP_BITWISE_AND: {
                    FValue b = thread.pop(); FValue a = thread.pop();
                    if (a.isNumber() && b.isNumber()) {
                        if (a.type == FValueType::VAL_INT && b.type == FValueType::VAL_INT)
                            thread.push(FValue::integer(a.as.integer & b.as.integer));
                        else { if (!handleError("Bitwise AND requires integer operands")) return false; break; }
                    } else { if (!handleError("Invalid operands for '&'")) return false; break; }
                    break;
                }
                case OP_BITWISE_OR: {
                    FValue b = thread.pop(); FValue a = thread.pop();
                    if (a.isNumber() && b.isNumber()) {
                        if (a.type == FValueType::VAL_INT && b.type == FValueType::VAL_INT)
                            thread.push(FValue::integer(a.as.integer | b.as.integer));
                        else { if (!handleError("Bitwise OR requires integer operands")) return false; break; }
                    } else { if (!handleError("Invalid operands for '|'")) return false; break; }
                    break;
                }
                case OP_BITWISE_XOR: {
                    FValue b = thread.pop(); FValue a = thread.pop();
                    if (a.isNumber() && b.isNumber()) {
                        if (a.type == FValueType::VAL_INT && b.type == FValueType::VAL_INT)
                            thread.push(FValue::integer(a.as.integer ^ b.as.integer));
                        else { if (!handleError("Bitwise XOR requires integer operands")) return false; break; }
                    } else { if (!handleError("Invalid operands for '^'")) return false; break; }
                    break;
                }
                case OP_NEGATE: {
                    FValue a = thread.pop();
                    if (a.isNumber()) {
                        if (a.type == FValueType::VAL_INT) thread.push(FValue::integer(-a.as.integer));
                        else thread.push(FValue::floating(-a.as.floating));
                    } else { if (!handleError("Cannot negate non-number")) return false; break; }
                    break;
                }
                case OP_NOT: thread.push(FValue::boolean(!thread.pop().isTruthy())); break;
                case OP_EQUAL: { FValue b = thread.pop(); FValue a = thread.pop(); thread.push(FValue::boolean(a.equals(b))); break; }
                case OP_NOT_EQUAL: { FValue b = thread.pop(); FValue a = thread.pop(); thread.push(FValue::boolean(!a.equals(b))); break; }
                case OP_LESS: {
                    FValue b = thread.pop(); FValue a = thread.pop();
                    if (a.isNumber() && b.isNumber()) thread.push(FValue::boolean(a.asNumber() < b.asNumber()));
                    else if (a.isString() && b.isString()) thread.push(FValue::boolean(a.asString()->value < b.asString()->value));
                    else { if (!handleError("Invalid comparison")) return false; break; }
                    break;
                }
                case OP_LESS_EQUAL: {
                    FValue b = thread.pop(); FValue a = thread.pop();
                    if (a.isNumber() && b.isNumber()) thread.push(FValue::boolean(a.asNumber() <= b.asNumber()));
                    else if (a.isString() && b.isString()) thread.push(FValue::boolean(a.asString()->value <= b.asString()->value));
                    else { if (!handleError("Invalid comparison")) return false; break; }
                    break;
                }
                case OP_GREATER: {
                    FValue b = thread.pop(); FValue a = thread.pop();
                    if (a.isNumber() && b.isNumber()) thread.push(FValue::boolean(a.asNumber() > b.asNumber()));
                    else if (a.isString() && b.isString()) thread.push(FValue::boolean(a.asString()->value > b.asString()->value));
                    else { if (!handleError("Invalid comparison")) return false; break; }
                    break;
                }
                case OP_GREATER_EQUAL: {
                    FValue b = thread.pop(); FValue a = thread.pop();
                    if (a.isNumber() && b.isNumber()) thread.push(FValue::boolean(a.asNumber() >= b.asNumber()));
                    else if (a.isString() && b.isString()) thread.push(FValue::boolean(a.asString()->value >= b.asString()->value));
                    else { if (!handleError("Invalid comparison")) return false; break; }
                    break;
                }

                case OP_JUMP: { uint16_t offset = FVM_READ_UINT16(); framePtr->ip += offset; break; }
                case OP_JUMP_IF_FALSE: { uint16_t offset = FVM_READ_UINT16(); if (!thread.peek().isTruthy()) framePtr->ip += offset; break; }
                case OP_JUMP_IF_TRUE: { uint16_t offset = FVM_READ_UINT16(); if (thread.peek().isTruthy()) framePtr->ip += offset; break; }
                case OP_LOOP: { uint16_t offset = FVM_READ_UINT16(); framePtr->ip -= offset; break; }

                case OP_CALL: {
                    uint8_t argCount = FVM_READ_BYTE();
                    FValue callee = thread.peek(argCount);
                    if (!thread.callValue(callee, argCount)) {
                        if (!handleError("Cannot call " + callee.toString())) return false;
                    } else {
                        framePtr = &thread.currentFrame();
                    }
                    break;
                }
                case OP_RETURN: {
                    FValue result = thread.pop();
                    FValue* returningSlots = framePtr->slots;
                    closeUpvalues(returningSlots);
                    thread.frameCount--;
                    if (thread.frameCount == 0) {
                        thread.push(result);
                        return true;
                    }
                    thread.stackTop = (int)(returningSlots - thread.stack.data());
                    thread.push(result);
                    framePtr = &thread.currentFrame();
                    break;
                }

                case OP_CLOSURE: {
                    FValue fnVal = FVM_READ_CONSTANT();
                    GCFunction* fn = fnVal.asFunction();
                    auto* cl = gc_.newObj<GCClosure>(fn);
                    for (int i = 0; i < fn->upvalueCount; i++) {
                        uint8_t isLocal = FVM_READ_BYTE();
                        uint8_t idx = FVM_READ_BYTE();
                        if (isLocal) {
                            cl->upvalues[i] = captureUpvalue(framePtr->slots + idx);
                        } else {
                            cl->upvalues[i] = framePtr->closure->upvalues[idx];
                        }
                    }
                    thread.push(FValue::obj(cl));
                    break;
                }

                case OP_PRINT: {
                    FValue val = thread.pop();
                    if (val.type != FValueType::VAL_NIL) std::cout << val.toString() << "\n";
                    break;
                }

                case OP_ARRAY: {
                    uint16_t count = FVM_READ_UINT16();
                    auto* arr = gc_.newObj<GCArray>();
                    for (int i = count - 1; i >= 0; i--) arr->elements.push_back(thread.peek(i));
                    for (int i = 0; i < count; i++) thread.pop();
                    thread.push(FValue::obj(arr));
                    break;
                }
                case OP_MAP: {
                    uint16_t count = FVM_READ_UINT16();
                    auto* map = gc_.newObj<GCMap>();
                    for (int i = 0; i < count; i++) {
                        FValue key = thread.peek(2 * i + 1);
                        FValue val = thread.peek(2 * i);
                        if (!key.isString()) {
                            if (!handleError("Map key must be a string")) return false;
                            break;
                        }
                        map->entries[key.asString()->value] = val;
                    }
                    for (int i = 0; i < count * 2; i++) thread.pop();
                    thread.push(FValue::obj(map));
                    break;
                }
                case OP_INDEX: {
                    FValue idx = thread.pop(); FValue obj = thread.pop();
                    if (obj.isArray()) {
                        long long i = idx.asInteger();
                        long long len = (long long)obj.asArray()->elements.size();
                        if (i < 0 || i >= len) { if (!handleError("Array index out of bounds")) return false; break; }
                        thread.push(obj.asArray()->elements[i]);
                    } else if (obj.isString()) {
                        long long i = idx.asInteger();
                        long long len = (long long)obj.asString()->value.size();
                        if (i < 0 || i >= len) { if (!handleError("String index out of bounds")) return false; break; }
                        auto* s = gc_.newObj<GCString>(std::string(1, obj.asString()->value[i]));
                        thread.push(FValue::obj(s));
                    } else if (obj.isMap()) {
                        if (!idx.isString()) { if (!handleError("Map key must be a string")) return false; break; }
                        auto it = obj.asMap()->entries.find(idx.asString()->value);
                        if (it == obj.asMap()->entries.end()) thread.push(FValue::nil());
                        else thread.push(it->second);
                    } else { if (!handleError("Cannot index into non-indexable value")) return false; break; }
                    break;
                }
                case OP_SET_INDEX: {
                    FValue val = thread.pop(); FValue idx = thread.pop(); FValue obj = thread.pop();
                    if (obj.isArray()) {
                        long long i = idx.asInteger();
                        long long len = (long long)obj.asArray()->elements.size();
                        if (i < 0 || i >= len) { if (!handleError("Array index out of bounds")) return false; break; }
                        obj.asArray()->elements[i] = val;
                        thread.push(val);
                    } else if (obj.isMap()) {
                        if (!idx.isString()) { if (!handleError("Map key must be a string")) return false; break; }
                        obj.asMap()->entries[idx.asString()->value] = val;
                        thread.push(val);
                    } else { if (!handleError("Cannot set index on non-indexable value")) return false; break; }
                    break;
                }
                case OP_INDEX_LEN: {
                    FValue obj = thread.pop();
                    if (obj.isArray()) thread.push(FValue::integer((long long)obj.asArray()->elements.size()));
                    else if (obj.isString()) thread.push(FValue::integer((long long)obj.asString()->value.size()));
                    else if (obj.isMap()) thread.push(FValue::integer((long long)obj.asMap()->entries.size()));
                    else { if (!handleError("Cannot get length of non-indexable value")) return false; break; }
                    break;
                }

                case OP_GET_PROPERTY: {
                    FValue name = FVM_READ_CONSTANT();
                    FValue obj = thread.pop();
                    if (obj.isInstance()) {
                        auto* inst = obj.asInstance();
                        auto it = inst->fields.find(name.asString()->value);
                        if (it != inst->fields.end()) { thread.push(it->second); }
                        else {
                            auto mit = inst->klass->methods.find(name.asString()->value);
                            if (mit != inst->klass->methods.end()) {
                                auto* bm = gc_.newObj<GCBoundMethod>(obj, mit->second.closure);
                                thread.push(FValue::obj(bm));
                            } else {
                                if (!handleError("Undefined property '" + name.asString()->value + "'")) return false;
                                break;
                            }
                        }
                    } else if (obj.isClass()) {
                        auto mit = obj.asClass()->methods.find(name.asString()->value);
                        if (mit != obj.asClass()->methods.end()) {
                            auto* bm = gc_.newObj<GCBoundMethod>(obj, mit->second.closure);
                            thread.push(FValue::obj(bm));
                        } else {
                            if (!handleError("Undefined method '" + name.asString()->value + "'")) return false;
                            break;
                        }
                    } else if (obj.isMap()) {
                        auto it = obj.asMap()->entries.find(name.asString()->value);
                        if (it != obj.asMap()->entries.end()) { thread.push(it->second); }
                        else {
                            if (!handleError("Undefined property '" + name.asString()->value + "'")) return false;
                            break;
                        }
                    } else if (obj.isArray()) {
                        std::string mname = name.asString()->value;
                        GCArray* arr = const_cast<GCArray*>(obj.asArray());
                        if (mname == "push") {
                            thread.push(FValue::obj(gc_.newObj<GCNative>([arr](const std::vector<FValue>& a) -> FValue {
                                for (auto& v : a) arr->elements.push_back(v);
                                return FValue::nil();
                            }, "array.push")));
                        } else if (mname == "pop") {
                            thread.push(FValue::obj(gc_.newObj<GCNative>([arr](const std::vector<FValue>&) -> FValue {
                                if (arr->elements.empty()) throw std::runtime_error("pop() on empty array");
                                FValue val = arr->elements.back();
                                arr->elements.pop_back();
                                return val;
                            }, "array.pop")));
                        } else if (mname == "len") {
                            thread.push(FValue::obj(gc_.newObj<GCNative>([arr](const std::vector<FValue>&) -> FValue {
                                return FValue::integer((long long)arr->elements.size());
                            }, "array.len")));
                        } else if (mname == "insert") {
                            thread.push(FValue::obj(gc_.newObj<GCNative>([arr](const std::vector<FValue>& a) -> FValue {
                                if (a.size() < 2) throw std::runtime_error("insert() expects (index, value)");
                                long long idx = a[0].asInteger();
                                if (idx < 0 || idx > (long long)arr->elements.size()) throw std::runtime_error("insert() index out of bounds");
                                arr->elements.insert(arr->elements.begin() + idx, a[1]);
                                return FValue::nil();
                            }, "array.insert")));
                        } else if (mname == "remove") {
                            thread.push(FValue::obj(gc_.newObj<GCNative>([arr](const std::vector<FValue>& a) -> FValue {
                                if (a.empty()) throw std::runtime_error("remove() expects (index)");
                                long long idx = a[0].asInteger();
                                if (idx < 0 || idx >= (long long)arr->elements.size()) throw std::runtime_error("remove() index out of bounds");
                                arr->elements.erase(arr->elements.begin() + idx);
                                return FValue::nil();
                            }, "array.remove")));
                        } else {
                            if (!handleError("Undefined array method '" + mname + "'")) return false;
                            break;
                        }
                    } else { if (!handleError("Cannot access property on non-object")) return false; break; }
                    break;
                }
                case OP_SET_PROPERTY: {
                    FValue name = FVM_READ_CONSTANT();
                    FValue val = thread.pop();
                    FValue obj = thread.pop();
                    if (!obj.isInstance()) { if (!handleError("Cannot set property on non-instance")) return false; break; }
                    obj.asInstance()->fields[name.asString()->value] = val;
                    thread.push(val);
                    break;
                }

                case OP_CLASS: {
                    FValue name = FVM_READ_CONSTANT();
                    auto* klass = gc_.newObj<GCClass>(name.asString()->value);
                    thread.push(FValue::obj(klass));
                    break;
                }
                case OP_METHOD: {
                    FValue name = FVM_READ_CONSTANT();
                    FValue methodVal = thread.peek();
                    FValue classVal = thread.peek(1);
                    if (!classVal.isClass()) { if (!handleError("Expected class for method")) return false; break; }
                    GCClosure* cl = methodVal.asClosure();
                    classVal.asClass()->methods[name.asString()->value] = {cl, false};
                    thread.pop();
                    break;
                }
                case OP_INHERIT: {
                    FValue superclass = thread.peek();
                    FValue subclass = thread.peek(1);
                    if (!superclass.isClass()) { if (!handleError("Superclass must be a class")) return false; break; }
                    subclass.asClass()->superclass = superclass.asClass();
                    for (auto& [n, m] : superclass.asClass()->methods) {
                        if (subclass.asClass()->methods.find(n) == subclass.asClass()->methods.end())
                            subclass.asClass()->methods[n] = m;
                    }
                    thread.pop();
                    break;
                }
                case OP_GET_SUPER: {
                    FValue name = FVM_READ_CONSTANT();
                    FValue instance = thread.pop();
                    if (!instance.isInstance()) { if (!handleError("Expected instance")) return false; break; }
                    GCClass* sup = instance.asInstance()->klass->superclass;
                    if (!sup) { if (!handleError("No superclass")) return false; break; }
                    auto it = sup->methods.find(name.asString()->value);
                    if (it == sup->methods.end()) { if (!handleError("Undefined method '" + name.asString()->value + "'")) return false; break; }
                    auto* bm = gc_.newObj<GCBoundMethod>(instance, it->second.closure);
                    thread.push(FValue::obj(bm));
                    break;
                }

                case OP_THROW: {
                    FValue val = thread.pop();
                    if (!tryFrames_.empty()) {
                        auto tf = tryFrames_.back();
                        tryFrames_.pop_back();
                        thread.frameCount = tf.frameCount;
                        thread.stackTop = tf.stackSize;
                        FVMFrame& cf = thread.currentFrame();
                        cf.ip = cf.closure->function->chunk->code.data() + tf.handlerOffset;
                        thread.push(val);
                    } else {
                        if (!handleError("Uncaught exception: " + val.toString())) return false;
                        break;
                    }
                    break;
                }
                case OP_TRY: {
                    uint16_t offset = FVM_READ_UINT16();
                    FVMFrame& cf = thread.currentFrame();
                    int currentOffset = (int)(cf.ip - cf.closure->function->chunk->code.data());
                    tryFrames_.push_back({thread.stackTop, thread.frameCount, currentOffset + offset});
                    break;
                }
                case OP_END_TRY: {
                    if (!tryFrames_.empty()) tryFrames_.pop_back();
                    break;
                }

                case OP_IMPORT: {
                    FValue name = FVM_READ_CONSTANT();
                    uint8_t paramCount = FVM_READ_BYTE();
                    (void)paramCount;
                    uint8_t libIdx = FVM_READ_BYTE();
                    (void)libIdx;
                    std::string modName = name.asString()->value;
                    auto it = modules_.find(modName);
                    if (it != modules_.end()) {
                        globals_[modName] = it->second;
                        thread.push(it->second);
                    } else {
                        auto* native = gc_.newObj<GCNative>(
                            [modName](const std::vector<FValue>&) -> FValue {
                                throw std::runtime_error("module '" + modName + "' not found");
                            }, modName);
                        globals_[modName] = FValue::obj(native);
                        thread.push(globals_[modName]);
                    }
                    break;
                }

                case OP_YIELD: { break; }
                case OP_NEXT: { break; }
                case OP_CREATE_GENERATOR: { break; }
                case OP_EOF: return true;

                default:
                    if (!handleError("Unknown instruction: " + std::to_string(instruction))) return false;
                    break;
            }
        } catch (const std::exception& e) {
            if (!handleError(e.what())) return false;
        }
    }
    return true;

    #undef FVM_READ_BYTE
    #undef FVM_READ_UINT16
    #undef FVM_READ_CONSTANT
}

bool ForgeVM::interpretSource(const std::string& source, const std::string& filename) {
    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Compiler compiler;
        auto fn = compiler.compile(program);
        if (compiler.hasError()) {
            std::cerr << "Compile Error: " << compiler.errorMessage() << "\n";
            return false;
        }

        auto* gcFunc = gc_.newObj<GCFunction>();
        gcFunc->ownedChunk = std::make_unique<Chunk>();
        gcFunc->ownedChunk->code = std::move(fn->chunk->code);
        gcFunc->ownedChunk->constants = std::move(fn->chunk->constants);
        gcFunc->ownedChunk->lines = std::move(fn->chunk->lines);
        gcFunc->ownedChunk->name = fn->chunk->name;
        gcFunc->chunk = gcFunc->ownedChunk.get();
        gcFunc->arity = fn->arity;
        gcFunc->upvalueCount = fn->upvalueCount;
        gcFunc->localCount = fn->localCount;
        gcFunc->name = fn->name;

        auto sharedFn = std::shared_ptr<GCFunction>(gcFunc, [](GCFunction*){});
        return interpret(sharedFn);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return false;
    }
}

// ============================================================
// .fclass binary loading & execution
// ============================================================

static std::vector<uint8_t> readFileBytes(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) throw std::runtime_error("Cannot open file: " + path);
    size_t size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

bool ForgeVM::interpretClassFile(const std::string& path) {
    try {
        auto data = readFileBytes(path);
        ClassFile cf;
        if (!cf.load(data.data(), data.size())) {
            std::cerr << "Error: Invalid .fclass file: " << path << "\n";
            return false;
        }
        return interpretClassFileData(cf, path);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return false;
    }
}

bool ForgeVM::interpretClassFileData(const ClassFile& cf, const std::string& filename) {
    // Phase 1: Create ObjFunction objects for each method
    // (valueToFValue converts these to GCFunction at runtime)
    std::vector<ObjFunction*> methodFns(cf.methods.size(), nullptr);
    for (size_t mi = 0; mi < cf.methods.size(); mi++) {
        auto& method = cf.methods[mi];
        if (method.bytecode.empty()) continue;
        auto* ofn = new ObjFunction();
        if (method.nameIndex > 0 && method.nameIndex < cf.constantPool.size()
            && cf.constantPool[method.nameIndex].tag == CPTag::UTF8) {
            ofn->name = cf.getUtf8(method.nameIndex);
        } else {
            ofn->name = "method_" + std::to_string(mi);
        }
        ofn->arity = method.arity;
        ofn->upvalueCount = method.upvalueCount;
        ofn->localCount = method.maxLocals;
        ofn->chunkPtr = std::make_shared<Chunk>();
        ofn->chunkPtr->code = method.bytecode;
        ofn->chunkPtr->lines.resize(method.bytecode.size(), 1);
        ofn->chunk = ofn->chunkPtr.get();
        methodFns[mi] = ofn;
    }

    // Phase 2: Build per-method chunk constants from their CP slices
    for (size_t mi = 0; mi < cf.methods.size(); mi++) {
        if (!methodFns[mi]) continue;
        auto& method = cf.methods[mi];
        auto* ofn = methodFns[mi];

        uint16_t offset = method.constantPoolOffset;
        uint16_t count = method.constantPoolCount;
        if (count == 0 && offset == 0) {
            offset = 1;
            count = (uint16_t)(cf.constantPool.size() - 1);
        }

        ofn->chunk->constants.clear();
        ofn->chunk->constants.reserve(count);
        for (uint16_t ci = 0; ci < count; ci++) {
            size_t cpIdx = offset + ci;
            if (cpIdx >= cf.constantPool.size()) {
                ofn->chunk->constants.push_back(Value::nil());
                continue;
            }
            auto& cp = cf.constantPool[cpIdx];
            switch (cp.tag) {
                case CPTag::UTF8:
                    ofn->chunk->constants.push_back(
                        Value::obj(std::make_shared<ObjString>(cp.getUtf8())));
                    break;
                case CPTag::INTEGER:
                    ofn->chunk->constants.push_back(Value::integer(cp.getInteger()));
                    break;
                case CPTag::FLOAT:
                    ofn->chunk->constants.push_back(Value::floating(cp.getFloat()));
                    break;
                case CPTag::LONG:
                    ofn->chunk->constants.push_back(Value::integer(cp.getLong()));
                    break;
                case CPTag::DOUBLE:
                    ofn->chunk->constants.push_back(Value::floating(cp.getDouble()));
                    break;
                case CPTag::BOOLEAN:
                    ofn->chunk->constants.push_back(Value::boolean(cp.data[0] != 0));
                    break;
                case CPTag::STRING:
                    ofn->chunk->constants.push_back(
                        Value::obj(std::make_shared<ObjString>(cf.getUtf8(cp.getStringIndex()))));
                    break;
                case CPTag::FORGE_METHOD: {
                    uint16_t targetIdx = (uint16_t)((cp.data[0] << 8) | cp.data[1]);
                    if (targetIdx < methodFns.size() && methodFns[targetIdx]) {
                        ofn->chunk->constants.push_back(
                            Value::obj(std::shared_ptr<ObjFunction>(methodFns[targetIdx],
                                [](ObjFunction*){})));
                    } else {
                        ofn->chunk->constants.push_back(Value::nil());
                    }
                    break;
                }
                default:
                    ofn->chunk->constants.push_back(Value::nil());
                    break;
            }
        }
    }

    // Find the first method with bytecode to execute
    ObjFunction* mainFn = nullptr;
    for (size_t mi = 0; mi < methodFns.size(); mi++) {
        if (methodFns[mi]) {
            mainFn = methodFns[mi];
            break;
        }
    }

    if (!mainFn) {
        std::cerr << "Error: No executable methods in .fclass file.\n";
        return false;
    }

    // Register non-main methods as globals (if they have unique names)
    for (size_t mi = 0; mi < methodFns.size(); mi++) {
        if (!methodFns[mi] || methodFns[mi] == mainFn) continue;
        auto* ofn = methodFns[mi];
        if (!ofn->name.empty() && ofn->name != mainFn->name) {
            auto sharedFn = std::shared_ptr<ObjFunction>(ofn, [](ObjFunction*){});
            globals_[ofn->name] = valueToFValue(Value::obj(sharedFn));
        }
    }

    // Convert main function to GCFunction via valueToFValue
    auto mainShared = std::shared_ptr<ObjFunction>(mainFn, [](ObjFunction*){});
    FValue gcMain = valueToFValue(Value::obj(mainShared));

    std::cerr << "Running " << filename << " (" << mainFn->chunk->code.size() << " bytes)\n";
    auto sharedFn = std::shared_ptr<GCFunction>(gcMain.asFunction(), [](GCFunction*){});
    return interpret(sharedFn);
}

// ============================================================
// Native Methods & Builtins
// ============================================================

void ForgeVM::defineNative(const std::string& name, GCNative::NativeFn fn, int arity) {
    auto* native = gc_.newObj<GCNative>(std::move(fn), name);
    native->arity = arity;
    globals_[name] = FValue::obj(native);
}

void ForgeVM::defineModule(const std::string& name, GCMap* module) {
    modules_[name] = FValue::obj(module);
}

void ForgeVM::defineBuiltins() {
    defineNative("print", [](const std::vector<FValue>& args) -> FValue {
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) std::cout << " ";
            std::cout << args[i].toString();
        }
        std::cout << "\n";
        return FValue::nil();
    });

    defineNative("len", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("len() expects 1 argument");
        if (args[0].isString()) return FValue::integer((long long)args[0].asString()->value.size());
        if (args[0].isArray()) return FValue::integer((long long)args[0].asArray()->elements.size());
        if (args[0].isMap()) return FValue::integer((long long)args[0].asMap()->entries.size());
        throw std::runtime_error("len() expects string, array, or map");
    });

    defineNative("push", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 2) throw std::runtime_error("push() expects 2 arguments");
        if (!args[0].isArray()) throw std::runtime_error("push() expects array");
        const_cast<GCArray*>(args[0].asArray())->elements.push_back(args[1]);
        return args[0];
    });

    defineNative("str", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("str() expects 1 argument");
        auto* s = new GCString(args[0].toString());
        return FValue::obj(s);
    });

    defineNative("int", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("int() expects 1 argument");
        return FValue::integer(args[0].asInteger());
    });

    defineNative("float", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("float() expects 1 argument");
        return FValue::floating(args[0].asNumber());
    });

    defineNative("type", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("type() expects 1 argument");
        std::string t;
        switch (args[0].type) {
            case FValueType::VAL_NIL: t = "nil"; break;
            case FValueType::VAL_BOOL: t = "bool"; break;
            case FValueType::VAL_INT: t = "int"; break;
            case FValueType::VAL_FLOAT: t = "float"; break;
            case FValueType::VAL_OBJ:
                if (args[0].isString()) t = "string";
                else if (args[0].isArray()) t = "array";
                else if (args[0].isMap()) t = "map";
                else if (args[0].isClass()) t = "class";
                else if (args[0].isInstance()) t = "instance";
                else t = "object";
                break;
        }
        auto* s = new GCString(t);
        return FValue::obj(s);
    });

    defineNative("error", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("error() expects 1 argument");
        throw std::runtime_error(args[0].toString());
    });

    defineNative("keys", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("keys() expects 1 argument");
        if (!args[0].isMap()) throw std::runtime_error("keys() expects a map");
        auto* arr = new GCArray();
        for (auto& [k, v] : args[0].asMap()->entries) {
            arr->elements.push_back(FValue::obj(new GCString(k)));
        }
        return FValue::obj(arr);
    });

    defineNative("values", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("values() expects 1 argument");
        if (!args[0].isMap()) throw std::runtime_error("values() expects a map");
        auto* arr = new GCArray();
        for (auto& [k, v] : args[0].asMap()->entries)
            arr->elements.push_back(v);
        return FValue::obj(arr);
    });

    defineNative("has", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 2) throw std::runtime_error("has() expects 2 arguments");
        if (!args[0].isMap()) throw std::runtime_error("has() expects a map");
        if (!args[1].isString()) throw std::runtime_error("has() expects a string key");
        return FValue::boolean(args[0].asMap()->entries.find(args[1].asString()->value) != args[0].asMap()->entries.end());
    });

    defineNative("entries", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("entries() expects 1 argument");
        if (!args[0].isMap()) throw std::runtime_error("entries() expects a map");
        auto* arr = new GCArray();
        for (auto& [k, v] : args[0].asMap()->entries) {
            auto* pair = new GCArray();
            pair->elements.push_back(FValue::obj(new GCString(k)));
            pair->elements.push_back(v);
            arr->elements.push_back(FValue::obj(pair));
        }
        return FValue::obj(arr);
    });

    defineNative("clone", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("clone() expects 1 argument");
        if (args[0].isMap()) {
            auto* m = new GCMap();
            m->entries = args[0].asMap()->entries;
            return FValue::obj(m);
        }
        if (args[0].isArray()) {
            auto* a = new GCArray();
            a->elements = args[0].asArray()->elements;
            return FValue::obj(a);
        }
        throw std::runtime_error("clone() expects map or array");
    });

    defineNative("upper", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1 || !args[0].isString()) throw std::runtime_error("upper() expects 1 string argument");
        std::string s = args[0].asString()->value;
        for (auto& c : s) c = std::toupper(c);
        return FValue::obj(new GCString(s));
    });

    defineNative("lower", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1 || !args[0].isString()) throw std::runtime_error("lower() expects 1 string argument");
        std::string s = args[0].asString()->value;
        for (auto& c : s) c = std::tolower(c);
        return FValue::obj(new GCString(s));
    });

    defineNative("trim", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1 || !args[0].isString()) throw std::runtime_error("trim() expects 1 string argument");
        std::string s = args[0].asString()->value;
        size_t start = s.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return FValue::obj(new GCString(""));
        size_t end = s.find_last_not_of(" \t\n\r");
        return FValue::obj(new GCString(s.substr(start, end - start + 1)));
    });

    defineNative("split", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 2) throw std::runtime_error("split() expects 2 arguments");
        if (!args[0].isString() || !args[1].isString()) throw std::runtime_error("split() expects 2 strings");
        std::string s = args[0].asString()->value;
        std::string delim = args[1].asString()->value;
        auto* arr = new GCArray();
        if (delim.empty()) {
            for (char c : s) arr->elements.push_back(FValue::obj(new GCString(std::string(1, c))));
        } else {
            size_t pos = 0;
            while ((pos = s.find(delim)) != std::string::npos) {
                arr->elements.push_back(FValue::obj(new GCString(s.substr(0, pos))));
                s.erase(0, pos + delim.size());
            }
            arr->elements.push_back(FValue::obj(new GCString(s)));
        }
        return FValue::obj(arr);
    });

    defineNative("contains", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 2) throw std::runtime_error("contains() expects 2 arguments");
        if (!args[0].isString() || !args[1].isString()) throw std::runtime_error("contains() expects 2 strings");
        return FValue::boolean(args[0].asString()->value.find(args[1].asString()->value) != std::string::npos);
    });

    defineNative("replace", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 3) throw std::runtime_error("replace() expects 3 arguments");
        if (!args[0].isString() || !args[1].isString() || !args[2].isString())
            throw std::runtime_error("replace() expects 3 strings");
        std::string s = args[0].asString()->value;
        std::string old = args[1].asString()->value;
        std::string repl = args[2].asString()->value;
        size_t pos = 0;
        while ((pos = s.find(old, pos)) != std::string::npos) {
            s.replace(pos, old.size(), repl);
            pos += repl.size();
        }
        return FValue::obj(new GCString(s));
    });

    defineNative("substring", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() < 2 || args.size() > 3) throw std::runtime_error("substring() expects 2 or 3 arguments");
        if (!args[0].isString()) throw std::runtime_error("substring() expects a string");
        std::string s = args[0].asString()->value;
        long long start = args[1].asInteger();
        long long end = (args.size() == 3) ? args[2].asInteger() : (long long)s.size();
        if (start < 0) start = 0;
        if (end > (long long)s.size()) end = s.size();
        if (start > end) start = end;
        return FValue::obj(new GCString(s.substr(start, end - start)));
    });

    defineNative("charAt", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 2) throw std::runtime_error("charAt() expects 2 arguments");
        if (!args[0].isString()) throw std::runtime_error("charAt() expects a string");
        long long i = args[1].asInteger();
        std::string s = args[0].asString()->value;
        if (i < 0 || i >= (long long)s.size()) throw std::runtime_error("charAt() index out of bounds");
        return FValue::obj(new GCString(std::string(1, s[i])));
    });

    defineNative("parseInt", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("parseInt() expects 1 argument");
        if (!args[0].isString()) throw std::runtime_error("parseInt() expects a string");
        return FValue::integer(std::stoll(args[0].asString()->value));
    });

    defineNative("abs", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("abs() expects 1 argument");
        if (args[0].type == FValueType::VAL_INT) return FValue::integer(std::llabs(args[0].as.integer));
        return FValue::floating(std::abs(args[0].asNumber()));
    });

    defineNative("min", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 2) throw std::runtime_error("min() expects 2 arguments");
        if (args[0].isNumber() && args[1].isNumber())
            return (args[0].asNumber() < args[1].asNumber()) ? args[0] : args[1];
        throw std::runtime_error("min() expects numbers");
    });

    defineNative("max", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 2) throw std::runtime_error("max() expects 2 arguments");
        if (args[0].isNumber() && args[1].isNumber())
            return (args[0].asNumber() > args[1].asNumber()) ? args[0] : args[1];
        throw std::runtime_error("max() expects numbers");
    });

    defineNative("sqrt", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("sqrt() expects 1 argument");
        return FValue::floating(std::sqrt(args[0].asNumber()));
    });

    defineNative("pow", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 2) throw std::runtime_error("pow() expects 2 arguments");
        return FValue::floating(std::pow(args[0].asNumber(), args[1].asNumber()));
    });

    defineNative("floor", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("floor() expects 1 argument");
        double v = std::floor(args[0].asNumber());
        if (v == (long long)v && v < 1e15) return FValue::integer((long long)v);
        return FValue::floating(v);
    });

    defineNative("ceil", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("ceil() expects 1 argument");
        double v = std::ceil(args[0].asNumber());
        if (v == (long long)v && v < 1e15) return FValue::integer((long long)v);
        return FValue::floating(v);
    });

    defineNative("round", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("round() expects 1 argument");
        double v = std::round(args[0].asNumber());
        if (v == (long long)v && v < 1e15) return FValue::integer((long long)v);
        return FValue::floating(v);
    });

    static std::mt19937 rng(std::random_device{}());
    defineNative("random", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 0) throw std::runtime_error("random() expects 0 arguments");
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return FValue::floating(dist(rng));
    });

    defineNative("randomInt", [](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 2) throw std::runtime_error("randomInt() expects 2 arguments");
        long long min = args[0].asInteger();
        long long max = args[1].asInteger();
        if (min > max) throw std::runtime_error("randomInt() min must be <= max");
        std::uniform_int_distribution<long long> dist(min, max);
        return FValue::integer(dist(rng));
    });
}

void ForgeVM::defineModules() {
    {
        auto* ioMod = new GCMap();
        ioMod->entries["write"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() < 1 || args.size() > 2) throw std::runtime_error("io.write() expects 1 or 2 arguments");
            std::string s = args[0].toString();
            std::string dest = args.size() > 1 && args[1].isString() ? args[1].asString()->value : "stdout";
            if (dest == "stdout") std::cout << s;
            else if (dest == "stderr") std::cerr << s;
            else {
                std::ofstream ofs(dest);
                if (ofs.is_open()) {
                    ofs << s;
                    ofs.close();
                }
            }
            return FValue::nil();
        }, "io.write"));
        ioMod->entries["read"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() != 1) throw std::runtime_error("io.read() expects 1 argument");
            std::string path = args[0].asString()->value;
            std::ifstream file(path);
            if (!file.is_open()) throw std::runtime_error("io.read() cannot open file: " + path);
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            return FValue::obj(new GCString(content));
        }, "io.read"));
        modules_["io"] = FValue::obj(ioMod);
    }
    {
        auto* osMod = new GCMap();
        osMod->entries["time"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            return FValue::integer((long long)std::time(nullptr));
        }, "os.time"));
        osMod->entries["execute"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() != 1) throw std::runtime_error("os.execute() expects 1 argument");
            std::string cmd = args[0].asString()->value;
            int result = std::system(cmd.c_str());
            return FValue::integer((long long)result);
        }, "os.execute"));
        osMod->entries["capture"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() != 1) throw std::runtime_error("os.capture() expects 1 argument");
            std::string cmd = args[0].asString()->value;
            std::string output;
            FILE* pipe = popen((cmd + " 2>/dev/null").c_str(), "r");
            if (!pipe) return FValue::nil();
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe)) {
                output += buffer;
            }
            pclose(pipe);
            return FValue::obj(new GCString(output));
        }, "os.capture"));
        osMod->entries["args"] = FValue::obj(new GCNative([this](const std::vector<FValue>&) -> FValue {
            auto* arr = new GCArray();
            for (int i = 0; i < savedArgc_; i++) {
                arr->elements.push_back(FValue::obj(new GCString(savedArgv_[i])));
            }
            return FValue::obj(arr);
        }, "os.args"));
        modules_["os"] = FValue::obj(osMod);
    }
    {
        auto* jsonMod = new GCMap();
        jsonMod->entries["stringify"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() != 1) throw std::runtime_error("json.stringify() expects 1 argument");
            return FValue::obj(new GCString(args[0].toString()));
        }, "json.stringify"));
        jsonMod->entries["parse"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() != 1) throw std::runtime_error("json.parse() expects 1 argument");
            return FValue::nil();
        }, "json.parse"));
        modules_["json"] = FValue::obj(jsonMod);
    }
    {
        auto* pathMod = new GCMap();
        pathMod->entries["join"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() < 1) throw std::runtime_error("path.join() expects at least 1 argument");
            std::string result;
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) result += "/";
                result += args[i].asString()->value;
            }
            return FValue::obj(new GCString(result));
        }, "path.join"));
        pathMod->entries["base"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() != 1) throw std::runtime_error("path.base() expects 1 argument");
            std::string p = args[0].asString()->value;
            size_t pos = p.find_last_of("/\\");
            return FValue::obj(new GCString(pos != std::string::npos ? p.substr(pos + 1) : p));
        }, "path.base"));
        modules_["path"] = FValue::obj(pathMod);
    }
    {
        auto* sysMod = new GCMap();
        sysMod->entries["version"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            return FValue::obj(new GCString("2.0.0"));
        }, "system.version"));
        sysMod->entries["platform"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
#ifdef __linux__
            return FValue::obj(new GCString("linux"));
#elif __APPLE__
            return FValue::obj(new GCString("darwin"));
#else
            return FValue::obj(new GCString("unknown"));
#endif
        }, "system.platform"));
        modules_["system"] = FValue::obj(sysMod);
    }
    {
        static struct termios origTermios;
        static bool rawModeActive = false;

        auto* uiMod = new GCMap();

        uiMod->entries["init"] = FValue::obj(new GCNative([](const std::vector<FValue>&) -> FValue {
            std::cout << "\x1b[?25l" << std::flush;
            return FValue::nil();
        }, "ui.init"));

        uiMod->entries["cleanup"] = FValue::obj(new GCNative([](const std::vector<FValue>&) -> FValue {
            std::cout << "\x1b[?25h\x1b[0m\x1b[2J\x1b[H" << std::flush;
            if (rawModeActive) {
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios);
                rawModeActive = false;
            }
            return FValue::nil();
        }, "ui.cleanup"));

        uiMod->entries["enable_raw_mode"] = FValue::obj(new GCNative([](const std::vector<FValue>&) -> FValue {
            if (!isatty(STDIN_FILENO)) return FValue::nil();
            tcgetattr(STDIN_FILENO, &origTermios);
            struct termios raw = origTermios;
            raw.c_lflag &= ~(ECHO | ICANON | ISIG);
            raw.c_iflag &= ~(IXON | ICRNL);
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 1;
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
            rawModeActive = true;
            return FValue::nil();
        }, "ui.enable_raw_mode"));

        uiMod->entries["disable_raw_mode"] = FValue::obj(new GCNative([](const std::vector<FValue>&) -> FValue {
            if (rawModeActive) {
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios);
                rawModeActive = false;
            }
            return FValue::nil();
        }, "ui.disable_raw_mode"));

        uiMod->entries["clear"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            int bg = args.size() > 0 && args[0].isNumber() ? (int)args[0].asInteger() : 40;
            std::cout << "\x1b[2J\x1b[H\x1b[48;5;" << bg << "m" << std::flush;
            return FValue::nil();
        }, "ui.clear"));

        uiMod->entries["flush"] = FValue::obj(new GCNative([](const std::vector<FValue>&) -> FValue {
            std::cout << std::flush;
            return FValue::nil();
        }, "ui.flush"));

        uiMod->entries["draw_text"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() < 3) throw std::runtime_error("ui.draw_text(x, y, text, fg, bg, bold)");
            int x = (int)args[0].asInteger();
            int y = (int)args[1].asInteger();
            std::string text = args[2].toString();
            int fg = args.size() > 3 && args[3].isNumber() ? (int)args[3].asInteger() : 97;
            int bg = args.size() > 4 && args[4].isNumber() ? (int)args[4].asInteger() : 40;
            bool bold = args.size() > 5 && args[5].isTruthy();
            std::cout << "\x1b[" << y << ";" << x << "H";
            if (bold) std::cout << "\x1b[1m";
            std::cout << "\x1b[38;5;" << fg << "m\x1b[48;5;" << bg << "m" << text;
            if (bold) std::cout << "\x1b[22m";
            return FValue::nil();
        }, "ui.draw_text"));

        uiMod->entries["draw_char"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() < 3) throw std::runtime_error("ui.draw_char(x, y, char, fg, bg)");
            int x = (int)args[0].asInteger();
            int y = (int)args[1].asInteger();
            int ch = args[2].isNumber() ? (int)args[2].asInteger() : (int)args[2].toString()[0];
            int fg = args.size() > 3 && args[3].isNumber() ? (int)args[3].asInteger() : 97;
            int bg = args.size() > 4 && args[4].isNumber() ? (int)args[4].asInteger() : 40;
            std::cout << "\x1b[" << y << ";" << x << "H"
                      << "\x1b[38;5;" << fg << "m\x1b[48;5;" << bg << "m"
                      << (char)ch;
            return FValue::nil();
        }, "ui.draw_char"));

        uiMod->entries["draw_rect"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() < 5) throw std::runtime_error("ui.draw_rect(x, y, w, h, bg)");
            int x = (int)args[0].asInteger();
            int y = (int)args[1].asInteger();
            int w = (int)args[2].asInteger();
            int h = (int)args[3].asInteger();
            int bg = (int)args[4].asInteger();
            for (int row = 0; row < h; row++) {
                std::cout << "\x1b[" << (y + row) << ";" << x << "H"
                          << "\x1b[48;5;" << bg << "m";
                for (int col = 0; col < w; col++) std::cout << ' ';
            }
            return FValue::nil();
        }, "ui.draw_rect"));

        uiMod->entries["draw_hline"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() < 4) throw std::runtime_error("ui.draw_hline(x, y, w, color)");
            int x = (int)args[0].asInteger();
            int y = (int)args[1].asInteger();
            int w = (int)args[2].asInteger();
            int color = (int)args[3].asInteger();
            std::cout << "\x1b[" << y << ";" << x << "H\x1b[48;5;" << color << "m";
            for (int i = 0; i < w; i++) std::cout << "\xe2\x94\x80";
            return FValue::nil();
        }, "ui.draw_hline"));

        uiMod->entries["draw_vline"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() < 4) throw std::runtime_error("ui.draw_vline(x, y, h, color)");
            int x = (int)args[0].asInteger();
            int y = (int)args[1].asInteger();
            int h = (int)args[2].asInteger();
            int color = (int)args[3].asInteger();
            for (int i = 0; i < h; i++) {
                std::cout << "\x1b[" << (y + i) << ";" << x << "H\x1b[48;5;" << color << "m"
                          << "\xe2\x94\x82";
            }
            return FValue::nil();
        }, "ui.draw_vline"));

        uiMod->entries["draw_hline_bg"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() < 5) throw std::runtime_error("ui.draw_hline_bg(x, y, w, fg, bg)");
            int x = (int)args[0].asInteger();
            int y = (int)args[1].asInteger();
            int w = (int)args[2].asInteger();
            int fg = (int)args[3].asInteger();
            int bg = (int)args[4].asInteger();
            std::cout << "\x1b[" << y << ";" << x << "H\x1b[38;5;" << fg << "m\x1b[48;5;" << bg << "m";
            for (int i = 0; i < w; i++) std::cout << "\xe2\x94\x80";
            return FValue::nil();
        }, "ui.draw_hline_bg"));

        uiMod->entries["draw_corner_tl"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() < 4) throw std::runtime_error("ui.draw_corner_tl(x, y, fg, bg)");
            int x = (int)args[0].asInteger(), y = (int)args[1].asInteger();
            int fg = (int)args[2].asInteger(), bg = (int)args[3].asInteger();
            std::cout << "\x1b[" << y << ";" << x << "H\x1b[38;5;" << fg << "m\x1b[48;5;" << bg << "m\xe2\x94\x8c";
            return FValue::nil();
        }, "ui.draw_corner_tl"));

        uiMod->entries["draw_corner_tr"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() < 4) throw std::runtime_error("ui.draw_corner_tr(x, y, fg, bg)");
            int x = (int)args[0].asInteger(), y = (int)args[1].asInteger();
            int fg = (int)args[2].asInteger(), bg = (int)args[3].asInteger();
            std::cout << "\x1b[" << y << ";" << x << "H\x1b[38;5;" << fg << "m\x1b[48;5;" << bg << "m\xe2\x94\x90";
            return FValue::nil();
        }, "ui.draw_corner_tr"));

        uiMod->entries["draw_corner_bl"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() < 4) throw std::runtime_error("ui.draw_corner_bl(x, y, fg, bg)");
            int x = (int)args[0].asInteger(), y = (int)args[1].asInteger();
            int fg = (int)args[2].asInteger(), bg = (int)args[3].asInteger();
            std::cout << "\x1b[" << y << ";" << x << "H\x1b[38;5;" << fg << "m\x1b[48;5;" << bg << "m\xe2\x94\x94";
            return FValue::nil();
        }, "ui.draw_corner_bl"));

        uiMod->entries["draw_corner_br"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() < 4) throw std::runtime_error("ui.draw_corner_br(x, y, fg, bg)");
            int x = (int)args[0].asInteger(), y = (int)args[1].asInteger();
            int fg = (int)args[2].asInteger(), bg = (int)args[3].asInteger();
            std::cout << "\x1b[" << y << ";" << x << "H\x1b[38;5;" << fg << "m\x1b[48;5;" << bg << "m\xe2\x94\x98";
            return FValue::nil();
        }, "ui.draw_corner_br"));

        uiMod->entries["draw_tee_l"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() < 4) throw std::runtime_error("ui.draw_tee_l(x, y, fg, bg)");
            int x = (int)args[0].asInteger(), y = (int)args[1].asInteger();
            int fg = (int)args[2].asInteger(), bg = (int)args[3].asInteger();
            std::cout << "\x1b[" << y << ";" << x << "H\x1b[38;5;" << fg << "m\x1b[48;5;" << bg << "m\xe2\x94\x9c";
            return FValue::nil();
        }, "ui.draw_tee_l"));

        uiMod->entries["draw_tee_r"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() < 4) throw std::runtime_error("ui.draw_tee_r(x, y, fg, bg)");
            int x = (int)args[0].asInteger(), y = (int)args[1].asInteger();
            int fg = (int)args[2].asInteger(), bg = (int)args[3].asInteger();
            std::cout << "\x1b[" << y << ";" << x << "H\x1b[38;5;" << fg << "m\x1b[48;5;" << bg << "m\xe2\x94\xa4";
            return FValue::nil();
        }, "ui.draw_tee_r"));

        uiMod->entries["get_size"] = FValue::obj(new GCNative([](const std::vector<FValue>&) -> FValue {
            struct winsize ws;
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
                auto* arr = new GCArray();
                arr->elements.push_back(FValue::integer(ws.ws_col));
                arr->elements.push_back(FValue::integer(ws.ws_row));
                return FValue::obj(arr);
            }
            auto* arr = new GCArray();
            arr->elements.push_back(FValue::integer(80));
            arr->elements.push_back(FValue::integer(24));
            return FValue::obj(arr);
        }, "ui.get_size"));

        uiMod->entries["read_key"] = FValue::obj(new GCNative([](const std::vector<FValue>&) -> FValue {
            char c = 0;
            if (read(STDIN_FILENO, &c, 1) == 1) {
                if (c == 27) {
                    char seq[8] = {0};
                    int n = read(STDIN_FILENO, &seq[0], 7);
                    if (n >= 1 && seq[0] == '[') {
                        if (n >= 2 && seq[1] >= 'A' && seq[1] <= 'D') {
                            auto* m = new GCMap();
                            m->entries["type"] = FValue::obj(new GCString("arrow"));
                            switch (seq[1]) {
                                case 'A': m->entries["key"] = FValue::obj(new GCString("up")); break;
                                case 'B': m->entries["key"] = FValue::obj(new GCString("down")); break;
                                case 'C': m->entries["key"] = FValue::obj(new GCString("right")); break;
                                case 'D': m->entries["key"] = FValue::obj(new GCString("left")); break;
                            }
                            if (n >= 4 && seq[2] == ';' && seq[3] == '5') {
                                m->entries["ctrl"] = FValue::boolean(true);
                            }
                            return FValue::obj(m);
                        }
                        if (n >= 2 && seq[1] >= '1' && seq[1] <= '6') {
                            char final = (n >= 3) ? seq[2] : 0;
                            if (n >= 3 && seq[2] == '~') {
                                auto* m = new GCMap();
                                m->entries["type"] = FValue::obj(new GCString("special"));
                                switch (seq[1]) {
                                    case '1': m->entries["key"] = FValue::obj(new GCString("home")); break;
                                    case '2': m->entries["key"] = FValue::obj(new GCString("insert")); break;
                                    case '3': m->entries["key"] = FValue::obj(new GCString("delete")); break;
                                    case '4': m->entries["key"] = FValue::obj(new GCString("end")); break;
                                    case '5': m->entries["key"] = FValue::obj(new GCString("pageup")); break;
                                    case '6': m->entries["key"] = FValue::obj(new GCString("pagedown")); break;
                                    default: m->entries["key"] = FValue::obj(new GCString("unknown")); break;
                                }
                                return FValue::obj(m);
                            }
                            if (n >= 3 && (seq[2] == 'H' || seq[2] == 'F')) {
                                auto* m = new GCMap();
                                m->entries["type"] = FValue::obj(new GCString("special"));
                                if (seq[2] == 'H') m->entries["key"] = FValue::obj(new GCString("home"));
                                else m->entries["key"] = FValue::obj(new GCString("end"));
                                return FValue::obj(m);
                            }
                        }
                        if (n >= 1 && seq[0] == 'O') {
                            auto* m = new GCMap();
                            m->entries["type"] = FValue::obj(new GCString("special"));
                            if (n >= 2) {
                                switch (seq[1]) {
                                    case 'H': m->entries["key"] = FValue::obj(new GCString("home")); break;
                                    case 'F': m->entries["key"] = FValue::obj(new GCString("end")); break;
                                    case 'P': m->entries["key"] = FValue::obj(new GCString("insert")); break;
                                    case 'Q': m->entries["key"] = FValue::obj(new GCString("pageup")); break;
                                    case 'R': m->entries["key"] = FValue::obj(new GCString("pagedown")); break;
                                    case 'S': m->entries["key"] = FValue::obj(new GCString("delete")); break;
                                    default: m->entries["key"] = FValue::obj(new GCString("unknown")); break;
                                }
                            }
                            return FValue::obj(m);
                        }
                    }
                    auto* m = new GCMap();
                    m->entries["type"] = FValue::obj(new GCString("escape"));
                    m->entries["key"] = FValue::obj(new GCString("escape"));
                    return FValue::obj(m);
                }
                auto* m = new GCMap();
                m->entries["type"] = FValue::obj(new GCString("char"));
                m->entries["key"] = FValue::obj(new GCString(std::string(1, c)));
                m->entries["code"] = FValue::integer(c);
                return FValue::obj(m);
            }
            auto* m = new GCMap();
            m->entries["type"] = FValue::obj(new GCString("none"));
            m->entries["key"] = FValue::obj(new GCString(""));
            return FValue::obj(m);
        }, "ui.read_key"));

        uiMod->entries["poll_input"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            int timeout_ms = args.size() > 0 && args[0].isNumber() ? (int)args[0].asInteger() : 100;
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);
            struct timeval tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
            return FValue::boolean(ret > 0);
        }, "ui.poll_input"));

        uiMod->entries["enter_alt_screen"] = FValue::obj(new GCNative([](const std::vector<FValue>&) -> FValue {
            std::cout << "\x1b[?1049h" << std::flush;
            return FValue::nil();
        }, "ui.enter_alt_screen"));

        uiMod->entries["exit_alt_screen"] = FValue::obj(new GCNative([](const std::vector<FValue>&) -> FValue {
            std::cout << "\x1b[?1049l" << std::flush;
            return FValue::nil();
        }, "ui.exit_alt_screen"));

        modules_["ui"] = FValue::obj(uiMod);
    }
    {
        defineSDL2Module(*this);
    }
    {
        auto* testMod = new GCMap();
        static int testPassed = 0, testFailed = 0;
        static std::string currentDescribe;

        testMod->entries["assert"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() < 1 || args.size() > 2) throw std::runtime_error("assert() expects 1 or 2 arguments");
            bool cond = args[0].isTruthy();
            std::string msg = args.size() > 1 ? args[1].toString() : "assertion failed";
            if (cond) {
                testPassed++;
                std::cout << "  [PASS] " << currentDescribe << " > " << msg << std::endl;
            } else {
                testFailed++;
                std::cout << "  [FAIL] " << currentDescribe << " > " << msg << std::endl;
            }
            return FValue::nil();
        }, "assert"));

        testMod->entries["assertEquals"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() < 2 || args.size() > 3) throw std::runtime_error("assertEquals() expects 2 or 3 arguments");
            bool eq = args[0].equals(args[1]);
            std::string msg = args.size() > 2 ? args[2].toString() : "assertEquals failed";
            if (eq) {
                testPassed++;
                std::cout << "  [PASS] " << currentDescribe << " > " << msg << std::endl;
            } else {
                testFailed++;
                std::cout << "  [FAIL] " << currentDescribe << " > " << msg << " (got " << args[0].toString() << " vs " << args[1].toString() << ")" << std::endl;
            }
            return FValue::nil();
        }, "assertEquals"));

        testMod->entries["assertNotEquals"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() < 2 || args.size() > 3) throw std::runtime_error("assertNotEquals() expects 2 or 3 arguments");
            bool neq = !args[0].equals(args[1]);
            std::string msg = args.size() > 2 ? args[2].toString() : "assertNotEquals failed";
            if (neq) {
                testPassed++;
                std::cout << "  [PASS] " << currentDescribe << " > " << msg << std::endl;
            } else {
                testFailed++;
                std::cout << "  [FAIL] " << currentDescribe << " > " << msg << std::endl;
            }
            return FValue::nil();
        }, "assertNotEquals"));

        testMod->entries["describe"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() != 1) throw std::runtime_error("describe() expects 1 argument");
            if (!args[0].isString()) throw std::runtime_error("describe() expects a string");
            if (testPassed > 0 || testFailed > 0)
                std::cout << "\nResults: " << testPassed << " passed, " << testFailed << " failed" << std::endl;
            testPassed = 0; testFailed = 0;
            currentDescribe = args[0].asString()->value;
            std::cout << "\n--- " << currentDescribe << " ---" << std::endl;
            return FValue::nil();
        }, "describe"));

        testMod->entries["results"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (testPassed > 0 || testFailed > 0)
                std::cout << "\nResults: " << testPassed << " passed, " << testFailed << " failed" << std::endl;
            return FValue::nil();
        }, "results"));

        modules_["test"] = FValue::obj(testMod);
    }
    {
        auto* fsMod = new GCMap();

        fsMod->entries["read_dir"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() != 1) throw std::runtime_error("fs.read_dir() expects 1 argument");
            std::string path = args[0].asString()->value;
            auto* arr = new GCArray();
            DIR* dir = opendir(path.c_str());
            if (!dir) return FValue::obj(arr);
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string name(entry->d_name);
                if (name == "." || name == "..") continue;
                arr->elements.push_back(FValue::obj(new GCString(name)));
            }
            closedir(dir);
            return FValue::obj(arr);
        }, "fs.read_dir"));

        fsMod->entries["is_dir"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() != 1) throw std::runtime_error("fs.is_dir() expects 1 argument");
            std::string path = args[0].asString()->value;
            struct stat st;
            if (stat(path.c_str(), &st) != 0) return FValue::boolean(false);
            return FValue::boolean(S_ISDIR(st.st_mode));
        }, "fs.is_dir"));

        fsMod->entries["exists"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() != 1) throw std::runtime_error("fs.exists() expects 1 argument");
            std::string path = args[0].asString()->value;
            struct stat st;
            return FValue::boolean(stat(path.c_str(), &st) == 0);
        }, "fs.exists"));

        fsMod->entries["read_file"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() != 1) throw std::runtime_error("fs.read_file() expects 1 argument");
            std::string path = args[0].asString()->value;
            std::ifstream file(path);
            if (!file.is_open()) return FValue::obj(new GCString(""));
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            return FValue::obj(new GCString(content));
        }, "fs.read_file"));

        fsMod->entries["write_file"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() != 2) throw std::runtime_error("fs.write_file() expects 2 arguments");
            std::string path = args[0].asString()->value;
            std::string content = args[1].asString()->value;
            std::ofstream file(path);
            if (!file.is_open()) return FValue::boolean(false);
            file << content;
            file.close();
            return FValue::boolean(true);
        }, "fs.write_file"));

        fsMod->entries["remove"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() != 1) throw std::runtime_error("fs.remove() expects 1 argument");
            std::string path = args[0].asString()->value;
            return FValue::boolean(std::remove(path.c_str()) == 0);
        }, "fs.remove"));

        fsMod->entries["rename"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() != 2) throw std::runtime_error("fs.rename() expects 2 arguments");
            std::string old_path = args[0].asString()->value;
            std::string new_path = args[1].asString()->value;
            return FValue::boolean(std::rename(old_path.c_str(), new_path.c_str()) == 0);
        }, "fs.rename"));

        fsMod->entries["create_dir"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
            if (args.size() != 1) throw std::runtime_error("fs.create_dir() expects 1 argument");
            std::string path = args[0].asString()->value;
            return FValue::boolean(mkdir(path.c_str(), 0755) == 0);
        }, "fs.create_dir"));

        modules_["fs"] = FValue::obj(fsMod);
    }
}

} // namespace forge::fvm
