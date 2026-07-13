#include "runtime.hpp"
#include "classfile.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <dlfcn.h>
#include <filesystem>
#include <iostream>

namespace forge::fvm {

// ============================================================
// ClassLoader Implementation
// ============================================================

ClassLoader* ClassLoader::bootstrapLoader = nullptr;
ClassLoader* ClassLoader::systemLoader = nullptr;

ClassRef ClassLoader::loadClass(const std::string& name, bool resolve) {
    // Check cache first
    ClassRef cached = findLoadedClass(name);
    if (cached) return cached;
    
    // Delegate to parent first (parent-first delegation)
    if (parent) {
        ClassRef parentClass = parent->loadClass(name, false);
        if (parentClass) {
            if (resolve) resolveClass(parentClass);
            return parentClass;
        }
    }
    
    // Load ourselves
    ClassRef clazz = loadFromFile(name);
    if (!clazz) return nullptr;
    
    // Cache it
    {
        std::lock_guard<std::mutex> lock(classesMutex);
        loadedClasses[name] = clazz;
    }
    
    if (resolve) resolveClass(clazz);
    return clazz;
}

ClassRef ClassLoader::findLoadedClass(const std::string& name) {
    std::lock_guard<std::mutex> lock(classesMutex);
    auto it = loadedClasses.find(name);
    return it != loadedClasses.end() ? it->second : nullptr;
}

ClassRef ClassLoader::loadFromFile(const std::string& name) {
    std::string filename = name;
    std::replace(filename.begin(), filename.end(), '/', '/');
    filename += ".fclass";
    
    for (const auto& path : classpath) {
        std::filesystem::path fullPath = path + "/" + filename;
        if (std::filesystem::exists(fullPath)) {
            std::ifstream file(fullPath, std::ios::binary);
            if (file) {
                std::vector<uint8_t> data(
                    (std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>()
                );
                return defineClass(name, data);
            }
        }
    }
    return nullptr;
}

ClassRef ClassLoader::defineClass(const std::string& name, const std::vector<uint8_t>& data) {
    ClassFile cf;
    if (!cf.load(data)) return nullptr;
    
    // Create Class object
    ClassRef clazz = std::make_shared<Class>(name, this);
    clazz->accessFlags = cf.accessFlags;
    clazz->superName = cf.getClassName(cf.superClass);
    
    // Load constant pool
    for (size_t i = 1; i < cf.constantPool.size(); i++) {
        const auto& cp = cf.constantPool[i];
        switch (cp.tag) {
            case CPTag::UTF8:
                clazz->stringConstants.push_back(cp.getUtf8());
                break;
            case CPTag::INTEGER:
                clazz->intConstants.push_back(cp.getInteger());
                break;
            case CPTag::FLOAT:
                clazz->floatConstants.push_back(cp.getFloat());
                break;
            case CPTag::LONG:
                clazz->longConstants.push_back(cp.getLong());
                break;
            case CPTag::DOUBLE:
                clazz->doubleConstants.push_back(cp.getDouble());
                break;
            case CPTag::CLASS:
                // Will be resolved later
                break;
            case CPTag::STRING:
                // Will be resolved later
                break;
        }
    }
    
    // Load fields
    for (const auto& fi : cf.fields) {
        FieldRef field = std::make_shared<Field>(fi.accessFlags, 
            cf.getUtf8(fi.nameIndex), cf.getUtf8(fi.descriptorIndex), clazz);
        clazz->fields[field->name] = field;
        clazz->fieldOrder.push_back(field);
    }
    clazz->fieldCount = clazz->fieldOrder.size();
    
    // Calculate instance size and field offsets
    size_t offset = 0;
    for (auto& field : clazz->fieldOrder) {
        field->offset = offset++;
        if ((field->accessFlags & AccessFlags::STATIC) != AccessFlags(0)) {
            // Static field
            clazz->staticFields.push_back(Value());
        }
    }
    clazz->instanceSize = offset;
    
    // Load methods
    for (const auto& mi : cf.methods) {
        MethodRef method = std::make_shared<Method>(mi.accessFlags,
            cf.getUtf8(mi.nameIndex), cf.getUtf8(mi.descriptorIndex), clazz);
        
        // Find Code attribute
        for (const auto& attr : mi.attributes) {
            if (attr.type == AttributeInfo::Type::CODE) {
                // Parse code attribute
                const uint8_t* codeData = attr.data.data();
                method->maxStack = (codeData[0] << 8) | codeData[1];
                method->maxLocals = (codeData[2] << 8) | codeData[3];
                uint32_t codeLen = (codeData[4] << 24) | (codeData[5] << 16) | (codeData[6] << 8) | codeData[7];
                method->bytecode.assign(codeData + 8, codeData + 8 + codeLen);
                
                // Parse exception table
                uint16_t excCount = (codeData[8 + codeLen] << 8) | codeData[8 + codeLen + 1];
                size_t excOffset = 8 + codeLen + 2;
                for (uint16_t i = 0; i < excCount; i++) {
                    ExceptionHandler eh;
                    eh.startPc = (codeData[excOffset] << 8) | codeData[excOffset + 1];
                    eh.endPc = (codeData[excOffset + 2] << 8) | codeData[excOffset + 3];
                    eh.handlerPc = (codeData[excOffset + 4] << 8) | codeData[excOffset + 5];
                    eh.catchType = (codeData[excOffset + 6] << 8) | codeData[excOffset + 7];
                    method->exceptionTable.push_back(eh);
                    excOffset += 8;
                }
                
                // Parse attributes (LineNumberTable, LocalVariableTable)
                // ...
                break;
            }
        }
        
        // Check for native
        method->isNative = (mi.accessFlags & AccessFlags::NATIVE) != AccessFlags(0);
        if (method->isNative) {
            // Will be linked later
        }
        
        clazz->methods[method->name] = method;
        clazz->methodOrder.push_back(method);
    }
    
    // Load attributes (SourceFile, etc.)
    for (const auto& attr : cf.attributes) {
        if (attr.type == AttributeInfo::Type::SOURCE_FILE) {
            clazz->sourceFile = cf.getUtf8((attr.data[0] << 8) | attr.data[1]);
        }
    }
    
    // Register
    {
        std::lock_guard<std::mutex> lock(classesMutex);
        loadedClasses[name] = clazz;
    }
    
    return clazz;
}

void ClassLoader::resolveClass(ClassRef clazz) {
    // Resolve superclass
    if (!clazz->superName.empty()) {
        ClassRef super = loadClass(clazz->superName);
        // Link
    }
    
    // Resolve interfaces
    for (const auto& iface : clazz->interfaces) {
        loadClass(iface);
    }
    
    // Resolve field/method references
    // ...
}

std::shared_ptr<std::istream> ClassLoader::getResourceAsStream(const std::string& name) {
    for (const auto& path : classpath) {
        std::filesystem::path fullPath = path + "/" + name;
        if (std::filesystem::exists(fullPath)) {
            return std::make_shared<std::ifstream>(fullPath, std::ios::binary);
        }
    }
    return nullptr;
}

// ============================================================
// Class Implementation
// ============================================================

FieldRef Class::findField(const std::string& name, const std::string& descriptor) {
    auto it = fields.find(name);
    if (it != fields.end() && it->second->descriptor == descriptor) {
        return it->second;
    }
    return nullptr;
}

MethodRef Class::findMethod(const std::string& name, const std::string& descriptor) {
    auto it = methods.find(name);
    if (it != methods.end() && it->second->descriptor == descriptor) {
        return it->second;
    }
    return nullptr;
}

MethodRef Class::findMethod(const std::string& name, const std::string& descriptor, bool includeSuper) {
    MethodRef m = findMethod(name, descriptor);
    if (m) return m;
    
    if (includeSuper && !superName.empty()) {
        ClassRef super = loader->loadClass(superName);
        if (super) return super->findMethod(name, descriptor, true);
    }
    return nullptr;
}

void Class::initialize(ThreadRef thread) {
    std::unique_lock<std::mutex> lock(initMutex);
    if (initState == InitState::INITIALIZED) return;
    if (initState == InitState::IN_PROGRESS) {
        // Wait for initialization to complete
        initCV.wait(lock, [this] { return initState != InitState::IN_PROGRESS; });
        return;
    }
    if (initState == InitState::ERROR) {
        throw std::runtime_error("Class initialization failed: " + name);
    }
    
    initState = InitState::IN_PROGRESS;
    lock.unlock();
    
    try {
        // Initialize superclass first
        if (!superName.empty()) {
            ClassRef super = loader->loadClass(superName);
            if (super) super->initialize(thread);
        }
        
        // Initialize interfaces
        for (const auto& iface : interfaces) {
            ClassRef ifaceClass = loader->loadClass(iface);
            if (ifaceClass) ifaceClass->initialize(thread);
        }
        
        // Run static initializer <clinit>
        MethodRef clinit = findMethod("<clinit>", "()V");
        if (clinit) {
            thread->pushFrame(std::make_unique<Frame>(clinit, thread, clinit->maxStack, clinit->maxLocals));
            // Execute frame - would need interpreter
            // For now, just mark as initialized
        }
        
        std::lock_guard<std::mutex> lock2(initMutex);
        initState = InitState::INITIALIZED;
        initCV.notify_all();
    } catch (...) {
        std::lock_guard<std::mutex> lock2(initMutex);
        initState = InitState::ERROR;
        initCV.notify_all();
        throw;
    }
}

bool Class::isAssignableFrom(ClassRef other) const {
    if (this == other.get()) return true;
    if (isInterface()) {
        // Check if other implements this interface
        for (const auto& iface : other->interfaces) {
            if (iface == name) return true;
            ClassRef ifaceClass = other->loader->loadClass(iface);
            if (ifaceClass && ifaceClass->isAssignableFrom(shared_from_this())) return true;
        }
    } else {
        // Check inheritance
        return other->isSubclassOf(shared_from_this());
    }
    return false;
}

bool Class::isSubclassOf(ClassRef other) const {
    if (!other) return false;
    if (this == other.get()) return true;
    if (!superName.empty()) {
        ClassRef super = loader->loadClass(superName);
        return super && super->isSubclassOf(other);
    }
    return false;
}

// ============================================================
// FVM Implementation
// ============================================================

FVM::FVM() {
    // Initialize bootstrap and system class loaders
    if (!ClassLoader::bootstrapLoader) {
        ClassLoader::bootstrapLoader = new ClassLoader(nullptr);
        ClassLoader::bootstrapLoader->classpath = {"/usr/lib/forge/classes", "./classes"};
    }
    if (!ClassLoader::systemLoader) {
        ClassLoader::systemLoader = new ClassLoader(ClassLoader::bootstrapLoader);
        ClassLoader::systemLoader->classpath = {"./classes", "./lib"};
    }
}

FVM::~FVM() {
    shutdown();
}

bool FVM::startup(const std::vector<std::string>& args) {
    running = true;
    
    // Set system properties
    systemProperties["forge.version"] = "1.0";
    systemProperties["forge.home"] = std::filesystem::current_path().string();
    systemProperties["user.dir"] = std::filesystem::current_path().string();
    systemProperties["user.name"] = getenv("USER") ? getenv("USER") : "unknown";
    systemProperties["os.name"] = ui::native::PLATFORM;
    systemProperties["os.arch"] = "x86_64";
    
    // Start GC thread
    gcThread = std::thread([this]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            gcMinor();
        }
    });
    
    // Start JIT thread
    jitThread = std::thread([this]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            // JIT compilation queue
        }
    });
    
    uiToolkit = std::make_unique<UIToolkit>();
    
    return true;
}

void FVM::shutdown() {
    running = false;
    
    if (gcThread.joinable()) gcThread.join();
    if (jitThread.joinable()) jitThread.join();
    
    // Shutdown UI
    ui::native::shutdownPlatformUI();
    
    // Terminate all threads
    {
        std::lock_guard<std::mutex> lock(threadsMutex);
        for (auto& t : allThreads) {
            t->state = Thread::State::TERMINATED;
            t->unpark();
        }
    }
}

int FVM::executeMain(ClassRef mainClass, const std::vector<std::string>& args) {
    // Find main method
    MethodRef mainMethod = mainClass->findMethod("main", "([Ljava/lang/String;)V");
    if (!mainMethod) {
        std::cerr << "Error: Main method not found in " << mainClass->name << std::endl;
        return 1;
    }
    
    // Create main thread
    ThreadRef mainThread = createThread("main", mainMethod, {});
    
    // Create String array for args
    ClassRef stringClass = getSystemLoader()->loadClass("java/lang/String");
    ObjectRef stringArray = newArray(stringClass, args.size());
    for (size_t i = 0; i < args.size(); i++) {
        stringArray->arraySet(i, Value(internString(args[i]).get()));
    }
    
    // Push args onto stack
    mainThread->topFrame()->locals[0] = Value(stringArray.get());
    
    // Run
    while (mainThread->state != Thread::State::TERMINATED) {
        // Execute bytecode...
    }
    
    return 0;
}

ThreadRef FVM::createThread(const std::string& name, MethodRef entryPoint, const std::vector<Value>& args) {
    static std::atomic<size_t> threadIdCounter{1};
    size_t id = threadIdCounter++;
    
    ThreadRef thread = std::make_shared<Thread>(id, name);
    thread->classLoader = ClassLoader::systemLoader;
    
    // Push initial frame
    auto frame = std::make_unique<Frame>(entryPoint, thread, entryPoint->maxStack, entryPoint->maxLocals);
    for (size_t i = 0; i < args.size(); i++) {
        frame->locals[i] = args[i];
    }
    thread->pushFrame(std::move(frame));
    
    {
        std::lock_guard<std::mutex> lock(threadsMutex);
        allThreads.push_back(thread);
    }
    
    thread->state = Thread::State::RUNNABLE;
    thread->nativeThread = std::thread([this, thread]() {
        // Thread entry point
        while (thread->state != Thread::State::TERMINATED) {
            // Execute bytecode
            executeThread(thread);
        }
    });
    
    return thread;
}

ThreadRef FVM::currentThread() {
    static thread_local ThreadRef current;
    return current;
}

ObjectRef FVM::newObject(ClassRef clazz) {
    // Ensure class is initialized
    // ...
    
    ObjectRef obj = std::make_shared<Object>(clazz);
    // Initialize fields to default values
    for (auto& field : obj->fields) {
        field = Value(); // zero/null
    }
    return obj;
}

ObjectRef FVM::newArray(ClassRef elementType, size_t length) {
    std::string arrayName = "[" + elementType->name;
    ClassRef arrayClass = getSystemLoader()->loadClass(arrayName);
    if (!arrayClass) {
        // Create array class
        arrayClass = std::make_shared<Class>(arrayName, getSystemLoader());
        arrayClass->accessFlags = AccessFlags::PUBLIC | AccessFlags::FINAL;
        arrayClass->isArray = true;
        arrayClass->fieldCount = 1; // length field
        getSystemLoader()->loadedClasses[arrayName] = arrayClass;
    }
    
    ObjectRef arr = std::make_shared<Object>(arrayClass);
    arr->fields.resize(length + 1); // +1 for length
    arr->fields[0] = Value(static_cast<int32_t>(length)); // length at index 0
    return arr;
}

ObjectRef FVM::newMultiArray(ClassRef clazz, const std::vector<size_t>& dimensions) {
    if (dimensions.empty()) return nullptr;
    if (dimensions.size() == 1) {
        return newArray(clazz, dimensions[0]);
    }
    
    ClassRef elementType = clazz;
    // Recursively create nested arrays
    // ...
    return nullptr;
}

ObjectRef FVM::boxInt(int32_t v) {
    ClassRef integerClass = getSystemLoader()->loadClass("java/lang/Integer");
    ObjectRef obj = newObject(integerClass);
    obj->fields[0] = Value(v); // value field
    return obj;
}

ObjectRef FVM::boxLong(int64_t v) {
    ClassRef longClass = getSystemLoader()->loadClass("java/lang/Long");
    ObjectRef obj = newObject(longClass);
    obj->fields[0] = Value(v);
    return obj;
}

ObjectRef FVM::boxFloat(float v) {
    ClassRef floatClass = getSystemLoader()->loadClass("java/lang/Float");
    ObjectRef obj = newObject(floatClass);
    obj->fields[0] = Value(v);
    return obj;
}

ObjectRef FVM::boxDouble(double v) {
    ClassRef doubleClass = getSystemLoader()->loadClass("java/lang/Double");
    ObjectRef obj = newObject(doubleClass);
    obj->fields[0] = Value(v);
    return obj;
}

ObjectRef FVM::boxBoolean(bool v) {
    ClassRef boolClass = getSystemLoader()->loadClass("java/lang/Boolean");
    ObjectRef obj = newObject(boolClass);
    obj->fields[0] = Value(v ? 1 : 0);
    return obj;
}

ObjectRef FVM::boxChar(uint16_t v) {
    ClassRef charClass = getSystemLoader()->loadClass("java/lang/Character");
    ObjectRef obj = newObject(charClass);
    obj->fields[0] = Value(static_cast<int32_t>(v));
    return obj;
}

ObjectRef FVM::internString(const std::string& str) {
    static std::unordered_map<std::string, ObjectRef> stringPool;
    static std::mutex poolMutex;
    
    std::lock_guard<std::mutex> lock(poolMutex);
    auto it = stringPool.find(str);
    if (it != stringPool.end()) return it->second;
    
    ClassRef stringClass = getSystemLoader()->loadClass("java/lang/String");
    ObjectRef obj = newObject(stringClass);
    // Store string data in object
    // For simplicity, store in a field
    // In real JVM, String would have a char[] field
    stringPool[str] = obj;
    return obj;
}

FVM::Stats FVM::getStats() const {
    Stats s;
    s.classesLoaded = 0;
    for (auto& loader : {ClassLoader::bootstrapLoader, ClassLoader::systemLoader}) {
        if (loader) {
            std::lock_guard<std::mutex> lock(loader->classesMutex);
            s.classesLoaded += loader->loadedClasses.size();
        }
    }
    return s;
}

// ============================================================
// Thread Implementation
// ============================================================

void Thread::pushFrame(std::unique_ptr<Frame> frame) {
    if (currentFrame) {
        frame->caller = currentFrame;
    }
    currentFrame = frame.get();
    frames.push_back(std::move(frame));
}

void Thread::popFrame() {
    if (!frames.empty()) {
        frames.pop_back();
        currentFrame = frames.empty() ? nullptr : frames.back().get();
    }
}

std::string Thread::getStackTrace() const {
    std::ostringstream oss;
    oss << "Thread: " << name << " (id=" << threadId << ")\n";
    for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
        const Frame* f = it->get();
        oss << "  at " << f->method->declaringClass->name 
            << "." << f->method->name 
            << f->method->descriptor 
            << " (line " << f->getLineNumber() << ")\n";
    }
    return oss.str();
}

void Thread::park() {
    std::unique_lock<std::mutex> lock(parkMutex);
    parked = true;
    // parkCV.wait(lock, [this] { return !parked; });
}

void Thread::unpark() {
    std::lock_guard<std::mutex> lock(parkMutex);
    parked = false;
    // parkCV.notify_one();
}

// ============================================================
// Interpreter (Bytecode Execution)
// ============================================================

void FVM::executeThread(ThreadRef thread) {
    while (thread->state != Thread::State::TERMINATED && thread->currentFrame) {
        Frame* frame = thread->currentFrame;
        if (frame->pc >= frame->method->bytecode.size()) {
            // Frame complete
            thread->popFrame();
            continue;
        }
        
        Opcode opcode = static_cast<Opcode>(frame->method->bytecode[frame->pc++]);
        
        try {
            executeOpcode(thread, frame, opcode);
        } catch (const std::exception& e) {
            // Handle exception
            thread->pendingException = std::make_shared<Object>(
                getSystemLoader()->loadClass("java/lang/RuntimeException")
            );
            // Unwind stack looking for catch block
        }
    }
}

void FVM::executeOpcode(ThreadRef thread, Frame* frame, Opcode opcode) {
    switch (opcode) {
        // Constants
        case Opcode::NOP: break;
        case Opcode::ACONST_NULL: frame->push(Value(nullptr)); break;
        case Opcode::ICONST_M1: frame->push(Value(-1)); break;
        case Opcode::ICONST_0: frame->push(Value(0)); break;
        case Opcode::ICONST_1: frame->push(Value(1)); break;
        case Opcode::ICONST_2: frame->push(Value(2)); break;
        case Opcode::ICONST_3: frame->push(Value(3)); break;
        case Opcode::ICONST_4: frame->push(Value(4)); break;
        case Opcode::ICONST_5: frame->push(Value(5)); break;
        case Opcode::LCONST_0: frame->push(Value(int64_t(0))); break;
        case Opcode::LCONST_1: frame->push(Value(int64_t(1))); break;
        case Opcode::FCONST_0: frame->push(Value(0.0f)); break;
        case Opcode::FCONST_1: frame->push(Value(1.0f)); break;
        case Opcode::FCONST_2: frame->push(Value(2.0f)); break;
        case Opcode::DCONST_0: frame->push(Value(0.0)); break;
        case Opcode::DCONST_1: frame->push(Value(1.0)); break;
        case Opcode::BIPUSH: {
            int8_t val = static_cast<int8_t>(frame->nextByte());
            frame->push(Value(static_cast<int32_t>(val)));
            break;
        }
        case Opcode::SIPUSH: {
            int16_t val = static_cast<int16_t>(frame->nextShort());
            frame->push(Value(static_cast<int32_t>(val)));
            break;
        }
        case Opcode::LDC: {
            uint8_t index = frame->nextByte();
            // Load from constant pool
            break;
        }
        case Opcode::LDC_W: {
            uint16_t index = frame->nextShort();
            break;
        }
        case Opcode::LDC2_W: {
            uint16_t index = frame->nextShort();
            break;
        }
        
        // Load/Store
        case Opcode::ILOAD: {
            uint8_t idx = frame->nextByte();
            frame->push(frame->local(idx));
            break;
        }
        case Opcode::ILOAD_0: frame->push(frame->local(0)); break;
        case Opcode::ILOAD_1: frame->push(frame->local(1)); break;
        case Opcode::ILOAD_2: frame->push(frame->local(2)); break;
        case Opcode::ILOAD_3: frame->push(frame->local(3)); break;
        case Opcode::ISTORE: {
            uint8_t idx = frame->nextByte();
            frame->local(idx) = frame->pop();
            break;
        }
        case Opcode::ISTORE_0: frame->local(0) = frame->pop(); break;
        case Opcode::ISTORE_1: frame->local(1) = frame->pop(); break;
        case Opcode::ISTORE_2: frame->local(2) = frame->pop(); break;
        case Opcode::ISTORE_3: frame->local(3) = frame->pop(); break;
        
        // Stack
        case Opcode::POP: frame->pop(); break;
        case Opcode::POP2: frame->pop(); frame->pop(); break;
        case Opcode::DUP: {
            Value v = frame->top();
            frame->push(v);
            break;
        }
        case Opcode::SWAP: {
            Value v1 = frame->pop();
            Value v2 = frame->pop();
            frame->push(v1);
            frame->push(v2);
            break;
        }
        
        // Arithmetic
        case Opcode::IADD: {
            int32_t v2 = frame->pop().intVal;
            int32_t v1 = frame->pop().intVal;
            frame->push(Value(v1 + v2));
            break;
        }
        case Opcode::ISUB: {
            int32_t v2 = frame->pop().intVal;
            int32_t v1 = frame->pop().intVal;
            frame->push(Value(v1 - v2));
            break;
        }
        case Opcode::IMUL: {
            int32_t v2 = frame->pop().intVal;
            int32_t v1 = frame->pop().intVal;
            frame->push(Value(v1 * v2));
            break;
        }
        case Opcode::IDIV: {
            int32_t v2 = frame->pop().intVal;
            int32_t v1 = frame->pop().intVal;
            if (v2 == 0) throw std::runtime_error("Division by zero");
            frame->push(Value(v1 / v2));
            break;
        }
        case Opcode::IREM: {
            int32_t v2 = frame->pop().intVal;
            int32_t v1 = frame->pop().intVal;
            frame->push(Value(v1 % v2));
            break;
        }
        case Opcode::INEG: {
            int32_t v = frame->pop().intVal;
            frame->push(Value(-v));
            break;
        }
        case Opcode::ISHL: {
            int32_t v2 = frame->pop().intVal;
            int32_t v1 = frame->pop().intVal;
            frame->push(Value(v1 << (v2 & 0x1F)));
            break;
        }
        case Opcode::ISHR: {
            int32_t v2 = frame->pop().intVal;
            int32_t v1 = frame->pop().intVal;
            frame->push(Value(v1 >> (v2 & 0x1F)));
            break;
        }
        case Opcode::IUSHR: {
            int32_t v2 = frame->pop().intVal;
            int32_t v1 = frame->pop().intVal;
            frame->push(Value(static_cast<uint32_t>(v1) >> (v2 & 0x1F)));
            break;
        }
        case Opcode::IAND: {
            int32_t v2 = frame->pop().intVal;
            int32_t v1 = frame->pop().intVal;
            frame->push(Value(v1 & v2));
            break;
        }
        case Opcode::IOR: {
            int32_t v2 = frame->pop().intVal;
            int32_t v1 = frame->pop().intVal;
            frame->push(Value(v1 | v2));
            break;
        }
        case Opcode::IXOR: {
            int32_t v2 = frame->pop().intVal;
            int32_t v1 = frame->pop().intVal;
            frame->push(Value(v1 ^ v2));
            break;
        }
        case Opcode::IINC: {
            uint8_t idx = frame->nextByte();
            int32_t const_val = static_cast<int8_t>(frame->nextByte());
            frame->local(idx).intVal += const_val;
            break;
        }
        case Opcode::I2L: frame->push(Value(static_cast<int64_t>(frame->pop().intVal))); break;
        case Opcode::I2F: frame->push(Value(static_cast<float>(frame->pop().intVal))); break;
        case Opcode::I2D: frame->push(Value(static_cast<double>(frame->pop().intVal))); break;
        case Opcode::L2I: frame->push(Value(static_cast<int32_t>(frame->pop().longVal))); break;
        case Opcode::F2I: frame->push(Value(static_cast<int32_t>(frame->pop().floatVal))); break;
        case Opcode::D2I: frame->push(Value(static_cast<int32_t>(frame->pop().doubleVal))); break;
        
        // Comparison
        case Opcode::LCMP: {
            int64_t v2 = frame->pop().longVal;
            int64_t v1 = frame->pop().longVal;
            frame->push(Value(v1 < v2 ? -1 : (v1 > v2 ? 1 : 0)));
            break;
        }
        case Opcode::FCMPL:
        case Opcode::FCMPG: {
            float v2 = frame->pop().floatVal;
            float v1 = frame->pop().floatVal;
            int cmp = (v1 < v2) ? -1 : (v1 > v2 ? 1 : (v1 == v2 ? 0 : (opcode == Opcode::FCMPL ? -1 : 1)));
            frame->push(Value(cmp));
            break;
        }
        
        // Control flow
        case Opcode::IFEQ: {
            int32_t v = frame->pop().intVal;
            int16_t offset = static_cast<int16_t>(frame->nextShort());
            if (v == 0) frame->pc = frame->pc - 3 + offset;
            break;
        }
        case Opcode::IFNE: {
            int32_t v = frame->pop().intVal;
            int16_t offset = static_cast<int16_t>(frame->nextShort());
            if (v != 0) frame->pc = frame->pc - 3 + offset;
            break;
        }
        case Opcode::IFLT: {
            int32_t v = frame->pop().intVal;
            int16_t offset = static_cast<int16_t>(frame->nextShort());
            if (v < 0) frame->pc = frame->pc - 3 + offset;
            break;
        }
        case Opcode::IFGE: {
            int32_t v = frame->pop().intVal;
            int16_t offset = static_cast<int16_t>(frame->nextShort());
            if (v >= 0) frame->pc = frame->pc - 3 + offset;
            break;
        }
        case Opcode::IFGT: {
            int32_t v = frame->pop().intVal;
            int16_t offset = static_cast<int16_t>(frame->nextShort());
            if (v > 0) frame->pc = frame->pc - 3 + offset;
            break;
        }
        case Opcode::IFLE: {
            int32_t v = frame->pop().intVal;
            int16_t offset = static_cast<int16_t>(frame->nextShort());
            if (v <= 0) frame->pc = frame->pc - 3 + offset;
            break;
        }
        case Opcode::IF_ICMPEQ: {
            int32_t v2 = frame->pop().intVal;
            int32_t v1 = frame->pop().intVal;
            int16_t offset = static_cast<int16_t>(frame->nextShort());
            if (v1 == v2) frame->pc = frame->pc - 3 + offset;
            break;
        }
        case Opcode::IF_ICMPNE: {
            int32_t v2 = frame->pop().intVal;
            int32_t v1 = frame->pop().intVal;
            int16_t offset = static_cast<int16_t>(frame->nextShort());
            if (v1 != v2) frame->pc = frame->pc - 3 + offset;
            break;
        }
        case Opcode::GOTO: {
            int16_t offset = static_cast<int16_t>(frame->nextShort());
            frame->pc = frame->pc - 3 + offset;
            break;
        }
        
        // Method invocation
        case Opcode::INVOKEVIRTUAL:
        case Opcode::INVOKESPECIAL:
        case Opcode::INVOKESTATIC:
        case Opcode::INVOKEINTERFACE: {
            uint16_t index = frame->nextShort();
            // Resolve method reference and invoke
            // ...
            break;
        }
        case Opcode::INVOKEDYNAMIC: {
            uint16_t index = frame->nextShort();
            frame->pc += 2; // Skip extra bytes
            break;
        }
        
        // Return
        case Opcode::IRETURN: {
            Value result = frame->pop();
            thread->popFrame();
            if (thread->currentFrame) {
                thread->currentFrame->push(result);
            }
            break;
        }
        case Opcode::RETURN: {
            thread->popFrame();
            break;
        }
        
        // Object operations
        case Opcode::NEW: {
            uint16_t index = frame->nextShort();
            // Create new object
            ClassRef clazz = frame->method->declaringClass->getClassLoader()->loadClass(
                frame->method->declaringClass->stringConstants[index]
            );
            ObjectRef obj = std::make_shared<Object>(clazz);
            // Initialize fields to default values
            for (auto& field : obj->fields) {
                field = Value(); // zero/null
            }
            frame->push(Value(obj.get()));
            break;
        }
        case Opcode::NEWARRAY: {
            uint8_t atype = frame->nextByte();
            int32_t count = frame->pop().intVal;
            // Create primitive array
            ClassRef arrayClass = frame->method->declaringClass->getClassLoader()->loadClass("[" + getArrayTypeChar(atype));
            ObjectRef arr = std::make_shared<Object>(frame->method->declaringClass->getClassLoader()->loadClass("[" + getArrayTypeChar(atype)));
            arr->fields.resize(count + 1);
            arr->fields[0] = Value(static_cast<int32_t>(count)); // length
            frame->push(Value(arr.get()));
            break;
        }
        case Opcode::ANEWARRAY: {
            uint16_t index = frame->nextShort();
            int32_t count = frame->pop().intVal;
            // Create array of references
            break;
        }
        case Opcode::ARRAYLENGTH: {
            Value v = frame->pop();
            // ObjectRef arr = std::dynamic_pointer_cast<Object>(v.refVal);
            // frame->push(Value(static_cast<int32_t>(arr->arrayLength())));
            frame->push(Value(0)); // placeholder
            break;
        }
        case Opcode::CHECKCAST: {
            uint16_t index = frame->nextShort();
            // Type check
            break;
        }
        case Opcode::INSTANCEOF: {
            uint16_t index = frame->nextShort();
            frame->pop(); // pop object ref
            frame->push(Value(0)); // or 1
            break;
        }
        
        // Field access
        case Opcode::GETFIELD: {
            uint16_t index = frame->nextShort();
            Value objRef = frame->pop();
            if (!objRef.isObject()) {
                throw std::runtime_error("GETFIELD: expected object reference");
            }
            ObjectRef obj = std::dynamic_pointer_cast<Object>(objRef.refVal);
            if (!obj) throw std::runtime_error("GETFIELD: not an object");
            // Get field index from constant pool
            uint16_t fieldIndex = frame->method->declaringClass->getFieldIndex(index);
            if (fieldIndex >= objRef->fields.size()) {
                throw std::runtime_error("Invalid field index");
            }
            frame->push(obj->fields[fieldIndex]);
            break;
        }
        case Opcode::PUTFIELD: {
            uint16_t index = frame->nextShort();
            Value val = frame->pop();
            Value objRef = frame->pop();
            if (!objRef.isObject()) throw std::runtime_error("PUTFIELD: expected object reference");
            ObjectRef obj = std::dynamic_pointer_cast<Object>(objRef.refVal);
            // Get field index from constant pool
            uint16_t fieldIndex = frame->method->declaringClass->getFieldIndex(index);
            if (fieldIndex >= objRef->fields.size()) {
                throw std::runtime_error("Invalid field index");
            }
            obj->fields[fieldIndex] = val;
            break;
        }
        case Opcode::GETSTATIC: {
            uint16_t index = frame->nextShort();
            // Get static field from class
            // For now, push a default value
            frame->push(Value());
            break;
        }
        case Opcode::PUTSTATIC: {
            uint16_t index = frame->nextShort();
            Value val = frame->pop();
            // Set static field
            break;
        }
        
        // Monitor
        case Opcode::MONITORENTER: {
            frame->pop(); // pop object ref
            // obj->monitorEnter(thread);
            break;
        }
        case Opcode::MONITOREXIT: {
            frame->pop(); // pop object ref
            // obj->monitorExit(thread);
            break;
        }
        
        // Throw
        case Opcode::ATHROW: {
            frame->pop(); // pop exception object
            // thread->pendingException = exception;
            break;
        }
        
        // Forge extensions
        case Opcode::FORGE_INVOKE_NATIVE: {
            uint16_t index = frame->nextShort();
            // Call native function
            break;
        }
        case Opcode::FORGE_YIELD: {
            // Generator yield
            break;
        }
        case Opcode::FORGE_AWAIT: {
            // Async await
            break;
        }
        case Opcode::FORGE_TRY: {
            // Try block start
            break;
        }
        case Opcode::FORGE_CATCH: {
            // Catch handler
            break;
        }
        case Opcode::FORGE_MATCH: {
            // Pattern matching
            break;
        }
        
        default:
            std::cerr << "Unimplemented opcode: 0x" << std::hex << static_cast<int>(opcode) << std::endl;
            throw std::runtime_error("Unimplemented opcode");
    }
}

// ============================================================
// Garbage Collection
// ============================================================

void FVM::gc() {
    // Mark-and-sweep
    // 1. Mark roots (thread stacks, static fields, JNI globals)
    // 2. Trace object graph
    // 3. Sweep unreachable objects
}

void FVM::gcMinor() {
    // Generational young generation collection
}

void FVM::gcMajor() {
    // Full heap collection
}

// ============================================================
// JNI/FFI Support
// ============================================================

extern "C" {
    // JNI-like interface
    typedef struct JNIEnv_* JNIEnv;
    typedef struct JavaVM_* JavaVM;
    
    // JNI functions would be implemented here
    // jint JNI_OnLoad(JavaVM* vm, void* reserved);
    // void JNI_OnUnload(JavaVM* vm, void* reserved);
}

} // namespace forge::fvm