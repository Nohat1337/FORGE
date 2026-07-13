#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cstdint>

enum class ObjType {
    STRING, FUNCTION, NATIVE, CLOSURE, UPVALUE, ARRAY, MAP,
    CLASS, INSTANCE, BOUND_METHOD
};

enum class ValueType {
    VAL_NIL, VAL_BOOL, VAL_INT, VAL_FLOAT, VAL_OBJ
};

struct Obj {
    ObjType type;
    Obj(ObjType t) : type(t) {}
    virtual ~Obj() = default;
};

struct Value {
    ValueType type = ValueType::VAL_NIL;
    union {
        bool boolean;
        long long integer;
        double floating;
        Obj* obj;
    } as = {};

    std::shared_ptr<Obj> _keepAlive;

    Value() : type(ValueType::VAL_NIL) {}

    static Value nil() { return Value(); }
    static Value boolean(bool v) { Value val; val.type = ValueType::VAL_BOOL; val.as.boolean = v; return val; }
    static Value integer(long long v) { Value val; val.type = ValueType::VAL_INT; val.as.integer = v; return val; }
    static Value floating(double v) { Value val; val.type = ValueType::VAL_FLOAT; val.as.floating = v; return val; }
    static Value obj(std::shared_ptr<Obj> o) {
        Value val; val.type = ValueType::VAL_OBJ; val.as.obj = o.get(); val._keepAlive = o; return val;
    }

    bool isTruthy() const;
    bool equals(const Value& other) const;
    std::string toString() const;
    bool isNumber() const { return type == ValueType::VAL_INT || type == ValueType::VAL_FLOAT; }
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
    struct ObjString* asString() const;
    struct ObjFunction* asFunction() const;
    struct ObjClosure* asClosure() const;
    struct ObjNative* asNative() const;
    struct ObjArray* asArray() const;
    struct ObjMap* asMap() const;
    struct ObjClass* asClass() const;
    struct ObjInstance* asInstance() const;
    struct ObjBoundMethod* asBoundMethod() const;
    struct ObjUpvalue* asUpvalue() const;
};

struct ObjString : Obj {
    std::string value;
    ObjString(const std::string& s) : Obj(ObjType::STRING), value(s) {}
};

struct ObjUpvalue : Obj {
    Value* location;
    Value closed;
    std::shared_ptr<ObjUpvalue> next;
    ObjUpvalue(Value* slot);
};

struct ObjFunction : Obj {
    std::shared_ptr<struct Chunk> chunkPtr;
    struct Chunk* chunk = nullptr;
    int arity = 0;
    int upvalueCount = 0;
    int localCount = 0;
    bool isGenerator = false;
    std::string name;
    ObjFunction();
};

struct ObjNative : Obj {
    using NativeFn = std::function<Value(const std::vector<Value>&)>;
    NativeFn function;
    std::string name;
    int arity = 0;
    ObjNative(NativeFn fn, const std::string& n) : Obj(ObjType::NATIVE), function(fn), name(n) {}
};

struct ObjClosure : Obj {
    std::shared_ptr<ObjFunction> function;
    std::vector<std::shared_ptr<ObjUpvalue>> upvalues;
    ObjClosure(std::shared_ptr<ObjFunction> fn);
};

struct ObjArray : Obj {
    std::vector<Value> elements;
    ObjArray() : Obj(ObjType::ARRAY) {}
};

struct ObjMap : Obj {
    std::unordered_map<std::string, Value> entries;
    ObjMap() : Obj(ObjType::MAP) {}
};

struct ObjClass : Obj {
    std::string name;
    std::shared_ptr<ObjClass> superclass;
    std::unordered_map<std::string, struct Method> methods;
    ObjClass(const std::string& n) : Obj(ObjType::CLASS), name(n) {}
};

struct ObjInstance : Obj {
    std::shared_ptr<ObjClass> klass;
    std::unordered_map<std::string, Value> fields;
    ObjInstance(std::shared_ptr<ObjClass> k) : Obj(ObjType::INSTANCE), klass(k) {}
};

struct ObjBoundMethod : Obj {
    Value receiver;
    std::shared_ptr<ObjClosure> method;
    ObjBoundMethod(const Value& r, std::shared_ptr<ObjClosure> m)
        : Obj(ObjType::BOUND_METHOD), receiver(r), method(m) {}
};

struct Method {
    std::shared_ptr<ObjClosure> closure;
    bool isStatic = false;
};
