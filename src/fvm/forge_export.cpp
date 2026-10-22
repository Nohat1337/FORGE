#include "forge_export.hpp"
#include "runtime.hpp"

#include <cstring>
#include <string>
#include <vector>

using namespace forge::fvm;

namespace {

// One VM per shared library instance. The exported .so keeps it alive for
// the lifetime of the process that loaded it.
ForgeVM& vm() {
    static ForgeVM instance;
    return instance;
}

std::string& lastError() {
    static std::string err;
    return err;
}

std::string& sourceStore() {
    static std::string src;
    return src;
}

// The function targeted by the *_selected_* helpers. Selecting first and
// calling second keeps every ABI uniform (all-integer, all-double or
// all-pointer), which is what a variadic FFI such as `ffi.call_int` can
// pass reliably.
std::string& selectedFunction() {
    static std::string name;
    return name;
}

// Returns a thread_local buffer so the caller can read the result before
// the next call overwrites it.
const char* stash(const std::string& text) {
    static thread_local std::string buffer;
    buffer = text;
    return buffer.c_str();
}

FValue argInt(int64_t v) { return FValue::integer((long long)v); }
FValue argFloat(double v) { return FValue::floating(v); }
FValue argStr(ForgeVM& m, const char* s) {
    if (!s) s = "";
    return FValue::obj(m.gc().newObj<GCString>(std::string(s)));
}

} // namespace

extern "C" {

int forge_lib_load(const char* source) {
    if (!source) {
        lastError() = "forge_lib_load: source is null";
        return 1;
    }
    sourceStore() = source;
    lastError().clear();
    if (!vm().interpretSource(source, "exported.fge")) {
        lastError() = "forge_lib_load: failed to compile/interpret source";
        return 1;
    }
    return 0;
}

int forge_lib_run(void) {
    lastError().clear();
    if (sourceStore().empty()) {
        lastError() = "forge_lib_run: no source loaded";
        return 1;
    }
    if (!vm().interpretSource(sourceStore(), "exported.fge")) {
        lastError() = "forge_lib_run: run failed";
        return 1;
    }
    return 0;
}

namespace {

// Shared plumbing for the typed call helpers.
bool callWith(const char* fn, std::vector<FValue>& args, FValue& result) {
    lastError().clear();
    if (!fn) {
        lastError() = "function name is null";
        return false;
    }
    if (!vm().callGlobal(fn, args, result)) {
        lastError() = "'" + std::string(fn) + "' failed";
        return false;
    }
    return true;
}

int64_t asInt(const FValue& v) {
    if (v.type == FValueType::VAL_INT) return v.asInteger();
    if (v.isNumber()) return (int64_t)v.asNumber();
    if (v.isTruthy()) return 1;
    return 0;
}

double asFloat(const FValue& v) {
    if (v.isNumber()) return v.asNumber();
    if (v.type == FValueType::VAL_INT) return (double)v.asInteger();
    return 0.0;
}

} // namespace

int64_t forge_lib_call_int0(const char* fn) {
    FValue result;
    std::vector<FValue> args;
    if (!callWith(fn, args, result)) return 0;
    return asInt(result);
}

int64_t forge_lib_call_int1(const char* fn, int64_t a) {
    FValue result;
    std::vector<FValue> args{argInt(a)};
    if (!callWith(fn, args, result)) return 0;
    return asInt(result);
}

int64_t forge_lib_call_int2(const char* fn, int64_t a, int64_t b) {
    FValue result;
    std::vector<FValue> args{argInt(a), argInt(b)};
    if (!callWith(fn, args, result)) return 0;
    return asInt(result);
}

double forge_lib_call_float0(const char* fn) {
    FValue result;
    std::vector<FValue> args;
    if (!callWith(fn, args, result)) return 0.0;
    return asFloat(result);
}

double forge_lib_call_float1(const char* fn, double a) {
    FValue result;
    std::vector<FValue> args{argFloat(a)};
    if (!callWith(fn, args, result)) return 0.0;
    return asFloat(result);
}

double forge_lib_call_float2(const char* fn, double a, double b) {
    FValue result;
    std::vector<FValue> args{argFloat(a), argFloat(b)};
    if (!callWith(fn, args, result)) return 0.0;
    return asFloat(result);
}

const char* forge_lib_call_str0(const char* fn) {
    FValue result;
    std::vector<FValue> args;
    if (!callWith(fn, args, result)) return stash("");
    if (result.isString()) return stash(result.asString()->value);
    return stash(result.toString());
}

const char* forge_lib_call_str1(const char* fn, const char* a) {
    FValue result;
    std::vector<FValue> args{argStr(vm(), a)};
    if (!callWith(fn, args, result)) return stash("");
    if (result.isString()) return stash(result.asString()->value);
    return stash(result.toString());
}

int64_t forge_lib_select(const char* fn) {
    lastError().clear();
    if (!fn) {
        lastError() = "forge_lib_select: function name is null";
        return 0;
    }
    selectedFunction() = fn;
    return 1;
}

int64_t forge_lib_call_selected_int0(void) {
    return forge_lib_call_int0(selectedFunction().c_str());
}

int64_t forge_lib_call_selected_int1(int64_t a) {
    return forge_lib_call_int1(selectedFunction().c_str(), a);
}

int64_t forge_lib_call_selected_int2(int64_t a, int64_t b) {
    return forge_lib_call_int2(selectedFunction().c_str(), a, b);
}

double forge_lib_call_selected_float0(void) {
    return forge_lib_call_float0(selectedFunction().c_str());
}

double forge_lib_call_selected_float1(double a) {
    return forge_lib_call_float1(selectedFunction().c_str(), a);
}

double forge_lib_call_selected_float2(double a, double b) {
    return forge_lib_call_float2(selectedFunction().c_str(), a, b);
}

const char* forge_lib_call_selected_str0(void) {
    return forge_lib_call_str0(selectedFunction().c_str());
}

const char* forge_lib_call_selected_str1(const char* a) {
    return forge_lib_call_str1(selectedFunction().c_str(), a);
}

const char* forge_lib_last_error(void) {
    return lastError().c_str();
}

void forge_lib_free_string(const char* s) {
    (void)s; // strings are owned by the library, nothing to release
}

const char* forge_lib_version(void) {
    return "forge-fvm 1.0";
}

} // extern "C"
