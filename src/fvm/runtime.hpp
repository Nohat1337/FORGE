#pragma once

#include "classfile.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>

namespace forge::fvm {

// ============================================================
// FVM Runtime - JVM-like VM with UI support
// ============================================================

// Forward declarations
class ClassLoader;
class Thread;
class Frame;
class Object;
class Class;
class Method;
class Field;

// Reference types
using ObjectRef = std::shared_ptr<Object>;
using ClassRef = std::shared_ptr<Class>;
using MethodRef = std::shared_ptr<Method>;
using FieldRef = std::shared_ptr<Field>;
using ThreadRef = std::shared_ptr<Thread>;

// ============================================================
// Class Metadata (loaded from .fclass)
// ============================================================

class Field {
public:
    AccessFlags accessFlags;
    std::string name;
    std::string descriptor;
    std::string signature;
    ClassRef declaringClass;
    size_t offset; // For instance fields
    Field(AccessFlags flags, std::string n, std::string desc, ClassRef dc)
        : accessFlags(flags), name(std::move(n)), descriptor(std::move(desc)), declaringClass(dc), offset(0) {}
};

class Method {
public:
    AccessFlags accessFlags;
    std::string name;
    std::string descriptor;
    std::string signature;
    ClassRef declaringClass;
    
    // Bytecode
    std::vector<uint8_t> bytecode;
    uint16_t maxStack = 0;
    uint16_t maxLocals = 0;
    std::vector<ExceptionHandler> exceptionTable;
    
    // Line number table
    struct LineNumber { uint16_t startPc; uint16_t lineNumber; };
    std::vector<LineNumber> lineNumberTable;
    
    // Local variable table
    struct LocalVar { uint16_t startPc; uint16_t length; uint16_t nameIndex; uint16_t descriptorIndex; uint16_t index; };
    std::vector<LocalVar> localVariableTable;
    
    // Native method pointer
    using NativeFunc = Value(*)(ThreadRef, const std::vector<Value>&);
    NativeFunc nativeFunc = nullptr;
    bool isNative = false;
    bool isConstructor = false;
    bool isStatic = false;
    bool isSynchronized = false;
    
    Method(AccessFlags flags, std::string n, std::string desc, ClassRef dc)
        : accessFlags(flags), name(std::move(n)), descriptor(std::move(desc)), declaringClass(dc) {
        isStatic = (flags & AccessFlags::STATIC) != AccessFlags(0);
        isNative = (flags & AccessFlags::NATIVE) != AccessFlags(0);
        isSynchronized = (flags & AccessFlags::SYNCHRONIZED) != AccessFlags(0);
        isConstructor = name == "<init>";
    }
};

class Class {
public:
    std::string name;           // "java/lang/Object"
    std::string superName;      // "java/lang/Object" or empty
    std::vector<std::string> interfaces;
    AccessFlags accessFlags;
    
    // Constant pool (resolved)
    std::vector<std::string> stringConstants;
    std::vector<int32_t> intConstants;
    std::vector<int64_t> longConstants;
    std::vector<float> floatConstants;
    std::vector<double> doubleConstants;
    std::vector<ClassRef> classConstants;
    
    // Members
    std::unordered_map<std::string, FieldRef> fields;
    std::unordered_map<std::string, MethodRef> methods;
    std::vector<FieldRef> fieldOrder;
    std::vector<MethodRef> methodOrder;
    
    // Static fields
    std::vector<Value> staticFields;
    
    // Class loader that loaded this class
    ClassLoader* loader = nullptr;
    
    // Initialization state
    enum class InitState { NOT_STARTED, IN_PROGRESS, INITIALIZED, ERROR };
    std::atomic<InitState> initState{InitState::NOT_STARTED};
    std::mutex initMutex;
    std::condition_variable initCV;
    
    // For object allocation
    size_t instanceSize = 0; // Size of instance fields
    size_t fieldCount = 0;
    
    // Source file info
    std::string sourceFile;
    std::string signature;
    
    Class(std::string n, ClassLoader* l) : name(std::move(n)), loader(l) {}
    
    // Resolution
    FieldRef findField(const std::string& name, const std::string& descriptor);
    MethodRef findMethod(const std::string& name, const std::string& descriptor);
    MethodRef findMethod(const std::string& name, const std::string& descriptor, bool includeSuper);
    
    // Initialization
    void initialize(ThreadRef thread);
    bool isInitialized() const { return initState == InitState::INITIALIZED; }
    bool isInterface() const { return (accessFlags & AccessFlags::INTERFACE) != AccessFlags(0); }
    bool isArray() const { return name.size() > 0 && name[0] == '['; }
    bool isPrimitive() const { return false; }
    
    // Subclass check
    bool isAssignableFrom(ClassRef other) const;
    bool isSubclassOf(ClassRef other) const;
};

// ============================================================
// Object Representation
// ============================================================

class Object : public std::enable_shared_from_this<Object> {
public:
    ClassRef clazz;
    std::vector<Value> fields; // Instance fields
    std::atomic<uint32_t> refCount{1}; // For GC
    
    // Monitor for synchronization
    std::mutex monitorMutex;
    std::thread::id monitorOwner;
    int monitorCount = 0;
    std::condition_variable monitorCV;
    
    // Identity hash code
    size_t identityHash = 0;
    bool hashComputed = false;
    
    Object(ClassRef c) : clazz(c) {
        fields.resize(c->fieldCount);
    }
    
    // Array support
    bool isArray() const { return clazz->isArray(); }
    size_t arrayLength() const { return fields.size(); }
    Value& arrayGet(size_t index) { return fields[index]; }
    void arraySet(size_t index, const Value& val) { fields[index] = val; }
    
    // Monitor operations
    void monitorEnter(ThreadRef thread);
    void monitorExit(ThreadRef thread);
    bool monitorTryEnter(ThreadRef thread);
    
    size_t getIdentityHash();
    
    // GC support
    void addRef() { refCount.fetch_add(1); }
    void release() { if (refCount.fetch_sub(1) == 1) delete this; }
};

// ============================================================
// Thread & Stack Frames
// ============================================================

class Frame {
public:
    MethodRef method;
    ClassRef clazz;
    ThreadRef thread;
    
    // Operand stack
    std::vector<Value> stack;
    size_t stackTop = 0;
    
    // Local variables
    std::vector<Value> locals;
    
    // Program counter
    size_t pc = 0;
    
    // For exception handling
    Frame* caller = nullptr;
    
    Frame(MethodRef m, ThreadRef t, size_t maxStack, size_t maxLocals)
        : method(m), thread(t), stack(maxStack), locals(maxLocals), clazz(m->declaringClass) {}
    
    // Stack operations
    void push(const Value& v) { stack[stackTop++] = v; }
    Value pop() { return stack[--stackTop]; }
    Value& top() { return stack[stackTop - 1]; }
    Value& peek(int n) { return stack[stackTop - 1 - n]; }
    
    // Local variable access
    Value& local(int index) { return locals[index]; }
    const Value& local(int index) const { return locals[index]; }
    
    // PC operations
    uint8_t nextByte() { return method->bytecode[pc++]; }
    uint16_t nextShort() {
        uint16_t val = (method->bytecode[pc] << 8) | method->bytecode[pc + 1];
        pc += 2;
        return val;
    }
    
    // Line number
    int getLineNumber() const;
};

class Thread : public std::enable_shared_from_this<Thread> {
public:
    enum class State { NEW, RUNNABLE, BLOCKED, WAITING, TIMED_WAITING, TERMINATED };
    
    std::atomic<State> state{State::NEW};
    std::thread nativeThread;
    ClassLoader* classLoader = nullptr;
    
    // Call stack
    std::vector<std::unique_ptr<Frame>> frames;
    Frame* currentFrame = nullptr;
    
    // For park/unpark
    std::mutex parkMutex;
    // std::condition_variable parkCV;
    bool parked = false;
    
    // Thread-local storage
    std::unordered_map<std::string, ObjectRef> threadLocals;
    
    // Exception handling
    ObjectRef pendingException;
    bool hasPendingException() const { return pendingException != nullptr; }
    
    // Thread ID
    size_t threadId;
    std::string name;
    
    Thread(size_t id, const std::string& n) : threadId(id), name(n) {}
    
    void pushFrame(std::unique_ptr<Frame> frame);
    void popFrame();
    Frame* topFrame() { return currentFrame; }
    
    // Stack trace
    std::string getStackTrace() const;
    
    // Park/unpark - simplified
    void park();
    void unpark();
    
    static ThreadRef current();
};

// ============================================================
// Class Loader Hierarchy (Bootstrap -> Extension -> Application)
// ============================================================

class ClassLoader {
public:
    ClassLoader* parent = nullptr;
    std::vector<std::string> classpath;
    std::vector<std::string> modulePath;
    
    // Loaded classes cache
    std::unordered_map<std::string, ClassRef> loadedClasses;
    std::mutex classesMutex;
    
    // Built-in class loaders
    static ClassLoader* bootstrapLoader;
    static ClassLoader* systemLoader;
    
    ClassLoader(ClassLoader* p = nullptr) : parent(p) {}
    
    // Load class by binary name (e.g., "java/lang/String")
    ClassRef loadClass(const std::string& name, bool resolve = true);
    
    // Find class in cache or parent
    ClassRef findLoadedClass(const std::string& name);
    
    // Load from file system
    ClassRef loadFromFile(const std::string& name);
    
    // Load from resource stream
    ClassRef loadFromStream(const std::string& name, std::istream& stream);
    
    // Define class from byte array
    ClassRef defineClass(const std::string& name, const std::vector<uint8_t>& data);
    
    // Resolve symbolic references
    void resolveClass(ClassRef clazz);
    
    // Get resource
    std::shared_ptr<std::istream> getResourceAsStream(const std::string& name);
    
    // Module support
    bool isModule(const std::string& name) const;
};

// ============================================================
// FVM Main Class
// ============================================================

class FVM {
public:
    static FVM& instance() {
        static FVM vm;
        return vm;
    }
    
    // Startup
    bool startup(const std::vector<std::string>& args);
    void shutdown();
    
    // Main entry
    int executeMain(ClassRef mainClass, const std::vector<std::string>& args);
    
    // Class loading
    ClassLoader* getBootstrapLoader() { return ClassLoader::bootstrapLoader; }
    ClassLoader* getSystemLoader() { return ClassLoader::systemLoader; }
    
    // Thread management
    ThreadRef createThread(const std::string& name, MethodRef entryPoint, const std::vector<Value>& args);
    ThreadRef currentThread();
    // void schedule();
    
    // GC
    void gc();
    void gcMinor();
    void gcMajor();
    
    // Memory management
    ObjectRef newObject(ClassRef clazz);
    ObjectRef newArray(ClassRef elementType, size_t length);
    ObjectRef newMultiArray(ClassRef clazz, const std::vector<size_t>& dimensions);
    
    // Primitive boxes
    ObjectRef boxInt(int32_t v);
    ObjectRef boxLong(int64_t v);
    ObjectRef boxFloat(float v);
    ObjectRef boxDouble(double v);
    ObjectRef boxBoolean(bool v);
    ObjectRef boxChar(uint16_t v);
    
    // Interned strings
    ObjectRef internString(const std::string& str);
    
    // System properties
    std::unordered_map<std::string, std::string> systemProperties;
    
    // JIT
    void enableJIT(bool enable);
    void compileMethod(MethodRef method);
    
    // Stats
    struct Stats {
        uint64_t classesLoaded = 0;
        uint64_t objectsAllocated = 0;
        uint64_t bytesAllocated = 0;
        uint64_t gcCollections = 0;
        uint64_t gcTimeMs = 0;
        size_t heapUsed = 0;
        size_t heapMax = 0;
    };
    Stats getStats() const;
    
private:
    // Internal methods - implemented
    void executeThread(ThreadRef thread);
    void executeOpcode(ThreadRef thread, Frame* frame, Opcode opcode);
    
    // Thread parking
    void park();
    void unpark();
    
    // Condition variable for thread parking
    std::condition_variable parkCV;
};
    
private:
    FVM();
    ~FVM();
    
    // std::unique_ptr<UIToolkit> uiToolkit;
    std::vector<ThreadRef> allThreads;
    std::mutex threadsMutex;
    std::atomic<bool> running{false};
    std::thread gcThread;
    std::thread jitThread;
};

// ============================================================
// UI Toolkit (Swing-like)
// ============================================================

namespace ui {

class Component;
class Container;
class Window;
class Frame;
class Panel;
class Button;
class Label;
class TextField;
class TextArea;
class LayoutManager;
class FlowLayout;
class BorderLayout;
class GridLayout;
class GridBagLayout;
class Event;
class ActionEvent;
class MouseEvent;
class KeyEvent;
class ActionListener;
class MouseListener;
class KeyListener;

// Graphics context
class Graphics {
public:
    virtual ~Graphics() = default;
    virtual void drawString(const std::string& text, int x, int y) = 0;
    virtual void drawRect(int x, int y, int width, int height) = 0;
    virtual void fillRect(int x, int y, int width, int height) = 0;
    virtual void setColor(uint32_t rgba) = 0;
    virtual void setFont(const std::string& name, int size, bool bold, bool italic) = 0;
    virtual std::pair<int, int> getSize() const = 0;
};

// Base component
class Component {
protected:
    int x = 0, y = 0, width = 0, height = 0;
    uint32_t background = 0xFFFFFFFF;
    uint32_t foreground = 0xFF000000;
    std::string fontName = "Sans";
    int fontSize = 12;
    bool bold = false, italic = false;
    bool visible = true;
    bool enabled = true;
    Container* parent = nullptr;
    std::vector<std::shared_ptr<ActionListener>> actionListeners;
    std::vector<std::shared_ptr<MouseListener>> mouseListeners;
    std::vector<std::shared_ptr<KeyListener>> keyListeners;
    
public:
    virtual ~Component() = default;
    
    // Position/size
    void setBounds(int x, int y, int w, int h);
    void setLocation(int x, int y);
    void setSize(int w, int h);
    void setPreferredSize(int w, int h);
    
    // Colors
    void setBackground(uint32_t rgba);
    void setForeground(uint32_t rgba);
    
    // Font
    void setFont(const std::string& name, int size, bool bold = false, bool italic = false);
    
    // Visibility
    void setVisible(bool v);
    void setEnabled(bool e);
    
    // Parent
    Container* getParent() const { return parent; }
    void setParent(Container* p) { parent = p; }
    
    // Event listeners
    void addActionListener(std::shared_ptr<ActionListener> l);
    void addMouseListener(std::shared_ptr<MouseListener> l);
    void addKeyListener(std::shared_ptr<KeyListener> l);
    
    // Painting
    virtual void paint(Graphics& g) = 0;
    virtual std::pair<int, int> getPreferredSize() const = 0;
    
    // Event dispatch
    virtual void processEvent(Event& e);
};

// Container
class Container : public Component {
    std::vector<std::shared_ptr<Component>> children;
    std::shared_ptr<LayoutManager> layout;
    
public:
    void add(std::shared_ptr<Component> child);
    void remove(std::shared_ptr<Component> child);
    void removeAll();
    std::vector<std::shared_ptr<Component>> getComponents() const;
    
    void setLayout(std::shared_ptr<LayoutManager> lm);
    void doLayout();
    void validate();
    void invalidate();
    
    void paint(Graphics& g) override;
};

// Window/Frame
class Window : public Container {
    std::string title;
    bool resizable = true;
    bool closeOperation = true; // EXIT_ON_CLOSE
    bool modal = false;
    void* nativeHandle = nullptr; // Platform-specific
    
public:
    Window(const std::string& title, int width, int height);
    ~Window();
    
    void setTitle(const std::string& t);
    void setResizable(bool r);
    void setDefaultCloseOperation(int op);
    void setVisible(bool v);
    void setModal(bool m);
    void pack();
    void dispose();
    
    // Native window operations
    void nativeCreate();
    void nativeShow();
    void nativeHide();
    void nativeSetTitle(const std::string& t);
    void nativeSetBounds(int x, int y, int w, int h);
};

// Frame (top-level window)
class Frame : public Window {
public:
    Frame(const std::string& title);
    Frame(const std::string& title, int width, int height);
};

// Panels
class Panel : public Container {
public:
    Panel();
    Panel(std::shared_ptr<LayoutManager> layout);
};

// Button
class Button : public Component {
    std::string text;
    bool pressed = false;
    
public:
    Button(const std::string& label);
    void setText(const std::string& t);
    const std::string& getText() const { return text; }
    
    void paint(Graphics& g) override;
    std::pair<int, int> getPreferredSize() const override;
    
    void setActionCommand(const std::string& cmd);
    std::string getActionCommand() const;
    
    void click(); // Programmatic click
};

// Label
class Label : public Component {
    std::string text;
    int alignment = 0; // LEFT, CENTER, RIGHT
    
public:
    Label(const std::string& text);
    void setText(const std::string& t);
    const std::string& getText() const { return text; }
    void setAlignment(int a);
    
    void paint(Graphics& g) override;
    std::pair<int, int> getPreferredSize() const override;
};

// TextField
class TextField : public Component {
    std::string text;
    int columns = 20;
    bool editable = true;
    int cursorPos = 0;
    int scrollOffset = 0;
    
public:
    TextField(int cols = 20);
    TextField(const std::string& text, int cols = 20);
    
    void setText(const std::string& t);
    const std::string& getText() const { return text; }
    void setEditable(bool e);
    void setColumns(int c);
    
    void paint(Graphics& g) override;
    std::pair<int, int> getPreferredSize() const override;
    
    void processEvent(Event& e) override;
};

// TextArea
class TextArea : public Component {
    std::vector<std::string> lines;
    int rows = 10, cols = 40;
    int scrollX = 0, scrollY = 0;
    bool editable = true;
    bool lineWrap = false;
    
public:
    TextArea(int rows = 10, int cols = 40);
    TextArea(const std::string& text, int rows, int cols);
    
    void setText(const std::string& text);
    std::string getText() const;
    void append(const std::string& text);
    void setRows(int r);
    void setColumns(int c);
    void setLineWrap(bool w);
    void setEditable(bool e);
    
    void paint(Graphics& g) override;
    std::pair<int, int> getPreferredSize() const override;
};

// Layout managers
class LayoutManager {
public:
    virtual ~LayoutManager() = default;
    virtual void addLayoutComponent(const std::string& name, std::shared_ptr<Component> comp) = 0;
    virtual void removeLayoutComponent(std::shared_ptr<Component> comp) = 0;
    virtual std::pair<int, int> preferredLayoutSize(Container* parent) = 0;
    virtual std::pair<int, int> minimumLayoutSize(Container* parent) = 0;
    virtual void layoutContainer(Container* parent) = 0;
};

class FlowLayout : public LayoutManager {
    int alignment = 0; // CENTER
    int hgap = 5, vgap = 5;
public:
    FlowLayout(int align = 0, int hgap = 5, int vgap = 5);
    void layoutContainer(Container* parent) override;
    std::pair<int, int> preferredLayoutSize(Container* parent) override;
    std::pair<int, int> minimumLayoutSize(Container* parent) override;
};

class BorderLayout : public LayoutManager {
    std::shared_ptr<Component> north, south, east, west, center;
    int hgap = 0, vgap = 0;
public:
    BorderLayout(int hgap = 0, int vgap = 0);
    void addLayoutComponent(const std::string& name, std::shared_ptr<Component> comp) override;
    void removeLayoutComponent(std::shared_ptr<Component> comp) override;
    void layoutContainer(Container* parent) override;
    std::pair<int, int> preferredLayoutSize(Container* parent) override;
    std::pair<int, int> minimumLayoutSize(Container* parent) override;
};

class GridLayout : public LayoutManager {
    int rows = 0, cols = 0;
    int hgap = 0, vgap = 0;
public:
    GridLayout(int rows, int cols, int hgap = 0, int vgap = 0);
    void layoutContainer(Container* parent) override;
    std::pair<int, int> preferredLayoutSize(Container* parent) override;
    std::pair<int, int> minimumLayoutSize(Container* parent) override;
};

// Events
class Event {
public:
    enum class Type {
        ACTION,
        MOUSE_CLICKED,
        MOUSE_PRESSED,
        MOUSE_RELEASED,
        MOUSE_ENTERED,
        MOUSE_EXITED,
        MOUSE_MOVED,
        MOUSE_DRAGGED,
        KEY_PRESSED,
        KEY_RELEASED,
        KEY_TYPED,
        WINDOW_OPENED,
        WINDOW_CLOSING,
        WINDOW_CLOSED,
        WINDOW_ICONIFIED,
        WINDOW_DEICONIFIED,
        WINDOW_ACTIVATED,
        WINDOW_DEACTIVATED,
    };
    
    Type type;
    std::shared_ptr<Component> source;
    int64_t when = 0;
    int modifiers = 0;
};

class ActionEvent : public Event {
public:
    std::string actionCommand;
    ActionEvent(std::shared_ptr<Component> src, const std::string& cmd);
};

class MouseEvent : public Event {
public:
    int x, y;
    int clickCount;
    int button;
    MouseEvent(std::shared_ptr<Component> src, Type t, int x, int y, int btn, int clicks, int mods);
};

class KeyEvent : public Event {
public:
    int keyCode;
    char keyChar;
    KeyEvent(std::shared_ptr<Component> src, Type t, int code, char ch, int mods);
};

// Event listeners
class ActionListener {
public:
    virtual ~ActionListener() = default;
    virtual void actionPerformed(ActionEvent& e) = 0;
};

class MouseListener {
public:
    virtual ~MouseListener() = default;
    virtual void mouseClicked(MouseEvent& e) {}
    virtual void mousePressed(MouseEvent& e) {}
    virtual void mouseReleased(MouseEvent& e) {}
    virtual void mouseEntered(MouseEvent& e) {}
    virtual void mouseExited(MouseEvent& e) {}
};

class KeyListener {
public:
    virtual ~KeyListener() = default;
    virtual void keyPressed(KeyEvent& e) {}
    virtual void keyReleased(KeyEvent& e) {}
    virtual void keyTyped(KeyEvent& e) {}
};

// Adapter classes (empty implementations)
class ActionAdapter : public ActionListener {
public:
    void actionPerformed(ActionEvent& e) override {}
};

class MouseAdapter : public MouseListener {};
class KeyAdapter : public KeyListener {};

// Event queue
class EventQueue {
    std::queue<std::shared_ptr<Event>> events;
    std::mutex mutex;
    std::condition_variable cv;
    
public:
    void postEvent(std::shared_ptr<Event> e);
    std::shared_ptr<Event> getNextEvent();
    bool hasEvents() const;
    
    static EventQueue& instance();
};

// Main event loop
void runEventLoop();
void exitEventLoop();
void invokeLater(std::function<void()> f);
void invokeAndWait(std::function<void()> f);

// ============================================================
// Native UI Backends (Platform-specific)
// ============================================================

namespace native {
    class WindowImpl;
    class GraphicsImpl;
    
    // Platform detection
    #if defined(_WIN32)
        const char* PLATFORM = "windows";
    #elif defined(__APPLE__)
        const char* PLATFORM = "macos";
    #elif defined(__linux__)
        const char* PLATFORM = "linux";
    #else
        const char* PLATFORM = "unknown";
    #endif
    
    // Initialize platform UI
    void initPlatformUI();
    void shutdownPlatformUI();
    
    // Window operations
    void* createWindow(const std::string& title, int x, int y, int w, int h, bool resizable);
    void showWindow(void* handle);
    void hideWindow(void* handle);
    void destroyWindow(void* handle);
    void setWindowTitle(void* handle, const std::string& title);
    void setWindowBounds(void* handle, int x, int y, int w, int h);
    
    // Graphics
    GraphicsImpl* createGraphics(void* windowHandle);
    void destroyGraphics(GraphicsImpl* g);
    
    // Event pump
    void pumpEvents();
    bool hasPendingEvents();
    
    // Clipboard
    void setClipboardText(const std::string& text);
    std::string getClipboardText();
    
    // File dialogs
    std::string showOpenFileDialog(const std::string& title, const std::vector<std::pair<std::string, std::string>>& filters);
    std::string showSaveFileDialog(const std::string& title, const std::vector<std::pair<std::string, std::string>>& filters);
    std::string showFolderDialog(const std::string& title);
    
    // Message dialogs
    int showMessageDialog(void* parent, const std::string& message, const std::string& title, int type);
    int showConfirmDialog(void* parent, const std::string& message, const std::string& title, int type);
    std::string showInputDialog(void* parent, const std::string& message, const std::string& title);
}

} // namespace ui

} // namespace forge::fvm