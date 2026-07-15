#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

namespace forge::fvm {

// ============================================================
// FVM Class File Format (.fclass)
// ============================================================

constexpr uint32_t FCLASS_MAGIC = 0xCAFEF00D;
constexpr uint16_t FCLASS_VERSION_MAJOR = 1;
constexpr uint16_t FCLASS_VERSION_MINOR = 0;

enum class CPTag : uint8_t {
    UTF8 = 1,
    INTEGER = 3,
    FLOAT = 4,
    LONG = 5,
    DOUBLE = 6,
    CLASS = 7,
    STRING = 8,
    BOOLEAN = 9,
    METHODREF = 10,
    NAME_AND_TYPE = 12,
    FORGE_METHOD = 200,
    FORGE_PROPERTY = 201,
};

struct CPInfo {
    CPTag tag;
    std::vector<uint8_t> data;

    std::string getUtf8() const;
    int32_t getInteger() const;
    float getFloat() const;
    int64_t getLong() const;
    double getDouble() const;
    uint16_t getClassIndex() const;
    uint16_t getStringIndex() const;

    struct NameAndType { uint16_t nameIndex; uint16_t descriptorIndex; };
    NameAndType getNameAndType() const;

    struct MethodRef { uint16_t classIndex; uint16_t nameAndTypeIndex; };
    MethodRef getMethodref() const;
};

struct ExceptionHandler {
    uint16_t startPc;
    uint16_t endPc;
    uint16_t handlerPc;
    uint16_t catchType;
};

struct AttributeInfo {
    enum class Type { UNKNOWN, CODE, SOURCE_FILE, LINE_NUMBER_TABLE, LOCAL_VARIABLE_TABLE };
    uint16_t nameIndex;
    std::vector<uint8_t> data;
    Type type = Type::UNKNOWN;
};

struct FieldInfo {
    uint16_t accessFlags;
    uint16_t nameIndex;
    uint16_t descriptorIndex;
    std::vector<AttributeInfo> attributes;
};

struct MethodInfo {
    uint16_t accessFlags;
    uint16_t nameIndex;
    uint16_t descriptorIndex;
    std::vector<AttributeInfo> attributes;
    std::vector<uint8_t> bytecode;
    uint16_t maxStack = 0;
    uint16_t maxLocals = 0;
    uint16_t arity = 0;
    uint16_t upvalueCount = 0;
    std::vector<ExceptionHandler> exceptionTable;
    uint16_t constantPoolOffset = 0; // starting index of this method's constants in the shared CP
    uint16_t constantPoolCount = 0;  // number of constants this method uses
};

struct ClassFile {
    uint32_t magic;
    uint16_t minorVersion;
    uint16_t majorVersion;
    std::vector<CPInfo> constantPool;
    uint16_t accessFlags;
    uint16_t thisClass;
    uint16_t superClass;
    std::vector<uint16_t> interfaces;
    std::vector<FieldInfo> fields;
    std::vector<MethodInfo> methods;
    std::vector<AttributeInfo> attributes;

    bool load(const uint8_t* data, size_t len);
    bool load(const std::vector<uint8_t>& data) { return load(data.data(), data.size()); }
    bool save(std::vector<uint8_t>& data) const;
    std::string getUtf8(uint16_t index) const;
    std::string getClassName(uint16_t index) const;
    std::pair<std::string, std::string> getNameAndType(uint16_t index) const;
    std::tuple<std::string, std::string, std::string> getMethodref(uint16_t index) const;
};

} // namespace forge::fvm
