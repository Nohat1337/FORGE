#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <variant>
#include <memory>

namespace forge::fvm {

// ============================================================
// FVM Class File Format (.fclass) - JVM-inspired
// ============================================================

// Magic number: 0xCAFEF00D (Forge VM)
constexpr uint32_t FCLASS_MAGIC = 0xCAFEF00D;
constexpr uint16_t FCLASS_VERSION = 50; // Major version

// Constant pool tags (matching JVM)
enum class CPTag : uint8_t {
    UTF8 = 1,
    INTEGER = 3,
    FLOAT = 4,
    LONG = 5,
    DOUBLE = 6,
    CLASS = 7,
    STRING = 8,
    FIELDREF = 9,
    METHODREF = 10,
    INTERFACE_METHODREF = 11,
    NAME_AND_TYPE = 12,
    METHOD_HANDLE = 15,
    METHOD_TYPE = 16,
    DYNAMIC = 17,
    INVOKE_DYNAMIC = 18,
    MODULE = 19,
    PACKAGE = 20,
};

// Access flags
enum class AccessFlags : uint16_t {
    PUBLIC = 0x0001,
    PRIVATE = 0x0002,
    PROTECTED = 0x0004,
    STATIC = 0x0008,
    FINAL = 0x0010,
    SYNCHRONIZED = 0x0020,
    VOLATILE = 0x0040,
    TRANSIENT = 0x0080,
    NATIVE = 0x0100,
    INTERFACE = 0x0200,
    ABSTRACT = 0x0400,
    STRICT = 0x0800,
    SYNTHETIC = 0x1000,
    ANNOTATION = 0x2000,
    ENUM = 0x4000,
    MODULE = 0x8000,
};

// Constant pool entry
struct CPInfo {
    CPTag tag;
    std::vector<uint8_t> data; // Raw bytes for flexibility
    
    // Typed accessors
    std::string getUtf8() const;
    int32_t getInteger() const;
    float getFloat() const;
    int64_t getLong() const;
    double getDouble() const;
    uint16_t getClassIndex() const;
    uint16_t getStringIndex() const;
    struct { uint16_t classIndex; uint16_t nameAndTypeIndex; } getFieldref() const;
    struct { uint16_t classIndex; uint16_t nameAndTypeIndex; } getMethodref() const;
    struct { uint16_t nameIndex; uint16_t descriptorIndex; } getNameAndType() const;
};

// Field info
struct FieldInfo {
    AccessFlags accessFlags;
    uint16_t nameIndex;
    uint16_t descriptorIndex;
    std::vector<AttributeInfo> attributes;
};

// Method info
struct MethodInfo {
    AccessFlags accessFlags;
    uint16_t nameIndex;
    uint16_t descriptorIndex;
    std::vector<AttributeInfo> attributes;
    
    // Resolved at load time
    struct CodeAttribute* code = nullptr;
    std::vector<uint8_t> bytecode; // For quick access
    uint16_t maxStack = 0;
    uint16_t maxLocals = 0;
    std::vector<ExceptionHandler> exceptionTable;
};

// Exception handler
struct ExceptionHandler {
    uint16_t startPc;
    uint16_t endPc;
    uint16_t handlerPc;
    uint16_t catchType; // Constant pool index
};

// Attribute info (generic)
struct AttributeInfo {
    uint16_t nameIndex;
    std::vector<uint8_t> data;
    
    // Known attribute types
    enum class Type {
        UNKNOWN,
        CODE,
        CONSTANT_VALUE,
        STACK_MAP_TABLE,
        EXCEPTIONS,
        INNER_CLASSES,
        ENCLOSING_METHOD,
        SYNTHETIC,
        SIGNATURE,
        SOURCE_FILE,
        SOURCE_DEBUG_EXTENSION,
        LINE_NUMBER_TABLE,
        LOCAL_VARIABLE_TABLE,
        LOCAL_VARIABLE_TYPE_TABLE,
        DEPRECATED,
        RUNTIME_VISIBLE_ANNOTATIONS,
        RUNTIME_INVISIBLE_ANNOTATIONS,
        RUNTIME_VISIBLE_PARAMETER_ANNOTATIONS,
        RUNTIME_INVISIBLE_PARAMETER_ANNOTATIONS,
        ANNOTATION_DEFAULT,
        BOOTSTRAP_METHODS,
        METHOD_PARAMETERS,
        MODULE,
        MODULE_PACKAGES,
        MODULE_MAIN_CLASS,
        NEST_HOST,
        NEST_MEMBERS,
    };
    Type type = Type::UNKNOWN;
};

// Code attribute (method bytecode)
struct CodeAttribute {
    uint16_t maxStack;
    uint16_t maxLocals;
    std::vector<uint8_t> bytecode;
    std::vector<ExceptionHandler> exceptionTable;
    std::vector<AttributeInfo> attributes;
};

// Stack map frame (for bytecode verification)
struct StackMapFrame {
    enum class Type : uint8_t {
        SAME_FRAME = 0,           // 0-63
        SAME_LOCALS_1_STACK = 64, // 64-127
        SAME_LOCALS_1_STACK_EXT = 247,
        CHOP = 248,               // 248-250
        SAME_EXT = 251,
        APPEND = 252,             // 252-254
        FULL = 255,
    };
    Type frameType;
    std::vector<uint8_t> locals; // Verification types
    std::vector<uint8_t> stack;  // Verification types
};

// Verification type
enum class VerificationType : uint8_t {
    TOP = 0,
    INTEGER = 1,
    FLOAT = 2,
    LONG = 3,
    DOUBLE = 4,
    NULL_TYPE = 5,
    UNINITIALIZED_THIS = 6,
    OBJECT = 7,        // followed by class index
    UNINITIALIZED = 8, // followed by offset
};

// Class file structure
struct ClassFile {
    uint32_t magic;
    uint16_t minorVersion;
    uint16_t majorVersion;
    uint16_t constantPoolCount;
    std::vector<CPInfo> constantPool; // Index 1 to count-1
    AccessFlags accessFlags;
    uint16_t thisClass;
    uint16_t superClass;
    uint16_t interfacesCount;
    std::vector<uint16_t> interfaces;
    uint16_t fieldsCount;
    std::vector<FieldInfo> fields;
    uint16_t methodsCount;
    std::vector<MethodInfo> methods;
    uint16_t attributesCount;
    std::vector<AttributeInfo> attributes;
    
    // Resolved references (cached)
    std::string className;
    std::string superClassName;
    std::vector<std::string> interfaceNames;
    
    // Methods
    bool load(const std::vector<uint8_t>& data);
    bool verify() const;
    const MethodInfo* findMethod(const std::string& name, const std::string& descriptor) const;
    const FieldInfo* findField(const std::string& name, const std::string& descriptor) const;
    
    // Constant pool helpers
    std::string getUtf8(uint16_t index) const;
    std::string getClassName(uint16_t index) const;
    std::pair<std::string, std::string> getNameAndType(uint16_t index) const;
    std::tuple<std::string, std::string, std::string> getFieldref(uint16_t index) const;
    std::tuple<std::string, std::string, std::string> getMethodref(uint16_t index) const;
};

// ============================================================
// FVM Opcodes (JVM-inspired)
// ============================================================

enum class Opcode : uint8_t {
    // Constants
    NOP = 0x00,
    ACONST_NULL = 0x01,
    ICONST_M1 = 0x02,
    ICONST_0 = 0x03,
    ICONST_1 = 0x04,
    ICONST_2 = 0x05,
    ICONST_3 = 0x06,
    ICONST_4 = 0x07,
    ICONST_5 = 0x08,
    LCONST_0 = 0x09,
    LCONST_1 = 0x0A,
    FCONST_0 = 0x0B,
    FCONST_1 = 0x0C,
    FCONST_2 = 0x0D,
    DCONST_0 = 0x0E,
    DCONST_1 = 0x0F,
    BIPUSH = 0x10,
    SIPUSH = 0x11,
    LDC = 0x12,
    LDC_W = 0x13,
    LDC2_W = 0x14,
    
    // Load from local variables
    ILOAD = 0x15,
    LLOAD = 0x16,
    FLOAD = 0x17,
    DLOAD = 0x18,
    ALOAD = 0x19,
    ILOAD_0 = 0x1A,
    ILOAD_1 = 0x1B,
    ILOAD_2 = 0x1C,
    ILOAD_3 = 0x1D,
    LLOAD_0 = 0x1E,
    LLOAD_1 = 0x1F,
    LLOAD_2 = 0x20,
    LLOAD_3 = 0x21,
    FLOAD_0 = 0x22,
    FLOAD_1 = 0x23,
    FLOAD_2 = 0x24,
    FLOAD_3 = 0x25,
    DLOAD_0 = 0x26,
    DLOAD_1 = 0x27,
    DLOAD_2 = 0x28,
    DLOAD_3 = 0x29,
    ALOAD_0 = 0x2A,
    ALOAD_1 = 0x2B,
    ALOAD_2 = 0x2C,
    ALOAD_3 = 0x2D,
    IALOAD = 0x2E,
    LALOAD = 0x2F,
    FALOAD = 0x30,
    DALOAD = 0x31,
    AALOAD = 0x32,
    BALOAD = 0x33,
    CALOAD = 0x34,
    SALOAD = 0x35,
    
    // Store to local variables
    ISTORE = 0x36,
    LSTORE = 0x37,
    FSTORE = 0x38,
    DSTORE = 0x39,
    ASTORE = 0x3A,
    ISTORE_0 = 0x3B,
    ISTORE_1 = 0x3C,
    ISTORE_2 = 0x3D,
    ISTORE_3 = 0x3E,
    LSTORE_0 = 0x3F,
    LSTORE_1 = 0x40,
    LSTORE_2 = 0x41,
    LSTORE_3 = 0x42,
    FSTORE_0 = 0x43,
    FSTORE_1 = 0x44,
    FSTORE_2 = 0x45,
    FSTORE_3 = 0x46,
    DSTORE_0 = 0x47,
    DSTORE_1 = 0x48,
    DSTORE_2 = 0x49,
    DSTORE_3 = 0x4A,
    ASTORE_0 = 0x4B,
    ASTORE_1 = 0x4C,
    ASTORE_2 = 0x4D,
    ASTORE_3 = 0x4E,
    IASTORE = 0x4F,
    LASTORE = 0x50,
    FASTORE = 0x51,
    DASTORE = 0x52,
    AASTORE = 0x53,
    BASTORE = 0x54,
    CASTORE = 0x55,
    SASTORE = 0x56,
    
    // Stack operations
    POP = 0x57,
    POP2 = 0x58,
    DUP = 0x59,
    DUP_X1 = 0x5A,
    DUP_X2 = 0x5B,
    DUP2 = 0x5C,
    DUP2_X1 = 0x5D,
    DUP2_X2 = 0x5E,
    SWAP = 0x5F,
    
    // Arithmetic
    IADD = 0x60,
    LADD = 0x61,
    FADD = 0x62,
    DADD = 0x63,
    ISUB = 0x64,
    LSUB = 0x65,
    FSUB = 0x66,
    DSUB = 0x67,
    IMUL = 0x68,
    LMUL = 0x69,
    FMUL = 0x6A,
    DMUL = 0x6B,
    IDIV = 0x6C,
    LDIV = 0x6D,
    FDIV = 0x6E,
    DDIV = 0x6F,
    IREM = 0x70,
    LREM = 0x71,
    FREM = 0x72,
    DREM = 0x73,
    INEG = 0x74,
    LNEG = 0x75,
    FNEG = 0x76,
    DNEG = 0x77,
    ISHL = 0x78,
    LSHL = 0x79,
    ISHR = 0x7A,
    LSHR = 0x7B,
    IUSHR = 0x7C,
    LUSHR = 0x7D,
    IAND = 0x7E,
    LAND = 0x7F,
    IOR = 0x80,
    LOR = 0x81,
    IXOR = 0x82,
    LXOR = 0x83,
    IINC = 0x84,
    I2L = 0x85,
    I2F = 0x86,
    I2D = 0x87,
    L2I = 0x88,
    L2F = 0x89,
    L2D = 0x8A,
    F2I = 0x8B,
    F2L = 0x8C,
    F2D = 0x8D,
    D2I = 0x8E,
    D2L = 0x8F,
    D2F = 0x90,
    I2B = 0x91,
    I2C = 0x92,
    I2S = 0x93,
    LCMP = 0x94,
    FCMPL = 0x95,
    FCMPG = 0x96,
    DCMPL = 0x97,
    DCMPG = 0x98,
    
    // Control flow
    IFEQ = 0x99,
    IFNE = 0x9A,
    IFLT = 0x9B,
    IFGE = 0x9C,
    IFGT = 0x9D,
    IFLE = 0x9E,
    IF_ICMPEQ = 0x9F,
    IF_ICMPNE = 0xA0,
    IF_ICMPLT = 0xA1,
    IF_ICMPGE = 0xA2,
    IF_ICMPGT = 0xA3,
    IF_ICMPLE = 0xA4,
    IF_ACMPEQ = 0xA5,
    IF_ACMPNE = 0xA6,
    GOTO = 0xA7,
    JSR = 0xA8,
    RET = 0xA9,
    TABLESWITCH = 0xAA,
    LOOKUPSWITCH = 0xAB,
    IRETURN = 0xAC,
    LRETURN = 0xAD,
    FRETURN = 0xAE,
    DRETURN = 0xAF,
    ARETURN = 0xB0,
    RETURN = 0xB1,
    
    // Field access
    GETSTATIC = 0xB2,
    PUTSTATIC = 0xB3,
    GETFIELD = 0xB4,
    PUTFIELD = 0xB5,
    
    // Method invocation
    INVOKEVIRTUAL = 0xB6,
    INVOKESPECIAL = 0xB7,
    INVOKESTATIC = 0xB8,
    INVOKEINTERFACE = 0xB9,
    INVOKEDYNAMIC = 0xBA,
    
    // Object creation/manipulation
    NEW = 0xBB,
    NEWARRAY = 0xBC,
    ANEWARRAY = 0xBD,
    ARRAYLENGTH = 0xBE,
    ATHROW = 0xBF,
    CHECKCAST = 0xC0,
    INSTANCEOF = 0xC1,
    MONITORENTER = 0xC2,
    MONITOREXIT = 0xC3,
    
    // Extended
    WIDE = 0xC4,
    MULTIANEWARRAY = 0xC5,
    IFNULL = 0xC6,
    IFNONNULL = 0xC7,
    GOTO_W = 0xC8,
    JSR_W = 0xC9,
    
    // Forge extensions
    FORGE_INVOKE_NATIVE = 0xE0,
    FORGE_GET_PROPERTY = 0xE1,
    FORGE_SET_PROPERTY = 0xE2,
    FORGE_TYPEOF = 0xE3,
    FORGE_ARRAY_NEW = 0xE4,
    FORGE_MAP_NEW = 0xE5,
    FORGE_CLOSURE_NEW = 0xE6,
    FORGE_YIELD = 0xE7,
    FORGE_AWAIT = 0xE8,
    FORGE_ASYNC_CALL = 0xE9,
    FORGE_TRY = 0xEA,
    FORGE_CATCH = 0xEB,
    FORGE_FINALLY = 0xEC,
    FORGE_MATCH = 0xED,
    FORGE_PATTERN = 0xEE,
};

// ============================================================
// Runtime Types (JVM-like)
// ============================================================

enum class ValueType : uint8_t {
    INT = 1,
    LONG = 2,
    FLOAT = 3,
    DOUBLE = 4,
    REFERENCE = 5, // Object reference
    RETURN_ADDRESS = 6,
};

struct Value {
    ValueType type;
    union {
        int32_t intVal;
        int64_t longVal;
        float floatVal;
        double doubleVal;
        void* refVal;
        uint16_t returnAddr;
    };
    
    Value() : type(ValueType::INT), intVal(0) {}
    Value(int32_t v) : type(ValueType::INT), intVal(v) {}
    Value(int64_t v) : type(ValueType::LONG), longVal(v) {}
    Value(float v) : type(ValueType::FLOAT), floatVal(v) {}
    Value(double v) : type(ValueType::DOUBLE), doubleVal(v) {}
    Value(void* v) : type(ValueType::REFERENCE), refVal(v) {}
    Value(std::nullptr_t) : type(ValueType::REFERENCE), refVal(nullptr) {}
    
    bool isNull() const { return type == ValueType::REFERENCE && refVal == nullptr; }
};

} // namespace forge::fvm