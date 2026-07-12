#include "value.hpp"
#include "chunk.hpp"
#include <sstream>
#include <cmath>

ObjUpvalue::ObjUpvalue(Value* slot) : Obj(ObjType::UPVALUE), location(slot), closed(Value::nil()) {}

ObjFunction::ObjFunction() : Obj(ObjType::FUNCTION), chunkPtr(std::make_shared<Chunk>()), chunk(chunkPtr.get()) {}

ObjClosure::ObjClosure(std::shared_ptr<ObjFunction> fn)
    : Obj(ObjType::CLOSURE), function(fn) {
    upvalues.resize(fn->upvalueCount);
}

bool Value::isTruthy() const {
    switch (type) {
        case ValueType::VAL_NIL: return false;
        case ValueType::VAL_BOOL: return as.boolean;
        case ValueType::VAL_INT: return as.integer != 0;
        case ValueType::VAL_FLOAT: return as.floating != 0.0;
        case ValueType::VAL_OBJ: return true;
    }
    return false;
}

double Value::asNumber() const {
    if (type == ValueType::VAL_FLOAT) return as.floating;
    if (type == ValueType::VAL_INT) return (double)as.integer;
    throw std::runtime_error("Value is not a number");
}

long long Value::asInteger() const {
    if (type == ValueType::VAL_INT) return as.integer;
    if (type == ValueType::VAL_FLOAT) return (long long)as.floating;
    throw std::runtime_error("Value is not an integer");
}

bool Value::equals(const Value& other) const {
    if (type != other.type) {
        if (isNumber() && other.isNumber())
            return asNumber() == other.asNumber();
        return false;
    }
    switch (type) {
        case ValueType::VAL_NIL: return true;
        case ValueType::VAL_BOOL: return as.boolean == other.as.boolean;
        case ValueType::VAL_INT: return as.integer == other.as.integer;
        case ValueType::VAL_FLOAT: return as.floating == other.as.floating;
        case ValueType::VAL_OBJ: {
            if (asString() && other.asString())
                return asString()->value == other.asString()->value;
            return as.obj == other.as.obj;
        }
    }
    return false;
}

std::string Value::toString() const {
    switch (type) {
        case ValueType::VAL_NIL: return "nil";
        case ValueType::VAL_BOOL: return as.boolean ? "true" : "false";
        case ValueType::VAL_INT: return std::to_string(as.integer);
        case ValueType::VAL_FLOAT: {
            std::ostringstream ss;
            double v = as.floating;
            if (v == (long long)v && v < 1e15)
                ss << (long long)v;
            else
                ss << v;
            return ss.str();
        }
        case ValueType::VAL_OBJ: {
            if (isString()) return asString()->value;
            if (isFunction()) return "<fn " + asFunction()->name + ">";
            if (isNative()) return "<native " + asNative()->name + ">";
            if (isClosure()) return "<fn " + asClosure()->function->name + ">";
            if (isArray()) {
                std::string result = "[";
                for (size_t i = 0; i < asArray()->elements.size(); i++) {
                    if (i > 0) result += ", ";
                    result += asArray()->elements[i].toString();
                }
                result += "]";
                return result;
            }
            if (isMap()) {
                std::string result = "{";
                bool first = true;
                for (auto& [k, v] : asMap()->entries) {
                    if (!first) result += ", ";
                    first = false;
                    result += "\"" + k + "\": " + v.toString();
                }
                result += "}";
                return result;
            }
            if (isClass()) return "<class " + asClass()->name + ">";
            if (isInstance()) return "<instance of " + asInstance()->klass->name + ">";
            if (isBoundMethod()) return "<method " + asBoundMethod()->method->function->name + ">";
            return "<object>";
        }
    }
    return "unknown";
}

bool Value::isString() const { return type == ValueType::VAL_OBJ && as.obj && as.obj->type == ObjType::STRING; }
bool Value::isFunction() const { return type == ValueType::VAL_OBJ && as.obj && as.obj->type == ObjType::FUNCTION; }
bool Value::isClosure() const { return type == ValueType::VAL_OBJ && as.obj && as.obj->type == ObjType::CLOSURE; }
bool Value::isNative() const { return type == ValueType::VAL_OBJ && as.obj && as.obj->type == ObjType::NATIVE; }
bool Value::isArray() const { return type == ValueType::VAL_OBJ && as.obj && as.obj->type == ObjType::ARRAY; }
bool Value::isMap() const { return type == ValueType::VAL_OBJ && as.obj && as.obj->type == ObjType::MAP; }
bool Value::isClass() const { return type == ValueType::VAL_OBJ && as.obj && as.obj->type == ObjType::CLASS; }
bool Value::isInstance() const { return type == ValueType::VAL_OBJ && as.obj && as.obj->type == ObjType::INSTANCE; }
bool Value::isBoundMethod() const { return type == ValueType::VAL_OBJ && as.obj && as.obj->type == ObjType::BOUND_METHOD; }

ObjString* Value::asString() const { return isString() ? static_cast<ObjString*>(as.obj) : nullptr; }
ObjFunction* Value::asFunction() const { return isFunction() ? static_cast<ObjFunction*>(as.obj) : nullptr; }
ObjClosure* Value::asClosure() const { return isClosure() ? static_cast<ObjClosure*>(as.obj) : nullptr; }
ObjNative* Value::asNative() const { return isNative() ? static_cast<ObjNative*>(as.obj) : nullptr; }
ObjArray* Value::asArray() const { return isArray() ? static_cast<ObjArray*>(as.obj) : nullptr; }
ObjMap* Value::asMap() const { return isMap() ? static_cast<ObjMap*>(as.obj) : nullptr; }
ObjClass* Value::asClass() const { return isClass() ? static_cast<ObjClass*>(as.obj) : nullptr; }
ObjInstance* Value::asInstance() const { return isInstance() ? static_cast<ObjInstance*>(as.obj) : nullptr; }
ObjBoundMethod* Value::asBoundMethod() const { return isBoundMethod() ? static_cast<ObjBoundMethod*>(as.obj) : nullptr; }
ObjUpvalue* Value::asUpvalue() const { return (type == ValueType::VAL_OBJ && as.obj && as.obj->type == ObjType::UPVALUE) ? static_cast<ObjUpvalue*>(as.obj) : nullptr; }
