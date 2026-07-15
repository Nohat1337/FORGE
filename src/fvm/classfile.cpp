#include "classfile.hpp"
#include <cstring>
#include <stdexcept>

namespace forge::fvm {

// ============================================================
// Binary helpers
// ============================================================

static uint8_t readU8(const uint8_t* p) { return *p; }
static uint16_t readU16(const uint8_t* p) { return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t readU32(const uint8_t* p) {
    return (uint32_t)(p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]);
}
static int32_t readI32(const uint8_t* p) { return (int32_t)readU32(p); }
static float readF32(const uint8_t* p) { float v; std::memcpy(&v, p, 4); return v; }
static int64_t readI64(const uint8_t* p) {
    uint64_t lo = readU32(p + 4);
    uint64_t hi = readU32(p);
    return (int64_t)(hi << 32 | lo);
}
static double readF64(const uint8_t* p) { double v; std::memcpy(&v, p, 8); return v; }

static void writeU8(std::vector<uint8_t>& out, uint8_t v) { out.push_back(v); }
static void writeU16(std::vector<uint8_t>& out, uint16_t v) { out.push_back(v >> 8); out.push_back(v & 0xFF); }
static void writeU32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((v >> 24) & 0xFF);
    out.push_back((v >> 16) & 0xFF);
    out.push_back((v >> 8) & 0xFF);
    out.push_back(v & 0xFF);
}

// ============================================================
// CPInfo getters
// ============================================================

std::string CPInfo::getUtf8() const {
    return std::string(data.begin(), data.end());
}

int32_t CPInfo::getInteger() const {
    if (data.size() < 4) return 0;
    return readI32(data.data());
}

float CPInfo::getFloat() const {
    if (data.size() < 4) return 0.0f;
    return readF32(data.data());
}

int64_t CPInfo::getLong() const {
    if (data.size() < 8) return 0;
    return readI64(data.data());
}

double CPInfo::getDouble() const {
    if (data.size() < 8) return 0.0;
    double v;
    std::memcpy(&v, data.data(), 8);
    return v;
}

uint16_t CPInfo::getClassIndex() const {
    if (data.size() < 2) return 0;
    return readU16(data.data());
}

uint16_t CPInfo::getStringIndex() const {
    if (data.size() < 2) return 0;
    return readU16(data.data());
}

CPInfo::NameAndType CPInfo::getNameAndType() const {
    NameAndType nat = {0, 0};
    if (data.size() >= 4) {
        nat.nameIndex = readU16(data.data());
        nat.descriptorIndex = readU16(data.data() + 2);
    }
    return nat;
}

CPInfo::MethodRef CPInfo::getMethodref() const {
    MethodRef mr = {0, 0};
    if (data.size() >= 4) {
        mr.classIndex = readU16(data.data());
        mr.nameAndTypeIndex = readU16(data.data() + 2);
    }
    return mr;
}

// ============================================================
// ClassFile::getUtf8 / getClassName / getNameAndType / getMethodref
// ============================================================

std::string ClassFile::getUtf8(uint16_t index) const {
    if (index == 0 || index >= constantPool.size()) return "";
    auto& cp = constantPool[index];
    if (cp.tag != CPTag::UTF8) return "";
    return cp.getUtf8();
}

std::string ClassFile::getClassName(uint16_t index) const {
    if (index == 0 || index >= constantPool.size()) return "";
    auto& cp = constantPool[index];
    if (cp.tag != CPTag::CLASS) return "";
    return getUtf8(cp.getClassIndex());
}

std::pair<std::string, std::string> ClassFile::getNameAndType(uint16_t index) const {
    if (index == 0 || index >= constantPool.size()) return {"", ""};
    auto& cp = constantPool[index];
    if (cp.tag != CPTag::NAME_AND_TYPE) return {"", ""};
    auto nat = cp.getNameAndType();
    return {getUtf8(nat.nameIndex), getUtf8(nat.descriptorIndex)};
}

std::tuple<std::string, std::string, std::string> ClassFile::getMethodref(uint16_t index) const {
    if (index == 0 || index >= constantPool.size()) return {"", "", ""};
    auto& cp = constantPool[index];
    if (cp.tag != CPTag::METHODREF) return {"", "", ""};
    auto mr = cp.getMethodref();
    return {getClassName(mr.classIndex), std::get<0>(getNameAndType(mr.nameAndTypeIndex)),
            std::get<1>(getNameAndType(mr.nameAndTypeIndex))};
}

// ============================================================
// ClassFile::load — deserialize from binary data
// ============================================================

bool ClassFile::load(const uint8_t* data, size_t len) {
    if (len < 16) return false;
    const uint8_t* p = data;
    const uint8_t* end = data + len;

    // Magic number
    magic = readU32(p); p += 4;
    if (magic != FCLASS_MAGIC) return false;

    // Version
    minorVersion = readU16(p); p += 2;
    majorVersion = readU16(p); p += 2;

    // Constant pool count (1-indexed)
    uint16_t cpCount = readU16(p); p += 2;
    constantPool.clear();
    constantPool.resize(cpCount);
    constantPool[0] = {}; // index 0 is unused

    for (uint16_t i = 1; i < cpCount; i++) {
        if (p >= end) return false;
        CPTag tag = (CPTag)*p++;
        constantPool[i].tag = tag;

        switch (tag) {
            case CPTag::UTF8: {
                uint16_t slen = readU16(p); p += 2;
                if (p + slen > end) return false;
                constantPool[i].data.assign(p, p + slen);
                p += slen;
                break;
            }
            case CPTag::INTEGER:
            case CPTag::FLOAT:
                constantPool[i].data.assign(p, p + 4); p += 4;
                break;
            case CPTag::LONG:
            case CPTag::DOUBLE:
                constantPool[i].data.assign(p, p + 8); p += 8;
                break;
            case CPTag::BOOLEAN:
                constantPool[i].data.assign(p, p + 1); p += 1;
                break;
            case CPTag::CLASS:
            case CPTag::STRING:
                constantPool[i].data.assign(p, p + 2); p += 2;
                break;
            case CPTag::METHODREF:
            case CPTag::NAME_AND_TYPE:
            case CPTag::FORGE_PROPERTY:
                constantPool[i].data.assign(p, p + 4); p += 4;
                break;
            case CPTag::FORGE_METHOD: {
                // 2-byte method index (big-endian)
                if (p + 2 > end) return false;
                constantPool[i].data.assign(p, p + 2); p += 2;
                break;
            }
            default:
                return false;
        }
    }

    // Access flags
    if (p + 2 > end) return false;
    accessFlags = readU16(p); p += 2;

    // This class
    if (p + 2 > end) return false;
    thisClass = readU16(p); p += 2;

    // Super class
    if (p + 2 > end) return false;
    superClass = readU16(p); p += 2;

    // Interfaces
    uint16_t ifaceCount = readU16(p); p += 2;
    interfaces.clear();
    for (uint16_t i = 0; i < ifaceCount; i++) {
        if (p + 2 > end) return false;
        interfaces.push_back(readU16(p)); p += 2;
    }

    // Helper to read member info (fields/methods)
    auto readMembers = [&](std::vector<FieldInfo>& members) -> bool {
        uint16_t count = readU16(p); p += 2;
        members.clear();
        members.resize(count);
        for (uint16_t i = 0; i < count; i++) {
            if (p + 8 > end) return false;
            members[i].accessFlags = readU16(p); p += 2;
            members[i].nameIndex = readU16(p); p += 2;
            members[i].descriptorIndex = readU16(p); p += 2;
            uint16_t attrCount = readU16(p); p += 2;
            members[i].attributes.resize(attrCount);
            for (uint16_t j = 0; j < attrCount; j++) {
                if (p + 2 > end) return false;
                uint16_t nameIdx = readU16(p); p += 2;
                members[i].attributes[j].nameIndex = nameIdx;
                std::string attrName = getUtf8(nameIdx);
                if (attrName == "Code") members[i].attributes[j].type = AttributeInfo::Type::CODE;
                else if (attrName == "SourceFile") members[i].attributes[j].type = AttributeInfo::Type::SOURCE_FILE;

                if (p + 4 > end) return false;
                uint32_t attrLen = readU32(p); p += 4;
                if (p + attrLen > end) return false;
                members[i].attributes[j].data.assign(p, p + attrLen);
                p += attrLen;
            }
        }
        return true;
    };

    // Fields
    if (!readMembers(fields)) return false;

    // Methods
    uint16_t methodCount = readU16(p); p += 2;
    methods.clear();
    methods.resize(methodCount);
    for (uint16_t i = 0; i < methodCount; i++) {
        if (p + 8 > end) return false;
        methods[i].accessFlags = readU16(p); p += 2;
        methods[i].nameIndex = readU16(p); p += 2;
        methods[i].descriptorIndex = readU16(p); p += 2;
        uint16_t attrCount = readU16(p); p += 2;
        methods[i].attributes.resize(attrCount);
        for (uint16_t j = 0; j < attrCount; j++) {
            if (p + 2 > end) return false;
            uint16_t nameIdx = readU16(p); p += 2;
            methods[i].attributes[j].nameIndex = nameIdx;
            std::string attrName = getUtf8(nameIdx);

            if (p + 4 > end) return false;
            uint32_t attrLen = readU32(p); p += 4;
            if (p + attrLen > end) return false;

            if (attrName == "Code") {
                methods[i].attributes[j].type = AttributeInfo::Type::CODE;
                const uint8_t* cp = p;
                methods[i].maxStack = readU16(cp); cp += 2;
                methods[i].maxLocals = readU16(cp); cp += 2;
                methods[i].arity = readU16(cp); cp += 2;
                methods[i].upvalueCount = readU16(cp); cp += 2;
                uint32_t codeLen = readU32(cp); cp += 4;
                methods[i].bytecode.assign(cp, cp + codeLen);
                cp += codeLen;
                uint16_t exCount = readU16(cp); cp += 2;
                methods[i].exceptionTable.resize(exCount);
                for (uint16_t e = 0; e < exCount; e++) {
                    methods[i].exceptionTable[e].startPc = readU16(cp); cp += 2;
                    methods[i].exceptionTable[e].endPc = readU16(cp); cp += 2;
                    methods[i].exceptionTable[e].handlerPc = readU16(cp); cp += 2;
                    methods[i].exceptionTable[e].catchType = readU16(cp); cp += 2;
                }
                // Read FVM extension fields if present
                const uint8_t* attrEnd = p + attrLen;
                if (cp + 4 <= attrEnd) {
                    methods[i].constantPoolOffset = readU16(cp); cp += 2;
                    methods[i].constantPoolCount = readU16(cp); cp += 2;
                }
            } else if (attrName == "SourceFile") {
                methods[i].attributes[j].type = AttributeInfo::Type::SOURCE_FILE;
            } else {
                methods[i].attributes[j].type = AttributeInfo::Type::UNKNOWN;
            }

            methods[i].attributes[j].data.assign(p, p + attrLen);
            p += attrLen;
        }
    }

    // Class attributes
    if (p + 2 > end) return false;
    uint16_t classAttrCount = readU16(p); p += 2;
    attributes.clear();
    attributes.resize(classAttrCount);
    for (uint16_t i = 0; i < classAttrCount; i++) {
        if (p + 2 > end) return false;
        uint16_t nameIdx = readU16(p); p += 2;
        attributes[i].nameIndex = nameIdx;
        if (p + 4 > end) return false;
        uint32_t attrLen = readU32(p); p += 4;
        if (p + attrLen > end) return false;
        attributes[i].data.assign(p, p + attrLen);
        p += attrLen;
    }

    return true;
}

// ============================================================
// ClassFile::save — serialize to binary data
// ============================================================

bool ClassFile::save(std::vector<uint8_t>& data) const {
    data.clear();
    data.reserve(1024);

    // Magic
    writeU32(data, magic);

    // Version
    writeU16(data, minorVersion);
    writeU16(data, majorVersion);

    // Constant pool count (1-indexed)
    writeU16(data, (uint16_t)constantPool.size());

    for (size_t i = 1; i < constantPool.size(); i++) {
        auto& cp = constantPool[i];
        writeU8(data, (uint8_t)cp.tag);

        switch (cp.tag) {
            case CPTag::UTF8: {
                writeU16(data, (uint16_t)cp.data.size());
                data.insert(data.end(), cp.data.begin(), cp.data.end());
                break;
            }
            case CPTag::INTEGER:
            case CPTag::FLOAT:
            case CPTag::LONG:
            case CPTag::DOUBLE:
            case CPTag::BOOLEAN:
            case CPTag::CLASS:
            case CPTag::STRING:
            case CPTag::METHODREF:
            case CPTag::NAME_AND_TYPE:
            case CPTag::FORGE_PROPERTY:
            case CPTag::FORGE_METHOD:
                data.insert(data.end(), cp.data.begin(), cp.data.end());
                break;
        }
    }

    // Access flags, this class, super class
    writeU16(data, accessFlags);
    writeU16(data, thisClass);
    writeU16(data, superClass);

    // Interfaces
    writeU16(data, (uint16_t)interfaces.size());
    for (auto idx : interfaces) writeU16(data, idx);

    // Helper to write members
    auto writeMembers = [&](const std::vector<FieldInfo>& members) {
        writeU16(data, (uint16_t)members.size());
        for (auto& m : members) {
            writeU16(data, m.accessFlags);
            writeU16(data, m.nameIndex);
            writeU16(data, m.descriptorIndex);
            writeU16(data, (uint16_t)m.attributes.size());
            for (auto& attr : m.attributes) {
                writeU16(data, attr.nameIndex);
                writeU32(data, (uint32_t)attr.data.size());
                data.insert(data.end(), attr.data.begin(), attr.data.end());
            }
        }
    };

    // Fields
    writeMembers(fields);

    // Methods
    writeU16(data, (uint16_t)methods.size());
    for (auto& m : methods) {
        writeU16(data, m.accessFlags);
        writeU16(data, m.nameIndex);
        writeU16(data, m.descriptorIndex);
        writeU16(data, (uint16_t)m.attributes.size());
        for (auto& attr : m.attributes) {
            writeU16(data, attr.nameIndex);
            if (attr.type == AttributeInfo::Type::CODE) {
                // Build Code attribute data inline
                std::vector<uint8_t> codeData;
                writeU16(codeData, m.maxStack);
                writeU16(codeData, m.maxLocals);
                writeU16(codeData, m.arity);
                writeU16(codeData, m.upvalueCount);
                writeU32(codeData, (uint32_t)m.bytecode.size());
                codeData.insert(codeData.end(), m.bytecode.begin(), m.bytecode.end());
                writeU16(codeData, (uint16_t)m.exceptionTable.size());
                for (auto& e : m.exceptionTable) {
                    writeU16(codeData, e.startPc);
                    writeU16(codeData, e.endPc);
                    writeU16(codeData, e.handlerPc);
                    writeU16(codeData, e.catchType);
                }
                // FVM extension: per-method CP offset and count
                writeU16(codeData, m.constantPoolOffset);
                writeU16(codeData, m.constantPoolCount);
                writeU32(data, (uint32_t)codeData.size());
                data.insert(data.end(), codeData.begin(), codeData.end());
            } else {
                writeU32(data, (uint32_t)attr.data.size());
                data.insert(data.end(), attr.data.begin(), attr.data.end());
            }
        }
    }

    // Class attributes
    writeU16(data, (uint16_t)attributes.size());
    for (auto& attr : attributes) {
        writeU16(data, attr.nameIndex);
        writeU32(data, (uint32_t)attr.data.size());
        data.insert(data.end(), attr.data.begin(), attr.data.end());
    }

    return true;
}

} // namespace forge::fvm
