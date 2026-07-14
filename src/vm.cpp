#include "vm.hpp"
#include <iostream>
#include <sstream>
#include <cmath>
#include <stdexcept>
#include <random>
#include <cctype>
#include <fstream>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#endif

VM::VM() {
    stack.reserve(STACK_MAX);
    defineBuiltins();
    defineModules();
}

void VM::reset() {
    stack.clear();
    frames.clear();
    frameCount = 0;
    tryFrames.clear();
    openUpvalues = nullptr;
}

void VM::push(const Value& val) { stack.push_back(val); }
Value VM::pop() { Value v = stack.back(); stack.pop_back(); return v; }
Value VM::peek(int distance) { return stack[stack.size() - 1 - distance]; }

void VM::runtimeError(const std::string& msg) {
    std::cerr << "Runtime Error: " << msg;
    for (int i = frameCount - 1; i >= 0; i--) {
        CallFrame& f = frames[i];
        if (!f.closure || !f.closure->function) continue;
        auto& fn = f.closure->function;
        std::cerr << "\n  " << (i == frameCount - 1 ? "in " : "  called from ");
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

std::shared_ptr<ObjUpvalue> VM::captureUpvalue(Value* slot) {
    std::shared_ptr<ObjUpvalue> prev = nullptr;
    auto curr = openUpvalues;
    while (curr && curr->location > slot) {
        prev = curr;
        curr = curr->next;
    }
    if (curr && curr->location == slot) return curr;
    auto uv = std::make_shared<ObjUpvalue>(slot);
    uv->next = curr;
    if (prev) prev->next = uv;
    else openUpvalues = uv;
    return uv;
}

void VM::closeUpvalues(Value* last) {
    while (openUpvalues && openUpvalues->location >= last) {
        openUpvalues->closed = *openUpvalues->location;
        openUpvalues->location = &openUpvalues->closed;
        openUpvalues = openUpvalues->next;
    }
}

bool VM::handleError(const std::string& msg) {
    auto errObj = std::make_shared<ObjString>(msg);
    Value errVal = Value::obj(errObj);
    if (!tryFrames.empty()) {
        auto tf = tryFrames.back();
        tryFrames.pop_back();
        while (frameCount > tf.frameCount) { frameCount--; frames.pop_back(); }
        stack.resize(tf.stackSize);
        CallFrame& cf = frames[frameCount - 1];
        cf.ip = cf.closure->function->chunk->code.data() + tf.handlerOffset;
        push(errVal);
        return true;
    }
    runtimeError(msg);
    return false;
}

void VM::defineBuiltins() {
    defineNative("print", [](const std::vector<Value>& args) -> Value {
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) std::cout << " ";
            std::cout << args[i].toString();
        }
        std::cout << "\n";
        return Value::nil();
    });

    defineNative("len", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("len() expects 1 argument");
        if (args[0].isString()) return Value::integer((long long)args[0].asString()->value.size());
        if (args[0].isArray()) return Value::integer((long long)args[0].asArray()->elements.size());
        if (args[0].isMap()) return Value::integer((long long)args[0].asMap()->entries.size());
        throw std::runtime_error("len() expects string, array, or map (got type " + std::to_string((int)args[0].type) + ")");
    });

    defineNative("push", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 2) throw std::runtime_error("push() expects 2 arguments");
        if (!args[0].isArray()) throw std::runtime_error("push() expects array");
        args[0].asArray()->elements.push_back(args[1]);
        return args[0];
    });

    defineNative("str", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("str() expects 1 argument");
        return Value::obj(std::make_shared<ObjString>(args[0].toString()));
    });

    defineNative("int", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("int() expects 1 argument");
        return Value::integer(args[0].asInteger());
    });

    defineNative("float", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("float() expects 1 argument");
        return Value::floating(args[0].asNumber());
    });

    defineNative("type", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("type() expects 1 argument");
        std::string t;
        switch (args[0].type) {
            case ValueType::VAL_NIL: t = "nil"; break;
            case ValueType::VAL_BOOL: t = "bool"; break;
            case ValueType::VAL_INT: t = "int"; break;
            case ValueType::VAL_FLOAT: t = "float"; break;
            case ValueType::VAL_OBJ:
                if (args[0].isString()) t = "string";
                else if (args[0].isArray()) t = "array";
                else if (args[0].isMap()) t = "map";
                else if (args[0].isClass()) t = "class";
                else if (args[0].isInstance()) t = "instance";
                else t = "object";
                break;
        }
        return Value::obj(std::make_shared<ObjString>(t));
    });

    defineNative("error", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("error() expects 1 argument");
        throw std::runtime_error(args[0].toString());
    });

    defineNative("keys", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("keys() expects 1 argument");
        if (!args[0].isMap()) throw std::runtime_error("keys() expects a map");
        auto arr = std::make_shared<ObjArray>();
        for (auto& [k, v] : args[0].asMap()->entries)
            arr->elements.push_back(Value::obj(std::make_shared<ObjString>(k)));
        return Value::obj(arr);
    });

    defineNative("values", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("values() expects 1 argument");
        if (!args[0].isMap()) throw std::runtime_error("values() expects a map");
        auto arr = std::make_shared<ObjArray>();
        for (auto& [k, v] : args[0].asMap()->entries)
            arr->elements.push_back(v);
        return Value::obj(arr);
    });

    defineNative("has", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 2) throw std::runtime_error("has() expects 2 arguments");
        if (!args[0].isMap()) throw std::runtime_error("has() expects a map as first argument");
        if (!args[1].isString()) throw std::runtime_error("has() expects a string as second argument");
        auto& entries = args[0].asMap()->entries;
        return Value::boolean(entries.find(args[1].asString()->value) != entries.end());
    });

    defineNative("entries", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("entries() expects 1 argument");
        if (!args[0].isMap()) throw std::runtime_error("entries() expects a map");
        auto arr = std::make_shared<ObjArray>();
        for (auto& [k, v] : args[0].asMap()->entries) {
            auto pair = std::make_shared<ObjArray>();
            pair->elements.push_back(Value::obj(std::make_shared<ObjString>(k)));
            pair->elements.push_back(v);
            arr->elements.push_back(Value::obj(pair));
        }
        return Value::obj(arr);
    });

    defineNative("clone", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("clone() expects 1 argument");
        if (args[0].isMap()) {
            auto newMap = std::make_shared<ObjMap>();
            newMap->entries = args[0].asMap()->entries;
            return Value::obj(newMap);
        }
        if (args[0].isArray()) {
            auto newArr = std::make_shared<ObjArray>();
            newArr->elements = args[0].asArray()->elements;
            return Value::obj(newArr);
        }
        throw std::runtime_error("clone() expects map or array");
    });

    defineNative("upper", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("upper() expects 1 argument");
        if (!args[0].isString()) throw std::runtime_error("upper() expects a string");
        std::string s = args[0].asString()->value;
        for (auto& c : s) c = std::toupper(c);
        return Value::obj(std::make_shared<ObjString>(s));
    });

    defineNative("lower", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("lower() expects 1 argument");
        if (!args[0].isString()) throw std::runtime_error("lower() expects a string");
        std::string s = args[0].asString()->value;
        for (auto& c : s) c = std::tolower(c);
        return Value::obj(std::make_shared<ObjString>(s));
    });

    defineNative("trim", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("trim() expects 1 argument");
        if (!args[0].isString()) throw std::runtime_error("trim() expects a string");
        std::string s = args[0].asString()->value;
        size_t start = s.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return Value::obj(std::make_shared<ObjString>(""));
        size_t end = s.find_last_not_of(" \t\n\r");
        return Value::obj(std::make_shared<ObjString>(s.substr(start, end - start + 1)));
    });

    defineNative("split", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 2) throw std::runtime_error("split() expects 2 arguments");
        if (!args[0].isString()) throw std::runtime_error("split() expects a string as first argument");
        if (!args[1].isString()) throw std::runtime_error("split() expects a string as second argument");
        std::string s = args[0].asString()->value;
        std::string delim = args[1].asString()->value;
        auto arr = std::make_shared<ObjArray>();
        if (delim.empty()) {
            for (char c : s) arr->elements.push_back(Value::obj(std::make_shared<ObjString>(std::string(1, c))));
        } else {
            size_t pos = 0;
            while ((pos = s.find(delim)) != std::string::npos) {
                arr->elements.push_back(Value::obj(std::make_shared<ObjString>(s.substr(0, pos))));
                s.erase(0, pos + delim.size());
            }
            arr->elements.push_back(Value::obj(std::make_shared<ObjString>(s)));
        }
        return Value::obj(arr);
    });

    defineNative("contains", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 2) throw std::runtime_error("contains() expects 2 arguments");
        if (!args[0].isString()) throw std::runtime_error("contains() expects a string as first argument");
        if (!args[1].isString()) throw std::runtime_error("contains() expects a string as second argument");
        return Value::boolean(args[0].asString()->value.find(args[1].asString()->value) != std::string::npos);
    });

    defineNative("replace", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 3) throw std::runtime_error("replace() expects 3 arguments");
        if (!args[0].isString()) throw std::runtime_error("replace() expects a string as first argument");
        if (!args[1].isString()) throw std::runtime_error("replace() expects a string as second argument");
        if (!args[2].isString()) throw std::runtime_error("replace() expects a string as third argument");
        std::string s = args[0].asString()->value;
        std::string old = args[1].asString()->value;
        std::string repl = args[2].asString()->value;
        size_t pos = 0;
        while ((pos = s.find(old, pos)) != std::string::npos) {
            s.replace(pos, old.size(), repl);
            pos += repl.size();
        }
        return Value::obj(std::make_shared<ObjString>(s));
    });

    defineNative("substring", [](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args.size() > 3) throw std::runtime_error("substring() expects 2 or 3 arguments");
        if (!args[0].isString()) throw std::runtime_error("substring() expects a string as first argument");
        std::string s = args[0].asString()->value;
        long long start = args[1].asInteger();
        long long end = (args.size() == 3) ? args[2].asInteger() : (long long)s.size();
        if (start < 0) start = 0;
        if (end > (long long)s.size()) end = s.size();
        if (start > end) start = end;
        return Value::obj(std::make_shared<ObjString>(s.substr(start, end - start)));
    });

    defineNative("charAt", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 2) throw std::runtime_error("charAt() expects 2 arguments");
        if (!args[0].isString()) throw std::runtime_error("charAt() expects a string as first argument");
        long long i = args[1].asInteger();
        std::string s = args[0].asString()->value;
        if (i < 0 || i >= (long long)s.size()) throw std::runtime_error("charAt() index out of bounds");
        return Value::obj(std::make_shared<ObjString>(std::string(1, s[i])));
    });

    defineNative("parseInt", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("parseInt() expects 1 argument");
        if (!args[0].isString()) throw std::runtime_error("parseInt() expects a string");
        return Value::integer(std::stoll(args[0].asString()->value));
    });

    defineNative("toFloat", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("toFloat() expects 1 argument");
        if (!args[0].isString()) throw std::runtime_error("toFloat() expects a string");
        return Value::floating(std::stod(args[0].asString()->value));
    });

    defineNative("abs", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("abs() expects 1 argument");
        if (args[0].type == ValueType::VAL_INT) return Value::integer(std::llabs(args[0].as.integer));
        return Value::floating(std::abs(args[0].asNumber()));
    });

    defineNative("min", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 2) throw std::runtime_error("min() expects 2 arguments");
        if (args[0].isNumber() && args[1].isNumber()) {
            return (args[0].asNumber() < args[1].asNumber()) ? args[0] : args[1];
        }
        throw std::runtime_error("min() expects numbers");
    });

    defineNative("max", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 2) throw std::runtime_error("max() expects 2 arguments");
        if (args[0].isNumber() && args[1].isNumber()) {
            return (args[0].asNumber() > args[1].asNumber()) ? args[0] : args[1];
        }
        throw std::runtime_error("max() expects numbers");
    });

    defineNative("sqrt", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("sqrt() expects 1 argument");
        return Value::floating(std::sqrt(args[0].asNumber()));
    });

    defineNative("pow", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 2) throw std::runtime_error("pow() expects 2 arguments");
        return Value::floating(std::pow(args[0].asNumber(), args[1].asNumber()));
    });

    defineNative("floor", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("floor() expects 1 argument");
        double v = std::floor(args[0].asNumber());
        if (v == (long long)v && v < 1e15) return Value::integer((long long)v);
        return Value::floating(v);
    });

    defineNative("ceil", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("ceil() expects 1 argument");
        double v = std::ceil(args[0].asNumber());
        if (v == (long long)v && v < 1e15) return Value::integer((long long)v);
        return Value::floating(v);
    });

    defineNative("round", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw std::runtime_error("round() expects 1 argument");
        double v = std::round(args[0].asNumber());
        if (v == (long long)v && v < 1e15) return Value::integer((long long)v);
        return Value::floating(v);
    });

    static std::mt19937 rng(std::random_device{}());
    defineNative("random", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 0) throw std::runtime_error("random() expects 0 arguments");
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return Value::floating(dist(rng));
    });

    defineNative("randomInt", [](const std::vector<Value>& args) -> Value {
        if (args.size() != 2) throw std::runtime_error("randomInt() expects 2 arguments");
        long long min = args[0].asInteger();
        long long max = args[1].asInteger();
        if (min > max) throw std::runtime_error("randomInt() min must be <= max");
        std::uniform_int_distribution<long long> dist(min, max);
        return Value::integer(dist(rng));
    });
}

void VM::defineModules() {
    {
        auto testMod = std::make_shared<ObjMap>();
        static int testPassed = 0, testFailed = 0;
        static std::string currentDescribe;

        auto assertFn = std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
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
            return Value::nil();
        }, "assert");
        testMod->entries["assert"] = Value::obj(assertFn);

        auto assertEqualsFn = std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
            if (args.size() < 2 || args.size() > 3) throw std::runtime_error("assertEquals() expects 2 or 3 arguments");
            bool eq = args[0].equals(args[1]);
            std::string msg = args.size() > 2 ? args[2].toString() : "assertEquals failed";
            if (eq) {
                testPassed++;
                std::cout << "  [PASS] " << currentDescribe << " > " << msg << std::endl;
            } else {
                testFailed++;
                std::cout << "  [FAIL] " << currentDescribe << " > " << msg << std::endl;
            }
            return Value::nil();
        }, "assertEquals");
        testMod->entries["assertEquals"] = Value::obj(assertEqualsFn);

        auto assertNotEqualsFn = std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
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
            return Value::nil();
        }, "assertNotEquals");
        testMod->entries["assertNotEquals"] = Value::obj(assertNotEqualsFn);

        auto describeFn = std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw std::runtime_error("describe() expects 1 argument");
            if (!args[0].isString()) throw std::runtime_error("describe() expects a string");
            if (testPassed > 0 || testFailed > 0) {
                std::cout << "\nResults: " << testPassed << " passed, " << testFailed << " failed" << std::endl;
            }
            testPassed = 0;
            testFailed = 0;
            currentDescribe = args[0].asString()->value;
            std::cout << "\n--- " << currentDescribe << " ---" << std::endl;
            return Value::nil();
        }, "describe");
        testMod->entries["describe"] = Value::obj(describeFn);

        auto resultsFn = std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
            if (testPassed > 0 || testFailed > 0) {
                std::cout << "\nResults: " << testPassed << " passed, " << testFailed << " failed" << std::endl;
            }
            return Value::nil();
        }, "results");
        testMod->entries["results"] = Value::obj(resultsFn);

        auto resetFn = std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
            testPassed = 0;
            testFailed = 0;
            currentDescribe = "";
            return Value::nil();
        }, "reset");
        testMod->entries["reset"] = Value::obj(resetFn);

        modules_["test"] = Value::obj(testMod);
    }
    {
        auto ioMod = std::make_shared<ObjMap>();
        ioMod->entries["write"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
            if (args.size() < 1 || args.size() > 2) throw std::runtime_error("io.write() expects 1 or 2 arguments");
            std::string s = args[0].toString();
            std::string dest = args.size() > 1 && args[1].isString() ? args[1].asString()->value : "stdout";
            if (dest == "stdout") std::cout << s;
            else if (dest == "stderr") std::cerr << s;
            return Value::nil();
        }, "io.write"));
        ioMod->entries["read"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw std::runtime_error("io.read() expects 1 argument");
            std::string path = args[0].asString()->value;
            std::ifstream file(path);
            if (!file.is_open()) throw std::runtime_error("io.read() cannot open file: " + path);
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            return Value::obj(std::make_shared<ObjString>(content));
        }, "io.read"));
        modules_["io"] = Value::obj(ioMod);
    }
    {
        auto osMod = std::make_shared<ObjMap>();
        osMod->entries["time"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
            return Value::integer((long long)std::time(nullptr));
        }, "os.time"));
        osMod->entries["execute"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw std::runtime_error("os.execute() expects 1 argument");
            std::string cmd = args[0].asString()->value;
            int result = std::system(cmd.c_str());
            return Value::integer((long long)result);
        }, "os.execute"));
        modules_["os"] = Value::obj(osMod);
    }
    {
        auto jsonMod = std::make_shared<ObjMap>();
        jsonMod->entries["stringify"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw std::runtime_error("json.stringify() expects 1 argument");
            return Value::obj(std::make_shared<ObjString>(args[0].toString()));
        }, "json.stringify"));
        jsonMod->entries["parse"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw std::runtime_error("json.parse() expects 1 argument");
            return Value::nil();
        }, "json.parse"));
        modules_["json"] = Value::obj(jsonMod);
    }
    {
        auto pathMod = std::make_shared<ObjMap>();
        pathMod->entries["join"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
            if (args.size() < 1) throw std::runtime_error("path.join() expects at least 1 argument");
            std::string result;
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) result += "/";
                result += args[i].asString()->value;
            }
            return Value::obj(std::make_shared<ObjString>(result));
        }, "path.join"));
        pathMod->entries["base"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw std::runtime_error("path.base() expects 1 argument");
            std::string p = args[0].asString()->value;
            size_t pos = p.find_last_of("/\\");
            return Value::obj(std::make_shared<ObjString>(pos != std::string::npos ? p.substr(pos + 1) : p));
        }, "path.base"));
        modules_["path"] = Value::obj(pathMod);
    }
    {
        auto sysMod = std::make_shared<ObjMap>();
        sysMod->entries["version"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
            return Value::obj(std::make_shared<ObjString>("2.0.0"));
        }, "system.version"));
        sysMod->entries["platform"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
#ifdef __linux__
            return Value::obj(std::make_shared<ObjString>("linux"));
#elif __APPLE__
            return Value::obj(std::make_shared<ObjString>("darwin"));
#else
            return Value::obj(std::make_shared<ObjString>("unknown"));
#endif
        }, "system.platform"));
        modules_["system"] = Value::obj(sysMod);
    }
    {
        static struct termios origTermios;
        static bool rawModeActive = false;

        auto uiMod = std::make_shared<ObjMap>();

        uiMod->entries["init"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>&) -> Value {
            std::cout << "\x1b[?25l" << std::flush;
            return Value::nil();
        }, "ui.init"));

        uiMod->entries["cleanup"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>&) -> Value {
            std::cout << "\x1b[?25h\x1b[0m\x1b[2J\x1b[H" << std::flush;
            if (rawModeActive) {
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios);
                rawModeActive = false;
            }
            return Value::nil();
        }, "ui.cleanup"));

        uiMod->entries["enable_raw_mode"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>&) -> Value {
            if (!isatty(STDIN_FILENO)) return Value::nil();
            tcgetattr(STDIN_FILENO, &origTermios);
            struct termios raw = origTermios;
            raw.c_lflag &= ~(ECHO | ICANON | ISIG);
            raw.c_iflag &= ~(IXON | ICRNL);
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 1;
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
            rawModeActive = true;
            return Value::nil();
        }, "ui.enable_raw_mode"));

        uiMod->entries["disable_raw_mode"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>&) -> Value {
            if (rawModeActive) {
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios);
                rawModeActive = false;
            }
            return Value::nil();
        }, "ui.disable_raw_mode"));

        uiMod->entries["clear"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
            int bg = args.size() > 0 && args[0].isNumber() ? (int)args[0].asInteger() : 40;
            std::cout << "\x1b[2J\x1b[H\x1b[48;5;" << bg << "m" << std::flush;
            return Value::nil();
        }, "ui.clear"));

        uiMod->entries["flush"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>&) -> Value {
            std::cout << std::flush;
            return Value::nil();
        }, "ui.flush"));

        uiMod->entries["draw_text"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
            if (args.size() < 3) throw std::runtime_error("ui.draw_text(x, y, text, fg, bg, bold)");
            int x = (int)args[0].asInteger();
            int y = (int)args[1].asInteger();
            std::string text = args[2].asString()->value;
            int fg = args.size() > 3 ? (int)args[3].asInteger() : 97;
            int bg = args.size() > 4 ? (int)args[4].asInteger() : 40;
            bool bold = args.size() > 5 && args[5].as.boolean;
            std::cout << "\x1b[" << y << ";" << x << "H";
            if (bold) std::cout << "\x1b[1m";
            std::cout << "\x1b[38;5;" << fg << "m\x1b[48;5;" << bg << "m" << text;
            if (bold) std::cout << "\x1b[22m";
            return Value::nil();
        }, "ui.draw_text"));

        uiMod->entries["draw_char"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
            if (args.size() < 3) throw std::runtime_error("ui.draw_char(x, y, char, fg, bg)");
            int x = (int)args[0].asInteger();
            int y = (int)args[1].asInteger();
            int ch = args[2].isString() ? (int)args[2].asString()->value[0] : (int)args[2].asInteger();
            int fg = args.size() > 3 ? (int)args[3].asInteger() : 97;
            int bg = args.size() > 4 ? (int)args[4].asInteger() : 40;
            std::cout << "\x1b[" << y << ";" << x << "H"
                      << "\x1b[38;5;" << fg << "m\x1b[48;5;" << bg << "m"
                      << (char)ch;
            return Value::nil();
        }, "ui.draw_char"));

        uiMod->entries["draw_rect"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
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
            return Value::nil();
        }, "ui.draw_rect"));

        uiMod->entries["draw_hline"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
            if (args.size() < 4) throw std::runtime_error("ui.draw_hline(x, y, w, color)");
            int x = (int)args[0].asInteger();
            int y = (int)args[1].asInteger();
            int w = (int)args[2].asInteger();
            int color = (int)args[3].asInteger();
            std::cout << "\x1b[" << y << ";" << x << "H\x1b[48;5;" << color << "m";
            for (int i = 0; i < w; i++) std::cout << "\xe2\x94\x80";
            return Value::nil();
        }, "ui.draw_hline"));

        uiMod->entries["get_size"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>&) -> Value {
            struct winsize ws;
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
                auto arr = std::make_shared<ObjArray>();
                arr->elements.push_back(Value::integer(ws.ws_col));
                arr->elements.push_back(Value::integer(ws.ws_row));
                return Value::obj(arr);
            }
            auto arr = std::make_shared<ObjArray>();
            arr->elements.push_back(Value::integer(80));
            arr->elements.push_back(Value::integer(24));
            return Value::obj(arr);
        }, "ui.get_size"));

        uiMod->entries["read_key"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>&) -> Value {
            char c = 0;
            if (read(STDIN_FILENO, &c, 1) == 1) {
                if (c == 27) {
                    char seq[2] = {0, 0};
                    read(STDIN_FILENO, &seq[0], 1);
                    read(STDIN_FILENO, &seq[1], 1);
                    if (seq[0] == '[') {
                        auto m = std::make_shared<ObjMap>();
                        m->entries["type"] = Value::obj(std::make_shared<ObjString>("arrow"));
                        switch (seq[1]) {
                            case 'A': m->entries["key"] = Value::obj(std::make_shared<ObjString>("up")); break;
                            case 'B': m->entries["key"] = Value::obj(std::make_shared<ObjString>("down")); break;
                            case 'C': m->entries["key"] = Value::obj(std::make_shared<ObjString>("right")); break;
                            case 'D': m->entries["key"] = Value::obj(std::make_shared<ObjString>("left")); break;
                            default: m->entries["key"] = Value::obj(std::make_shared<ObjString>("unknown")); break;
                        }
                        return Value::obj(m);
                    }
                    auto m = std::make_shared<ObjMap>();
                    m->entries["type"] = Value::obj(std::make_shared<ObjString>("escape"));
                    m->entries["key"] = Value::obj(std::make_shared<ObjString>("escape"));
                    return Value::obj(m);
                }
                auto m = std::make_shared<ObjMap>();
                m->entries["type"] = Value::obj(std::make_shared<ObjString>("char"));
                m->entries["key"] = Value::obj(std::make_shared<ObjString>(std::string(1, c)));
                m->entries["code"] = Value::integer(c);
                return Value::obj(m);
            }
            auto m = std::make_shared<ObjMap>();
            m->entries["type"] = Value::obj(std::make_shared<ObjString>("none"));
            m->entries["key"] = Value::obj(std::make_shared<ObjString>(""));
            return Value::obj(m);
        }, "ui.read_key"));

        uiMod->entries["poll_input"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>& args) -> Value {
            int timeout_ms = args.size() > 0 ? (int)args[0].asInteger() : 100;
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);
            struct timeval tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
            return Value::boolean(ret > 0);
        }, "ui.poll_input"));

        uiMod->entries["enter_alt_screen"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>&) -> Value {
            std::cout << "\x1b[?1049h" << std::flush;
            return Value::nil();
        }, "ui.enter_alt_screen"));

        uiMod->entries["exit_alt_screen"] = Value::obj(std::make_shared<ObjNative>([](const std::vector<Value>&) -> Value {
            std::cout << "\x1b[?1049l" << std::flush;
            return Value::nil();
        }, "ui.exit_alt_screen"));

        modules_["ui"] = Value::obj(uiMod);
    }
}

void VM::defineNative(const std::string& name, std::function<Value(const std::vector<Value>&)> fn, int arity) {
    globals[name] = Value::obj(std::make_shared<ObjNative>(fn, name));
}

bool VM::call(ObjClosure* closure, int argCount) {
    if (frameCount >= MAX_CALL_FRAMES) {
        return handleError("Stack overflow: too many nested function calls");
    }
    if (argCount != closure->function->arity) {
        return handleError("Expected " + std::to_string(closure->function->arity) +
            " arguments but got " + std::to_string(argCount));
    }
    CallFrame& frame = frames.emplace_back();
    frame.closure = closure;
    frame.ip = closure->function->chunk->code.data();
    frame.slots = stack.data() + stack.size() - argCount - 1;
    frameCount++;
    return true;
}

bool VM::callValue(const Value& callee, int argCount) {
    if (callee.isClosure()) return call(callee.asClosure(), argCount);

    if (callee.isNative()) {
        std::vector<Value> args(stack.end() - argCount, stack.end());
        stack.resize(stack.size() - argCount - 1);
        Value result = callee.asNative()->function(args);
        push(result);
        return true;
    }
    if (callee.isClass()) {
        auto klassShared = std::dynamic_pointer_cast<ObjClass>(callee._keepAlive);
        auto instance = std::make_shared<ObjInstance>(klassShared);
        stack[stack.size() - argCount - 1] = Value::obj(instance);
        auto it = klassShared->methods.find("init");
        if (it != klassShared->methods.end()) {
            // init method expects receiver + user args
            return call(it->second.closure.get(), argCount + 1);
        } else if (argCount != 0) {
            return handleError("Expected 0 arguments for constructor");
        }
        return true;
    }
    if (callee.isBoundMethod()) {
        auto* bm = callee.asBoundMethod();
        stack[stack.size() - argCount - 1] = bm->receiver;
        return call(bm->method.get(), argCount + 1);
    }
    return handleError("Cannot call " + callee.toString());
}

bool VM::interpret(std::shared_ptr<ObjFunction> function) {
    reset();
    auto closure = std::make_shared<ObjClosure>(function);
    push(Value::obj(closure));

    if (!call(closure.get(), 0)) {
        return false;
    }

    #define READ_BYTE() (*frame.ip++)
    #define READ_UINT16() (frame.ip += 2, (uint16_t)((frame.ip[-2] << 8) | frame.ip[-1]))
    #define READ_CONSTANT() (frame.closure->function->chunk->constants[READ_UINT16()])

    while (frameCount > 0) {
        CallFrame& frame = frames[frameCount - 1];

        try {
            if (hasDebugHook()) {
                auto& chunk = frame.closure->function->chunk;
                int offset = (int)(frame.ip - chunk->code.data()) - 1;
                int line = (offset >= 0 && offset < (int)chunk->lines.size())
                    ? chunk->lineAt(offset) : -1;
                std::string fname = frame.closure->function->name;
                debugHook_(line, fname, *this);
            }
            uint8_t instruction = READ_BYTE();
            switch (instruction) {
                case OP_CONSTANT: { push(READ_CONSTANT()); break; }
                case OP_NIL: push(Value::nil()); break;
                case OP_TRUE: push(Value::boolean(true)); break;
                case OP_FALSE: push(Value::boolean(false)); break;
                case OP_POP: pop(); break;
                case OP_DUP: push(peek()); break;

                case OP_DEFINE_GLOBAL: {
                    Value name = READ_CONSTANT();
                    globals[name.asString()->value] = peek();
                    pop();
                    break;
                }
                case OP_GET_GLOBAL: {
                    Value name = READ_CONSTANT();
                    auto it = globals.find(name.asString()->value);
                    if (it == globals.end()) {
                        if (!handleError("Undefined variable '" + name.asString()->value + "'")) return false;
                        break;
                    }
                    push(it->second);
                    break;
                }
                case OP_SET_GLOBAL: {
                    Value name = READ_CONSTANT();
                    if (globals.find(name.asString()->value) == globals.end()) {
                        if (!handleError("Undefined variable '" + name.asString()->value + "'")) return false;
                        break;
                    }
                    globals[name.asString()->value] = peek();
                    break;
                }
                case OP_GET_LOCAL: { uint16_t slot = READ_UINT16(); push(frame.slots[slot]); break; }
                case OP_SET_LOCAL: { uint16_t slot = READ_UINT16(); frame.slots[slot] = peek(); break; }
                case OP_GET_UPVALUE: { uint8_t idx = READ_BYTE(); push(*frame.closure->upvalues[idx]->location); break; }
                case OP_SET_UPVALUE: { uint8_t idx = READ_BYTE(); *frame.closure->upvalues[idx]->location = peek(); break; }
                case OP_CLOSE_UPVALUE: { uint16_t slot = READ_UINT16(); closeUpvalues(frame.slots + slot); break; }

                case OP_ADD: {
                    Value b = pop(); Value a = pop();
                    if (a.isNumber() && b.isNumber()) {
                        if (a.type == ValueType::VAL_INT && b.type == ValueType::VAL_INT)
                            push(Value::integer(a.as.integer + b.as.integer));
                        else push(Value::floating(a.asNumber() + b.asNumber()));
                    } else if (a.isString() && b.isString()) {
                        push(Value::obj(std::make_shared<ObjString>(a.asString()->value + b.asString()->value)));
                    } else { if (!handleError("Invalid operands for '+'")) return false; break; }
                    break;
                }
                case OP_SUBTRACT: {
                    Value b = pop(); Value a = pop();
                    if (a.isNumber() && b.isNumber()) {
                        if (a.type == ValueType::VAL_INT && b.type == ValueType::VAL_INT)
                            push(Value::integer(a.as.integer - b.as.integer));
                        else push(Value::floating(a.asNumber() - b.asNumber()));
                    } else { if (!handleError("Invalid operands for '-'")) return false; break; }
                    break;
                }
                case OP_MULTIPLY: {
                    Value b = pop(); Value a = pop();
                    if (a.isNumber() && b.isNumber()) {
                        if (a.type == ValueType::VAL_INT && b.type == ValueType::VAL_INT)
                            push(Value::integer(a.as.integer * b.as.integer));
                        else push(Value::floating(a.asNumber() * b.asNumber()));
                    } else { if (!handleError("Invalid operands for '*'")) return false; break; }
                    break;
                }
                case OP_DIVIDE: {
                    Value b = pop(); Value a = pop();
                    if (a.isNumber() && b.isNumber()) {
                        if (b.asNumber() == 0) { if (!handleError("Division by zero")) return false; break; }
                        if (a.type == ValueType::VAL_INT && b.type == ValueType::VAL_INT)
                            push(Value::integer(a.as.integer / b.as.integer));
                        else push(Value::floating(a.asNumber() / b.asNumber()));
                    } else { if (!handleError("Invalid operands for '/'")) return false; break; }
                    break;
                }
                case OP_MODULO: {
                    Value b = pop(); Value a = pop();
                    if (a.isNumber() && b.isNumber()) {
                        if (b.asNumber() == 0) { if (!handleError("Division by zero")) return false; break; }
                        if (a.type == ValueType::VAL_INT && b.type == ValueType::VAL_INT)
                            push(Value::integer(a.as.integer % b.as.integer));
                        else push(Value::floating(std::fmod(a.asNumber(), b.asNumber())));
                    } else { if (!handleError("Invalid operands for '%'")) return false; break; }
                    break;
                }
                case OP_BITWISE_AND: {
                    Value b = pop(); Value a = pop();
                    if (a.isNumber() && b.isNumber()) {
                        if (a.type == ValueType::VAL_INT && b.type == ValueType::VAL_INT)
                            push(Value::integer(a.as.integer & b.as.integer));
                        else { if (!handleError("Bitwise AND requires integer operands")) return false; break; }
                    } else { if (!handleError("Invalid operands for '&'")) return false; break; }
                    break;
                }
                case OP_BITWISE_OR: {
                    Value b = pop(); Value a = pop();
                    if (a.isNumber() && b.isNumber()) {
                        if (a.type == ValueType::VAL_INT && b.type == ValueType::VAL_INT)
                            push(Value::integer(a.as.integer | b.as.integer));
                        else { if (!handleError("Bitwise OR requires integer operands")) return false; break; }
                    } else { if (!handleError("Invalid operands for '|'")) return false; break; }
                    break;
                }
                case OP_BITWISE_XOR: {
                    Value b = pop(); Value a = pop();
                    if (a.isNumber() && b.isNumber()) {
                        if (a.type == ValueType::VAL_INT && b.type == ValueType::VAL_INT)
                            push(Value::integer(a.as.integer ^ b.as.integer));
                        else { if (!handleError("Bitwise XOR requires integer operands")) return false; break; }
                    } else { if (!handleError("Invalid operands for '^'")) return false; break; }
                    break;
                }
                case OP_NEGATE: {
                    Value a = pop();
                    if (a.isNumber()) {
                        if (a.type == ValueType::VAL_INT) push(Value::integer(-a.as.integer));
                        else push(Value::floating(-a.as.floating));
                    } else { if (!handleError("Cannot negate non-number")) return false; break; }
                    break;
                }
                case OP_NOT: push(Value::boolean(!pop().isTruthy())); break;
                case OP_EQUAL: { Value b = pop(); Value a = pop(); push(Value::boolean(a.equals(b))); break; }
                case OP_NOT_EQUAL: { Value b = pop(); Value a = pop(); push(Value::boolean(!a.equals(b))); break; }
                case OP_LESS: {
                    Value b = pop(); Value a = pop();
                    if (a.isNumber() && b.isNumber()) push(Value::boolean(a.asNumber() < b.asNumber()));
                    else if (a.isString() && b.isString()) push(Value::boolean(a.asString()->value < b.asString()->value));
                    else { if (!handleError("Invalid comparison")) return false; break; }
                    break;
                }
                case OP_LESS_EQUAL: {
                    Value b = pop(); Value a = pop();
                    if (a.isNumber() && b.isNumber()) push(Value::boolean(a.asNumber() <= b.asNumber()));
                    else if (a.isString() && b.isString()) push(Value::boolean(a.asString()->value <= b.asString()->value));
                    else { if (!handleError("Invalid comparison")) return false; break; }
                    break;
                }
                case OP_GREATER: {
                    Value b = pop(); Value a = pop();
                    if (a.isNumber() && b.isNumber()) push(Value::boolean(a.asNumber() > b.asNumber()));
                    else if (a.isString() && b.isString()) push(Value::boolean(a.asString()->value > b.asString()->value));
                    else { if (!handleError("Invalid comparison")) return false; break; }
                    break;
                }
                case OP_GREATER_EQUAL: {
                    Value b = pop(); Value a = pop();
                    if (a.isNumber() && b.isNumber()) push(Value::boolean(a.asNumber() >= b.asNumber()));
                    else if (a.isString() && b.isString()) push(Value::boolean(a.asString()->value >= b.asString()->value));
                    else { if (!handleError("Invalid comparison")) return false; break; }
                    break;
                }

                case OP_JUMP: { uint16_t offset = READ_UINT16(); frame.ip += offset; break; }
                case OP_JUMP_IF_FALSE: { uint16_t offset = READ_UINT16(); if (!peek().isTruthy()) frame.ip += offset; break; }
                case OP_JUMP_IF_TRUE: { uint16_t offset = READ_UINT16(); if (peek().isTruthy()) frame.ip += offset; break; }
                case OP_LOOP: { uint16_t offset = READ_UINT16(); frame.ip -= offset; break; }

                case OP_CALL: {
                    uint8_t argCount = READ_BYTE();
                    Value callee = peek(argCount);
                    if (frameCount >= MAX_CALL_FRAMES) {
                        if (!handleError("Stack overflow: too many nested function calls (max " + std::to_string(MAX_CALL_FRAMES) + ")")) return false;
                        break;
                    }
                    if (!callValue(callee, argCount)) { return false; }
                    break;
                }
                case OP_RETURN: {
                    Value result = pop();
                    Value* returningSlots = frames[frameCount - 1].slots;
                    closeUpvalues(returningSlots);
                    frameCount--;
                    frames.pop_back();
                    if (frameCount == 0) { push(result); return true; }
                    stack.resize((int)(returningSlots - stack.data()));
                    push(result);
                    break;
                }

                case OP_CLOSURE: {
                    Value fnVal = READ_CONSTANT();
                    auto fnShared = std::dynamic_pointer_cast<ObjFunction>(fnVal._keepAlive);
                    auto cl = std::make_shared<ObjClosure>(fnShared);
                    auto* fn = fnShared.get();
                    for (int i = 0; i < fn->upvalueCount; i++) {
                        uint8_t isLocal = READ_BYTE();
                        uint8_t idx = READ_BYTE();
                        if (isLocal) {
                            cl->upvalues[i] = captureUpvalue(frame.slots + idx);
                        } else {
                            cl->upvalues[i] = frame.closure->upvalues[idx];
                        }
                    }
                    push(Value::obj(cl));
                    break;
                }

                case OP_PRINT: { Value val = pop(); if (val.type != ValueType::VAL_NIL) std::cout << val.toString() << "\n"; break; }

                case OP_ARRAY: {
                    uint16_t count = READ_UINT16();
                    auto arr = std::make_shared<ObjArray>();
                    for (int i = count - 1; i >= 0; i--) arr->elements.push_back(peek(i));
                    for (int i = 0; i < count; i++) pop();
                    push(Value::obj(arr));
                    break;
                }
                case OP_MAP: {
                    uint16_t count = READ_UINT16();
                    auto map = std::make_shared<ObjMap>();
                    for (int i = 0; i < count; i++) {
                        Value key = peek(2 * i + 1);
                        Value val = peek(2 * i);
                        if (!key.isString()) {
                            if (!handleError("Map key must be a string")) return false;
                            break;
                        }
                        map->entries[key.asString()->value] = val;
                    }
                    for (int i = 0; i < count * 2; i++) pop();
                    push(Value::obj(map));
                    break;
                }
                case OP_INDEX: {
                    Value idx = pop(); Value obj = pop();
                    if (obj.isArray()) {
                        long long i = idx.asInteger();
                        long long len = (long long)obj.asArray()->elements.size();
                        if (i < 0 || i >= len) {
                            if (!handleError("Array index " + std::to_string(i) + " out of bounds (length " + std::to_string(len) + ")")) return false;
                            break;
                        }
                        push(obj.asArray()->elements[i]);
                    } else if (obj.isString()) {
                        long long i = idx.asInteger();
                        long long len = (long long)obj.asString()->value.size();
                        if (i < 0 || i >= len) {
                            if (!handleError("String index " + std::to_string(i) + " out of bounds (length " + std::to_string(len) + ")")) return false;
                            break;
                        }
                        push(Value::obj(std::make_shared<ObjString>(std::string(1, obj.asString()->value[i]))));
                    } else if (obj.isMap()) {
                        if (!idx.isString()) {
                            if (!handleError("Map key must be a string")) return false;
                            break;
                        }
                        auto it = obj.asMap()->entries.find(idx.asString()->value);
                        if (it == obj.asMap()->entries.end()) {
                            push(Value::nil());
                        } else {
                            push(it->second);
                        }
                    } else { if (!handleError("Cannot index into non-indexable value")) return false; break; }
                    break;
                }
                case OP_SET_INDEX: {
                    Value val = pop(); Value idx = pop(); Value obj = pop();
                    if (obj.isArray()) {
                        long long i = idx.asInteger();
                        long long len = (long long)obj.asArray()->elements.size();
                        if (i < 0 || i >= len) {
                            if (!handleError("Array index " + std::to_string(i) + " out of bounds (length " + std::to_string(len) + ")")) return false;
                            break;
                        }
                        obj.asArray()->elements[i] = val;
                        push(val);
                    } else if (obj.isMap()) {
                        if (!idx.isString()) {
                            if (!handleError("Map key must be a string")) return false;
                            break;
                        }
                        obj.asMap()->entries[idx.asString()->value] = val;
                        push(val);
                    } else { if (!handleError("Cannot set index on non-indexable value")) return false; break; }
                    break;
                }
                case OP_INDEX_LEN: {
                    Value obj = pop();
                    if (obj.isArray()) {
                        push(Value::integer((long long)obj.asArray()->elements.size()));
                    } else if (obj.isString()) {
                        push(Value::integer((long long)obj.asString()->value.size()));
                    } else if (obj.isMap()) {
                        push(Value::integer((long long)obj.asMap()->entries.size()));
                    } else {
                        std::string typeName;
                        if (obj.isNumber()) typeName = "number";
                        else if (obj.type == ValueType::VAL_NIL) typeName = "nil";
                        else if (obj.type == ValueType::VAL_BOOL) typeName = "bool";
                        else typeName = "unknown obj type";
                        if (!handleError("Cannot get length of non-indexable value (got " + typeName + ")")) return false;
                        break;
                    }
                    break;
                }

                case OP_GET_PROPERTY: {
                    Value name = READ_CONSTANT();
                    Value obj = pop();
                    if (obj.isInstance()) {
                        auto* inst = obj.asInstance();
                        auto it = inst->fields.find(name.asString()->value);
                        if (it != inst->fields.end()) { push(it->second); }
                        else {
                            auto mit = inst->klass->methods.find(name.asString()->value);
                            if (mit != inst->klass->methods.end()) {
                                push(Value::obj(std::make_shared<ObjBoundMethod>(obj, mit->second.closure)));
                            } else { if (!handleError("Undefined property '" + name.asString()->value + "'")) return false; break; }
                        }
                    } else if (obj.isClass()) {
                        auto mit = obj.asClass()->methods.find(name.asString()->value);
                        if (mit != obj.asClass()->methods.end()) {
                            push(Value::obj(std::make_shared<ObjBoundMethod>(obj, mit->second.closure)));
                        } else { if (!handleError("Undefined method '" + name.asString()->value + "'")) return false; break; }
                    } else if (obj.isMap()) {
                        auto* m = obj.asMap();
                        auto it = m->entries.find(name.asString()->value);
                        if (it != m->entries.end()) { push(it->second); }
                        else { if (!handleError("Undefined property '" + name.asString()->value + "'")) return false; break; }
                    } else { if (!handleError("Cannot access property on non-object")) return false; break; }
                    break;
                }
                case OP_SET_PROPERTY: {
                    Value name = READ_CONSTANT();
                    Value val = pop();
                    Value obj = pop();
                    if (!obj.isInstance()) { if (!handleError("Cannot set property on non-instance")) return false; break; }
                    obj.asInstance()->fields[name.asString()->value] = val;
                    push(val);
                    break;
                }

                case OP_CLASS: {
                    Value name = READ_CONSTANT();
                    push(Value::obj(std::make_shared<ObjClass>(name.asString()->value)));
                    break;
                }
                case OP_METHOD: {
                    Value name = READ_CONSTANT();
                    Value methodVal = peek();
                    Value classVal = peek(1);
                    if (!classVal.isClass()) { if (!handleError("Expected class for method")) return false; break; }
                    auto cl = std::dynamic_pointer_cast<ObjClosure>(methodVal._keepAlive);
                    classVal.asClass()->methods[name.asString()->value] = Method{cl, false};
                    pop();
                    break;
                }
                case OP_INHERIT: {
                    Value superclass = peek();
                    Value subclass = peek(1);
                    if (!superclass.isClass()) { if (!handleError("Superclass must be a class")) return false; break; }
                    subclass.asClass()->superclass = std::dynamic_pointer_cast<ObjClass>(superclass._keepAlive);
                    for (auto& [n, m] : superclass.asClass()->methods) {
                        if (subclass.asClass()->methods.find(n) == subclass.asClass()->methods.end())
                            subclass.asClass()->methods[n] = m;
                    }
                    pop();
                    break;
                }
                case OP_GET_SUPER: {
                    Value name = READ_CONSTANT();
                    Value instance = pop();
                    if (!instance.isInstance()) { if (!handleError("Expected instance")) return false; break; }
                    auto& sup = instance.asInstance()->klass->superclass;
                    if (!sup) { if (!handleError("No superclass")) return false; break; }
                    auto it = sup->methods.find(name.asString()->value);
                    if (it == sup->methods.end()) { if (!handleError("Undefined method '" + name.asString()->value + "'")) return false; break; }
                    push(Value::obj(std::make_shared<ObjBoundMethod>(instance, it->second.closure)));
                    break;
                }

                case OP_THROW: {
                    Value val = pop();
                    if (!tryFrames.empty()) {
                        auto tf = tryFrames.back();
                        tryFrames.pop_back();
                        while (frameCount > tf.frameCount) { frameCount--; frames.pop_back(); }
                        stack.resize(tf.stackSize);
                        CallFrame& cf = frames[frameCount - 1];
                        cf.ip = cf.closure->function->chunk->code.data() + tf.handlerOffset;
                        push(val);
                    } else { if (!handleError("Uncaught exception: " + val.toString())) return false; break; }
                    break;
                }
                case OP_TRY: {
                    uint16_t offset = READ_UINT16();
                    CallFrame& cf = frames[frameCount - 1];
                    int currentOffset = (int)(cf.ip - cf.closure->function->chunk->code.data());
                    tryFrames.push_back({(int)stack.size(), frameCount, currentOffset + offset});
                    break;
                }
                case OP_END_TRY: {
                    if (!tryFrames.empty()) tryFrames.pop_back();
                    break;
                }

                case OP_IMPORT: {
                    Value name = READ_CONSTANT();
                    uint8_t paramCount = READ_BYTE();
                    (void)paramCount;
                    uint8_t libIdx = READ_BYTE();
                    (void)libIdx;
                    std::string modName = name.asString()->value;
                    auto it = modules_.find(modName);
                    if (it != modules_.end()) {
                        globals[modName] = it->second;
                        push(it->second);
                    } else {
                        auto native = std::make_shared<ObjNative>([modName](const std::vector<Value>& args) -> Value {
                            throw std::runtime_error("module '" + modName + "' not found");
                        }, modName);
                        globals[modName] = Value::obj(native);
                        push(globals[modName]);
                    }
                    break;
                }

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

    #undef READ_BYTE
    #undef READ_UINT16
    #undef READ_CONSTANT
}
