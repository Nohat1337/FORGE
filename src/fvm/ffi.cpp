#include "ffi.hpp"
#include <dlfcn.h>

namespace forge::fvm {

// Store open library handles as global state
static std::unordered_map<long long, void*> ffiLibraries;
static long long nextHandle = 1;

void ForgeFFI::defineModule(ForgeVM& vm) {
    auto* ffiMod = new GCMap();

    // ffi.open(path) -> handle
    // Opens a shared library using dlopen
    ffiMod->entries["open"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("ffi.open() expects 1 argument (path)");
        if (!args[0].isString()) throw std::runtime_error("ffi.open() expects a string path");
        std::string path = args[0].asString()->value;
        void* handle = dlopen(path.c_str(), RTLD_LAZY);
        if (!handle) {
            std::string err = dlerror();
            throw std::runtime_error("ffi.open() failed: " + err);
        }
        long long h = nextHandle++;
        ffiLibraries[h] = handle;
        return FValue::integer(h);
    }, "ffi.open", 1));

    // ffi.close(handle) -> nil
    ffiMod->entries["close"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("ffi.close() expects 1 argument");
        long long h = args[0].asInteger();
        auto it = ffiLibraries.find(h);
        if (it == ffiLibraries.end()) throw std::runtime_error("ffi.close() invalid handle");
        dlclose(it->second);
        ffiLibraries.erase(it);
        return FValue::nil();
    }, "ffi.close", 1));

    // ffi.sym(handle, name) -> pointer (as integer)
    ffiMod->entries["sym"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 2) throw std::runtime_error("ffi.sym() expects 2 arguments (handle, symbol_name)");
        long long h = args[0].asInteger();
        auto it = ffiLibraries.find(h);
        if (it == ffiLibraries.end()) throw std::runtime_error("ffi.sym() invalid handle");
        std::string name = args[1].asString()->value;
        void* sym = dlsym(it->second, name.c_str());
        if (!sym) throw std::runtime_error("ffi.sym() symbol not found: " + name);
        return FValue::integer(reinterpret_cast<long long>(sym));
    }, "ffi.sym", 2));

    // ffi.call_int(ptr, args...) -> int
    // Calls a function pointer with integer arguments, returns int
    ffiMod->entries["call_int"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
        if (args.size() < 1) throw std::runtime_error("ffi.call_int() expects at least 1 argument (function_ptr)");
        typedef long long (*FnPtr)(...);
        FnPtr fn = reinterpret_cast<FnPtr>(args[0].asInteger());
        // Pass remaining args as C varargs
        // For safety, support up to 8 int args
        long long a[8] = {0};
        for (size_t i = 1; i < args.size() && i <= 8; i++) {
            a[i-1] = args[i].asInteger();
        }
        long long result = fn(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
        return FValue::integer(result);
    }, "ffi.call_int"));

    // ffi.call_float(ptr, args...) -> float
    ffiMod->entries["call_float"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
        if (args.size() < 1) throw std::runtime_error("ffi.call_float() expects at least 1 argument");
        typedef double (*FnPtr)(...);
        FnPtr fn = reinterpret_cast<FnPtr>(args[0].asInteger());
        double a[8] = {0};
        for (size_t i = 1; i < args.size() && i <= 8; i++) {
            a[i-1] = args[i].asNumber();
        }
        double result = fn(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
        return FValue::floating(result);
    }, "ffi.call_float"));

    // ffi.call_str(ptr, args...) -> string
    ffiMod->entries["call_str"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
        if (args.size() < 1) throw std::runtime_error("ffi.call_str() expects at least 1 argument");
        typedef const char* (*FnPtr)(...);
        FnPtr fn = reinterpret_cast<FnPtr>(args[0].asInteger());
        long long a[8] = {0};
        for (size_t i = 1; i < args.size() && i <= 8; i++) {
            a[i-1] = args[i].asInteger();
        }
        const char* result = fn(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
        if (!result) return FValue::nil();
        return FValue::obj(new GCString(result));
    }, "ffi.call_str"));

    // ffi.call_void(ptr, args...) -> nil
    ffiMod->entries["call_void"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
        if (args.size() < 1) throw std::runtime_error("ffi.call_void() expects at least 1 argument");
        typedef void (*FnPtr)(...);
        FnPtr fn = reinterpret_cast<FnPtr>(args[0].asInteger());
        long long a[8] = {0};
        for (size_t i = 1; i < args.size() && i <= 8; i++) {
            a[i-1] = args[i].asInteger();
        }
        fn(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
        return FValue::nil();
    }, "ffi.call_void"));

    // ffi.str_ptr(str) -> integer (pointer to C string)
    ffiMod->entries["str_ptr"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("ffi.str_ptr() expects 1 argument");
        if (!args[0].isString()) throw std::runtime_error("ffi.str_ptr() expects a string");
        // Return pointer to the internal string data
        return FValue::integer(reinterpret_cast<long long>(args[0].asString()->value.c_str()));
    }, "ffi.str_ptr", 1));

    // ffi.size_of_int() -> 4
    ffiMod->entries["sizeof_int"] = FValue::obj(new GCNative([](const std::vector<FValue>&) -> FValue {
        return FValue::integer(sizeof(int));
    }, "ffi.sizeof_int", 0));

    // ffi.size_of_long() -> 8
    ffiMod->entries["sizeof_long"] = FValue::obj(new GCNative([](const std::vector<FValue>&) -> FValue {
        return FValue::integer(sizeof(long long));
    }, "ffi.sizeof_long", 0));

    // ffi.size_of_ptr() -> 8
    ffiMod->entries["sizeof_ptr"] = FValue::obj(new GCNative([](const std::vector<FValue>&) -> FValue {
        return FValue::integer(sizeof(void*));
    }, "ffi.sizeof_ptr", 0));

    // ffi.mem_read(ptr, size) -> array of integers (bytes)
    ffiMod->entries["mem_read"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 2) throw std::runtime_error("ffi.mem_read() expects 2 arguments (ptr, size)");
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(args[0].asInteger());
        long long size = args[1].asInteger();
        auto* arr = new GCArray();
        for (long long i = 0; i < size; i++) {
            arr->elements.push_back(FValue::integer((long long)ptr[i]));
        }
        return FValue::obj(arr);
    }, "ffi.mem_read", 2));

    // ffi.mem_write(ptr, data) -> nil
    ffiMod->entries["mem_write"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 2) throw std::runtime_error("ffi.mem_write() expects 2 arguments (ptr, data_array)");
        uint8_t* ptr = reinterpret_cast<uint8_t*>(args[0].asInteger());
        if (!args[1].isArray()) throw std::runtime_error("ffi.mem_write() expects array for data");
        auto& elems = args[1].asArray()->elements;
        for (size_t i = 0; i < elems.size(); i++) {
            ptr[i] = (uint8_t)elems[i].asInteger();
        }
        return FValue::nil();
    }, "ffi.mem_write", 2));

    vm.defineModule("ffi", ffiMod);
}

void defineFFIModule(ForgeVM& vm) {
    ForgeFFI::defineModule(vm);
}

} // namespace forge::fvm
