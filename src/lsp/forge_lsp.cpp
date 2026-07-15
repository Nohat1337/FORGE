#include "forge_lsp.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <set>

namespace forge::lsp {

// ============================================================
// Minimal JSON parser/generator (no external dependencies)
// ============================================================

class JsonValue {
public:
    enum Type { NUL, BOOL, NUMBER, STRING, ARRAY, OBJECT };

    JsonValue() : type_(NUL) {}
    JsonValue(bool b) : type_(BOOL), boolVal_(b) {}
    JsonValue(double n) : type_(NUMBER), numVal_(n) {}
    JsonValue(int n) : type_(NUMBER), numVal_(n) {}
    JsonValue(const std::string& s) : type_(STRING), strVal_(s) {}
    JsonValue(const char* s) : type_(STRING), strVal_(s) {}

    Type type() const { return type_; }

    bool isNull() const { return type_ == NUL; }
    bool isBool() const { return type_ == BOOL; }
    bool isNumber() const { return type_ == NUMBER; }
    bool isString() const { return type_ == STRING; }
    bool isArray() const { return type_ == ARRAY; }
    bool isObject() const { return type_ == OBJECT; }

    bool boolVal() const { return boolVal_; }
    double numVal() const { return numVal_; }
    int intVal() const { return static_cast<int>(numVal_); }
    const std::string& strVal() const { return strVal_; }
    const std::vector<JsonValue>& arrVal() const { return arrVal_; }
    const std::map<std::string, JsonValue>& objVal() const { return objVal_; }

    // Object access
    const JsonValue& operator[](const std::string& key) const {
        static JsonValue nullVal;
        auto it = objVal_.find(key);
        return (it != objVal_.end()) ? it->second : nullVal;
    }

    const JsonValue& operator[](const char* key) const {
        return (*this)[std::string(key)];
    }

    // Array access
    const JsonValue& operator[](int idx) const {
        static JsonValue nullVal;
        if (idx < 0 || idx >= (int)arrVal_.size()) return nullVal;
        return arrVal_[idx];
    }

    std::string getString(const std::string& key, const std::string& def = "") const {
        auto it = objVal_.find(key);
        if (it != objVal_.end() && it->second.isString()) return it->second.strVal();
        return def;
    }

    int getInt(const std::string& key, int def = 0) const {
        auto it = objVal_.find(key);
        if (it != objVal_.end() && it->second.isNumber()) return it->second.intVal();
        return def;
    }

    bool getBool(const std::string& key, bool def = false) const {
        auto it = objVal_.find(key);
        if (it != objVal_.end() && it->second.isBool()) return it->second.boolVal();
        return def;
    }

    // Build object
    static JsonValue makeObject() { JsonValue v; v.type_ = OBJECT; return v; }
    static JsonValue makeArray() { JsonValue v; v.type_ = ARRAY; return v; }

    void set(const std::string& key, JsonValue val) {
        type_ = OBJECT;
        objVal_[key] = val;
    }

    void push(JsonValue val) {
        type_ = ARRAY;
        arrVal_.push_back(val);
    }

    // Serialize to JSON string
    std::string stringify() const {
        switch (type_) {
            case NUL: return "null";
            case BOOL: return boolVal_ ? "true" : "false";
            case NUMBER: {
                if (numVal_ == static_cast<int>(numVal_) && numVal_ < 1e9 && numVal_ > -1e9)
                    return std::to_string(static_cast<int>(numVal_));
                std::ostringstream oss;
                oss << numVal_;
                return oss.str();
            }
            case STRING: return "\"" + escapeString(strVal_) + "\"";
            case ARRAY: {
                std::string s = "[";
                for (size_t i = 0; i < arrVal_.size(); i++) {
                    if (i > 0) s += ",";
                    s += arrVal_[i].stringify();
                }
                return s + "]";
            }
            case OBJECT: {
                std::string s = "{";
                bool first = true;
                for (auto& kv : objVal_) {
                    if (!first) s += ",";
                    s += "\"" + escapeString(kv.first) + "\":" + kv.second.stringify();
                    first = false;
                }
                return s + "}";
            }
        }
        return "null";
    }

    // Parse JSON from string
    static JsonValue parse(const std::string& json) {
        size_t pos = 0;
        JsonValue result = parseValue(json, pos);
        return result;
    }

private:
    Type type_ = NUL;
    bool boolVal_ = false;
    double numVal_ = 0;
    std::string strVal_;
    std::vector<JsonValue> arrVal_;
    std::map<std::string, JsonValue> objVal_;

    static std::string escapeString(const std::string& s) {
        std::string out;
        for (char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                case '\0': out += "\\0"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                        out += buf;
                    } else {
                        out += c;
                    }
            }
        }
        return out;
    }

    static void skipWhitespace(const std::string& json, size_t& pos) {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r'))
            pos++;
    }

    static JsonValue parseValue(const std::string& json, size_t& pos) {
        skipWhitespace(json, pos);
        if (pos >= json.size()) return JsonValue();
        char c = json[pos];
        if (c == '"') return parseString(json, pos);
        if (c == '{') return parseObject(json, pos);
        if (c == '[') return parseArray(json, pos);
        if (c == 't' || c == 'f') return parseBool(json, pos);
        if (c == 'n') return parseNull(json, pos);
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(json, pos);
        return JsonValue();
    }

    static JsonValue parseString(const std::string& json, size_t& pos) {
        pos++; // skip opening "
        std::string s;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\') {
                pos++;
                if (pos >= json.size()) break;
                switch (json[pos]) {
                    case '"': s += '"'; break;
                    case '\\': s += '\\'; break;
                    case '/': s += '/'; break;
                    case 'n': s += '\n'; break;
                    case 'r': s += '\r'; break;
                    case 't': s += '\t'; break;
                    case 'b': s += '\b'; break;
                    case 'f': s += '\f'; break;
                    case '0': s += '\0'; break;
                    case 'u': {
                        // Parse 4 hex digits
                        if (pos + 4 < json.size()) {
                            std::string hex = json.substr(pos + 1, 4);
                            unsigned int cp = std::stoul(hex, nullptr, 16);
                            pos += 4;
                            if (cp < 0x80) s += static_cast<char>(cp);
                            else if (cp < 0x800) {
                                s += static_cast<char>(0xC0 | (cp >> 6));
                                s += static_cast<char>(0x80 | (cp & 0x3F));
                            } else {
                                s += static_cast<char>(0xE0 | (cp >> 12));
                                s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                                s += static_cast<char>(0x80 | (cp & 0x3F));
                            }
                        }
                        break;
                    }
                    default: s += json[pos]; break;
                }
            } else {
                s += json[pos];
            }
            pos++;
        }
        if (pos < json.size()) pos++; // skip closing "
        return JsonValue(s);
    }

    static JsonValue parseNumber(const std::string& json, size_t& pos) {
        size_t start = pos;
        if (json[pos] == '-') pos++;
        while (pos < json.size() && std::isdigit(json[pos])) pos++;
        if (pos < json.size() && json[pos] == '.') {
            pos++;
            while (pos < json.size() && std::isdigit(json[pos])) pos++;
        }
        if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
            pos++;
            if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) pos++;
            while (pos < json.size() && std::isdigit(json[pos])) pos++;
        }
        std::string numStr = json.substr(start, pos - start);
        return JsonValue(std::stod(numStr));
    }

    static JsonValue parseBool(const std::string& json, size_t& pos) {
        if (json.substr(pos, 4) == "true") { pos += 4; return JsonValue(true); }
        if (json.substr(pos, 5) == "false") { pos += 5; return JsonValue(false); }
        return JsonValue();
    }

    static JsonValue parseNull(const std::string& json, size_t& pos) {
        if (json.substr(pos, 4) == "null") { pos += 4; }
        return JsonValue();
    }

    static JsonValue parseArray(const std::string& json, size_t& pos) {
        pos++; // skip [
        JsonValue arr = JsonValue::makeArray();
        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ']') { pos++; return arr; }
        while (pos < json.size()) {
            arr.push(parseValue(json, pos));
            skipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == ',') { pos++; continue; }
            if (pos < json.size() && json[pos] == ']') { pos++; break; }
            break;
        }
        return arr;
    }

    static JsonValue parseObject(const std::string& json, size_t& pos) {
        pos++; // skip {
        JsonValue obj = JsonValue::makeObject();
        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == '}') { pos++; return obj; }
        while (pos < json.size()) {
            skipWhitespace(json, pos);
            JsonValue key = parseString(json, pos);
            skipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == ':') pos++;
            skipWhitespace(json, pos);
            obj.set(key.strVal(), parseValue(json, pos));
            skipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == ',') { pos++; continue; }
            if (pos < json.size() && json[pos] == '}') { pos++; break; }
            break;
        }
        return obj;
    }
};

// ============================================================
// ForgeLSP Implementation
// ============================================================

ForgeLSP::ForgeLSP() {}

// --- Position / Offset helpers ---

int ForgeLSP::positionToOffset(const std::string& source, Position pos) {
    int line = 0;
    int offset = 0;
    for (size_t i = 0; i < source.size(); i++) {
        if (line == pos.line && (offset - (i == 0 ? 0 : 0)) == pos.character)
            return i;
        if (line == pos.line) {
            // We've reached the right line, now advance by character count
        }
        if (source[i] == '\n') {
            line++;
            offset = 0;
        } else {
            offset++;
        }
    }
    // Recompute simply
    int curLine = 0;
    int curCol = 0;
    for (size_t i = 0; i <= source.size(); i++) {
        if (curLine == pos.line && curCol == pos.character)
            return static_cast<int>(i);
        if (i >= source.size()) break;
        if (source[i] == '\n') {
            curLine++;
            curCol = 0;
        } else {
            curCol++;
        }
    }
    return static_cast<int>(source.size());
}

Position ForgeLSP::offsetToPosition(const std::string& source, int offset) {
    Position pos;
    pos.line = 0;
    pos.character = 0;
    for (int i = 0; i < offset && i < (int)source.size(); i++) {
        if (source[i] == '\n') {
            pos.line++;
            pos.character = 0;
        } else {
            pos.character++;
        }
    }
    return pos;
}

int ForgeLSP::getLineStart(const std::string& source, int line) {
    int curLine = 0;
    int offset = 0;
    for (size_t i = 0; i <= source.size(); i++) {
        if (curLine == line) return offset;
        if (i >= source.size()) break;
        if (source[i] == '\n') {
            curLine++;
            offset = static_cast<int>(i) + 1;
        }
    }
    return offset;
}

std::string ForgeLSP::getWordAt(const std::string& source, Position pos) {
    int offset = positionToOffset(source, pos);
    if (offset < 0 || offset >= (int)source.size()) return "";
    // Expand left
    int start = offset;
    while (start > 0 && (std::isalnum(source[start - 1]) || source[start - 1] == '_'))
        start--;
    // Expand right
    int end = offset;
    while (end < (int)source.size() && (std::isalnum(source[end]) || source[end] == '_'))
        end++;
    return source.substr(start, end - start);
}

// ============================================================
// Minimal Forge tokenizer for source analysis
// ============================================================

struct ForgeToken {
    enum Type {
        TOK_IDENT, TOK_NUMBER, TOK_STRING, TOK_LPAREN, TOK_RPAREN,
        TOK_LBRACE, TOK_RBRACE, TOK_LBRACKET, TOK_RBRACKET,
        TOK_SEMICOLON, TOK_COLON, TOK_COMMA, TOK_DOT, TOK_ARROW,
        TOK_ASSIGN, TOK_EQUAL, TOK_PLUS, TOK_MINUS, TOK_STAR,
        TOK_SLASH, TOK_PERCENT, TOK_LESS, TOK_GREATER,
        TOK_BANG, TOK_QUESTION, TOK_AND, TOK_OR,
        TOK_EOF, TOK_NEWLINE, TOK_UNKNOWN
    };
    Type type;
    std::string value;
    int line;
    int column;
};

static std::vector<ForgeToken> tokenizeForge(const std::string& source) {
    std::vector<ForgeToken> tokens;
    int line = 1;
    int col = 1;
    size_t pos = 0;

    auto advance = [&]() -> char {
        char c = source[pos++];
        if (c == '\n') { line++; col = 1; } else { col++; }
        return c;
    };
    auto peek = [&]() -> char {
        return pos < source.size() ? source[pos] : '\0';
    };
    auto peekNext = [&]() -> char {
        return pos + 1 < source.size() ? source[pos + 1] : '\0';
    };

    while (pos < source.size()) {
        char c = peek();

        // Skip whitespace
        if (c == ' ' || c == '\t' || c == '\r') { advance(); continue; }

        // Skip line comments
        if (c == '/' && peekNext() == '/') {
            while (pos < source.size() && peek() != '\n') advance();
            continue;
        }

        // Skip block comments
        if (c == '/' && peekNext() == '*') {
            advance(); advance();
            while (pos < source.size()) {
                if (peek() == '*' && peekNext() == '/') { advance(); advance(); break; }
                advance();
            }
            continue;
        }

        int startLine = line;
        int startCol = col;

        // Newline
        if (c == '\n') {
            advance();
            tokens.push_back({ForgeToken::TOK_NEWLINE, "\\n", startLine, startCol});
            continue;
        }

        // Numbers
        if (std::isdigit(c)) {
            std::string num;
            while (pos < source.size() && (std::isdigit(peek()) || peek() == '.')) num += advance();
            tokens.push_back({ForgeToken::TOK_NUMBER, num, startLine, startCol});
            continue;
        }

        // Strings
        if (c == '"') {
            advance();
            std::string str;
            while (pos < source.size() && peek() != '"') {
                if (peek() == '\\') { advance(); }
                str += advance();
            }
            if (pos < source.size()) advance(); // closing "
            tokens.push_back({ForgeToken::TOK_STRING, str, startLine, startCol});
            continue;
        }

        // Char literals
        if (c == '\'') {
            advance();
            if (peek() == '\\') advance();
            if (pos < source.size()) advance();
            if (pos < source.size() && peek() == '\'') advance();
            tokens.push_back({ForgeToken::TOK_STRING, "", startLine, startCol});
            continue;
        }

        // Identifiers / keywords
        if (std::isalpha(c) || c == '_') {
            std::string id;
            while (pos < source.size() && (std::isalnum(peek()) || peek() == '_'))
                id += advance();
            tokens.push_back({ForgeToken::TOK_IDENT, id, startLine, startCol});
            continue;
        }

        // Symbols
        advance();
        switch (c) {
            case '(': tokens.push_back({ForgeToken::TOK_LPAREN, "(", startLine, startCol}); break;
            case ')': tokens.push_back({ForgeToken::TOK_RPAREN, ")", startLine, startCol}); break;
            case '{': tokens.push_back({ForgeToken::TOK_LBRACE, "{", startLine, startCol}); break;
            case '}': tokens.push_back({ForgeToken::TOK_RBRACE, "}", startLine, startCol}); break;
            case '[': tokens.push_back({ForgeToken::TOK_LBRACKET, "[", startLine, startCol}); break;
            case ']': tokens.push_back({ForgeToken::TOK_RBRACKET, "]", startLine, startCol}); break;
            case ';': tokens.push_back({ForgeToken::TOK_SEMICOLON, ";", startLine, startCol}); break;
            case ':': tokens.push_back({ForgeToken::TOK_COLON, ":", startLine, startCol}); break;
            case ',': tokens.push_back({ForgeToken::TOK_COMMA, ",", startLine, startCol}); break;
            case '.': tokens.push_back({ForgeToken::TOK_DOT, ".", startLine, startCol}); break;
            case '?': tokens.push_back({ForgeToken::TOK_QUESTION, "?", startLine, startCol}); break;
            case '+': tokens.push_back({ForgeToken::TOK_PLUS, "+", startLine, startCol}); break;
            case '*': tokens.push_back({ForgeToken::TOK_STAR, "*", startLine, startCol}); break;
            case '/': tokens.push_back({ForgeToken::TOK_SLASH, "/", startLine, startCol}); break;
            case '%': tokens.push_back({ForgeToken::TOK_PERCENT, "%", startLine, startCol}); break;
            case '=':
                if (peek() == '=') { advance(); tokens.push_back({ForgeToken::TOK_EQUAL, "==", startLine, startCol}); }
                else tokens.push_back({ForgeToken::TOK_ASSIGN, "=", startLine, startCol});
                break;
            case '!':
                if (peek() == '=') { advance(); tokens.push_back({ForgeToken::TOK_BANG, "!=", startLine, startCol}); }
                else tokens.push_back({ForgeToken::TOK_BANG, "!", startLine, startCol});
                break;
            case '<':
                if (peek() == '=') { advance(); }
                tokens.push_back({ForgeToken::TOK_LESS, "<", startLine, startCol});
                break;
            case '>':
                if (peek() == '=') { advance(); }
                tokens.push_back({ForgeToken::TOK_GREATER, ">", startLine, startCol});
                break;
            case '-':
                if (peek() == '>') { advance(); tokens.push_back({ForgeToken::TOK_ARROW, "->", startLine, startCol}); }
                else tokens.push_back({ForgeToken::TOK_MINUS, "-", startLine, startCol});
                break;
            case '&':
                if (peek() == '&') { advance(); }
                tokens.push_back({ForgeToken::TOK_AND, "&", startLine, startCol});
                break;
            case '|':
                if (peek() == '|') { advance(); }
                tokens.push_back({ForgeToken::TOK_OR, "|", startLine, startCol});
                break;
            default:
                tokens.push_back({ForgeToken::TOK_UNKNOWN, std::string(1, c), startLine, startCol});
                break;
        }
    }
    tokens.push_back({ForgeToken::TOK_EOF, "", line, col});
    return tokens;
}

// ============================================================
// Source Analysis
// ============================================================

static const std::set<std::string> forgeKeywords = {
    "let", "const", "fn", "return", "if", "else", "while", "for",
    "class", "extends", "this", "super", "new", "import", "as",
    "match", "case", "throw", "try", "catch", "finally", "gen",
    "yield", "break", "continue", "struct", "enum", "impl", "extern",
    "true", "false", "nil", "and", "or", "not", "in", "from"
};

static const std::set<std::string> forgeBuiltinFunctions = {
    "print", "len", "push", "str", "int", "float", "type", "error",
    "keys", "values", "has", "entries", "clone", "upper", "lower",
    "trim", "split", "contains", "replace", "substring", "charAt",
    "parseInt", "abs", "min", "max", "sqrt", "pow", "floor", "ceil",
    "round", "random", "randomInt"
};

static const std::set<std::string> forgeTypeKeywords = {
    "int", "float", "string", "bool", "nil", "any", "array", "map"
};

// Collect variable/function definitions from source using basic scope tracking
struct SymbolDef {
    std::string name;
    int line;        // 1-based line number of definition
    int startCol;    // 0-based column
    int endCol;      // 0-based column (exclusive)
    std::string kind; // "variable", "function", "parameter", "class", "struct", "enum"
    std::string typeHint;
};

static std::vector<SymbolDef> collectSymbols(const std::string& source) {
    std::vector<SymbolDef> symbols;
    auto tokens = tokenizeForge(source);

    for (size_t i = 0; i < tokens.size(); i++) {
        auto& tok = tokens[i];

        // let / const variable declarations
        if (tok.value == "let" || tok.value == "const") {
            // let name = ... or let name: Type = ...
            if (i + 1 < tokens.size() && tokens[i + 1].type == ForgeToken::TOK_IDENT) {
                std::string varName = tokens[i + 1].value;
                int vLine = tokens[i + 1].line;
                int vCol = tokens[i + 1].column - 1; // to 0-based
                std::string hint;
                size_t j = i + 2;
                if (j < tokens.size() && tokens[j].type == ForgeToken::TOK_COLON && j + 1 < tokens.size()) {
                    hint = tokens[j + 1].value;
                    j += 2;
                }
                symbols.push_back({varName, vLine, vCol, vCol + (int)varName.size(), "variable", hint});
            }
        }

        // fn declarations
        if (tok.value == "fn") {
            if (i + 1 < tokens.size() && tokens[i + 1].type == ForgeToken::TOK_IDENT) {
                std::string fnName = tokens[i + 1].value;
                int fLine = tokens[i + 1].line;
                int fCol = tokens[i + 1].column - 1;
                symbols.push_back({fnName, fLine, fCol, fCol + (int)fnName.size(), "function", ""});
                // Also record parameters
                size_t j = i + 2;
                if (j < tokens.size() && tokens[j].type == ForgeToken::TOK_LPAREN) {
                    j++;
                    while (j < tokens.size() && tokens[j].type != ForgeToken::TOK_RBRACE) {
                        if (tokens[j].type == ForgeToken::TOK_IDENT && tokens[j].value != "fn") {
                            // Check if it's a parameter (followed by , or ) or :)
                            if (j + 1 < tokens.size() &&
                                (tokens[j + 1].type == ForgeToken::TOK_COMMA ||
                                 tokens[j + 1].type == ForgeToken::TOK_RPAREN ||
                                 tokens[j + 1].type == ForgeToken::TOK_COLON)) {
                                symbols.push_back({tokens[j].value, tokens[j].line, tokens[j].column - 1,
                                    tokens[j].column - 1 + (int)tokens[j].value.size(), "parameter", ""});
                            }
                        }
                        j++;
                    }
                }
            }
        }

        // class declarations
        if (tok.value == "class" && i + 1 < tokens.size() && tokens[i + 1].type == ForgeToken::TOK_IDENT) {
            std::string className = tokens[i + 1].value;
            int cLine = tokens[i + 1].line;
            int cCol = tokens[i + 1].column - 1;
            symbols.push_back({className, cLine, cCol, cCol + (int)className.size(), "class", ""});
        }

        // struct declarations
        if (tok.value == "struct" && i + 1 < tokens.size() && tokens[i + 1].type == ForgeToken::TOK_IDENT) {
            std::string structName = tokens[i + 1].value;
            int sLine = tokens[i + 1].line;
            int sCol = tokens[i + 1].column - 1;
            symbols.push_back({structName, sLine, sCol, sCol + (int)structName.size(), "struct", ""});
        }

        // enum declarations
        if (tok.value == "enum" && i + 1 < tokens.size() && tokens[i + 1].type == ForgeToken::TOK_IDENT) {
            std::string enumName = tokens[i + 1].value;
            int eLine = tokens[i + 1].line;
            int eCol = tokens[i + 1].column - 1;
            symbols.push_back({enumName, eLine, eCol, eCol + (int)enumName.size(), "enum", ""});
        }
    }
    return symbols;
}

std::vector<Diagnostic> ForgeLSP::analyzeSource(const std::string& source, const std::string& uri) {
    std::vector<Diagnostic> diagnostics;
    auto tokens = tokenizeForge(source);
    auto symbols = collectSymbols(source);

    // Track defined variable names for scope checking
    std::set<std::string> definedVars;
    // Add built-in function names so they aren't flagged
    for (auto& b : forgeBuiltinFunctions) definedVars.insert(b);

    int braceDepth = 0;
    int parenDepth = 0;
    int bracketDepth = 0;

    // Track if we're inside a string or block comment
    bool inBlockComment = false;

    for (size_t i = 0; i < tokens.size(); i++) {
        auto& tok = tokens[i];

        if (tok.type == ForgeToken::TOK_LBRACE) braceDepth++;
        if (tok.type == ForgeToken::TOK_RBRACE) {
            braceDepth--;
            if (braceDepth < 0) {
                Diagnostic d;
                d.range.start = {tok.line - 1, tok.column - 1};
                d.range.end = {tok.line - 1, tok.column};
                d.severity = 1;
                d.message = "Unmatched closing brace '}'";
                diagnostics.push_back(d);
                braceDepth = 0;
            }
        }
        if (tok.type == ForgeToken::TOK_LPAREN) parenDepth++;
        if (tok.type == ForgeToken::TOK_RPAREN) {
            parenDepth--;
            if (parenDepth < 0) {
                Diagnostic d;
                d.range.start = {tok.line - 1, tok.column - 1};
                d.range.end = {tok.line - 1, tok.column};
                d.severity = 1;
                d.message = "Unmatched closing parenthesis ')'";
                diagnostics.push_back(d);
                parenDepth = 0;
            }
        }
        if (tok.type == ForgeToken::TOK_LBRACKET) bracketDepth++;
        if (tok.type == ForgeToken::TOK_RBRACKET) {
            bracketDepth--;
            if (bracketDepth < 0) {
                Diagnostic d;
                d.range.start = {tok.line - 1, tok.column - 1};
                d.range.end = {tok.line - 1, tok.column};
                d.severity = 1;
                d.message = "Unmatched closing bracket ']'";
                diagnostics.push_back(d);
                bracketDepth = 0;
            }
        }

        // Collect variables as we encounter definitions
        if ((tok.value == "let" || tok.value == "const") && i + 1 < tokens.size()) {
            definedVars.insert(tokens[i + 1].value);
        }
        if (tok.value == "fn" && i + 1 < tokens.size() && tokens[i + 1].type == ForgeToken::TOK_IDENT) {
            definedVars.insert(tokens[i + 1].value);
        }
        if ((tok.value == "class" || tok.value == "struct" || tok.value == "enum")
            && i + 1 < tokens.size() && tokens[i + 1].type == ForgeToken::TOK_IDENT) {
            definedVars.insert(tokens[i + 1].value);
        }
        // for loop variable
        if (tok.value == "for") {
            // for name in ...
            for (size_t j = i + 1; j < std::min(i + 6, tokens.size()); j++) {
                if (tokens[j].value == "in" && j > i + 1) {
                    if (tokens[j - 1].type == ForgeToken::TOK_IDENT) {
                        definedVars.insert(tokens[j - 1].value);
                    }
                    break;
                }
            }
        }
    }

    // Check for unclosed constructs at end of file
    if (braceDepth > 0) {
        Diagnostic d;
        Position endPos = offsetToPosition(source, (int)source.size());
        d.range.start = endPos;
        d.range.end = endPos;
        d.severity = 2; // warning
        d.message = "Unclosed braces: " + std::to_string(braceDepth) + " '{' still open";
        diagnostics.push_back(d);
    }
    if (parenDepth > 0) {
        Diagnostic d;
        Position endPos = offsetToPosition(source, (int)source.size());
        d.range.start = endPos;
        d.range.end = endPos;
        d.severity = 2;
        d.message = "Unclosed parentheses: " + std::to_string(parenDepth) + " '(' still open";
        diagnostics.push_back(d);
    }
    if (bracketDepth > 0) {
        Diagnostic d;
        Position endPos = offsetToPosition(source, (int)source.size());
        d.range.start = endPos;
        d.range.end = endPos;
        d.severity = 2;
        d.message = "Unclosed brackets: " + std::to_string(bracketDepth) + " '[' still open";
        diagnostics.push_back(d);
    }

    // Check for undefined variables (basic)
    for (size_t i = 0; i < tokens.size(); i++) {
        auto& tok = tokens[i];
        if (tok.type != ForgeToken::TOK_IDENT) continue;
        if (forgeKeywords.count(tok.value)) continue;
        if (forgeBuiltinFunctions.count(tok.value)) continue;
        if (forgeTypeKeywords.count(tok.value)) continue;
        if (tok.value == "this" || tok.value == "super" || tok.value == "nil") continue;
        if (tok.value == "true" || tok.value == "false") continue;

        // Skip if it's a function/class/struct definition name
        bool isDef = false;
        for (auto& sym : symbols) {
            if (sym.name == tok.value && sym.line == tok.line) { isDef = true; break; }
        }
        if (isDef) continue;

        // Skip if used after a dot (property access)
        if (i > 0 && tokens[i - 1].type == ForgeToken::TOK_DOT) continue;

        // Skip if used as module name before :: or .
        if (i + 1 < tokens.size() && (tokens[i + 1].type == ForgeToken::TOK_COLON)) continue;

        // Skip import-related identifiers
        if (tok.value == "as" || tok.value == "from" || tok.value == "import") continue;
        // Skip if previous token is import
        for (int j = (int)i - 1; j >= 0 && j >= (int)i - 3; j--) {
            if (tokens[j].value == "import" || tokens[j].value == "from") {
                isDef = true;
                break;
            }
        }
        if (isDef) continue;

        // Skip if it follows a fn keyword (already handled above)
        // Check if undefined
        if (!definedVars.count(tok.value)) {
            // Only report if it's used in an expression context (not a declaration)
            bool skip = false;
            // Skip struct field names in class definitions
            for (size_t j = i + 1; j < tokens.size() && j < i + 3; j++) {
                if (tokens[j].type == ForgeToken::TOK_COLON || tokens[j].type == ForgeToken::TOK_ASSIGN) {
                    // Could be a field declaration
                }
            }
            if (!skip) {
                Diagnostic d;
                d.range.start = {tok.line - 1, tok.column - 1};
                d.range.end = {tok.line - 1, tok.column - 1 + (int)tok.value.size()};
                d.severity = 2; // warning (not error, since many false positives possible)
                d.message = "Possibly undefined variable '" + tok.value + "'";
                diagnostics.push_back(d);
            }
        }
    }

    return diagnostics;
}

// ============================================================
// Completions
// ============================================================

std::vector<CompletionItem> ForgeLSP::getCompletions(const std::string& source, Position pos) {
    std::vector<CompletionItem> items;
    std::string prefix = getWordAt(source, pos);
    std::string lowerPrefix = prefix;
    std::transform(lowerPrefix.begin(), lowerPrefix.end(), lowerPrefix.begin(), ::tolower);

    auto symbols = collectSymbols(source);

    // Determine context: are we after a dot? (property access)
    int offset = positionToOffset(source, pos);
    bool afterDot = false;
    std::string objectName;
    if (offset > 0) {
        // Look backwards for non-whitespace
        int lookback = offset - 1;
        while (lookback >= 0 && (source[lookback] == ' ' || source[lookback] == '\t')) lookback--;
        if (lookback >= 0 && source[lookback] == '.') {
            afterDot = true;
            // Get the object name before the dot
            int objEnd = lookback;
            while (objEnd > 0 && (std::isalnum(source[objEnd - 1]) || source[objEnd - 1] == '_'))
                objEnd--;
            objectName = source.substr(objEnd, lookback - objEnd);
        }
    }

    if (afterDot) {
        // Module/property completions
        static const std::map<std::string, std::vector<std::pair<std::string, std::string>>> moduleMembers = {
            {"io", {{"read", "function"}, {"write", "function"}, {"readLine", "function"},
                     {"print", "function"}, {"println", "function"}, {"format", "function"}}},
            {"os", {{"env", "function"}, {"exit", "function"}, {"sleep", "function"},
                     {"execute", "function"}, {"clock", "function"}}},
            {"json", {{"parse", "function"}, {"stringify", "function"}, {"prettyPrint", "function"}}},
            {"path", {{"join", "function"}, {"dirname", "function"}, {"basename", "function"},
                       {"exists", "function"}, {"extension", "function"}}},
            {"fs", {{"read", "function"}, {"write", "function"}, {"exists", "function"},
                     {"remove", "function"}, {"mkdir", "function"}, {"list", "function"}}},
            {"system", {{"platform", "function"}, {"arch", "function"}, {"args", "function"},
                         {"env", "function"}}},
            {"test", {{"assert", "function"}, {"assertEquals", "function"}, {"assertThrows", "function"},
                       {"describe", "function"}, {"it", "function"}, {"beforeEach", "function"}}},
            {"concurrent", {{"spawn", "function"}, {"wait", "function"}, {"async", "function"},
                              {"channel", "function"}, {"mutex", "function"}}},
            {"net", {{"httpGet", "function"}, {"httpPost", "function"}, {"connect", "function"},
                       {"listen", "function"}}},
            {"ffi", {{"cdef", "function"}, {"load", "function"}, {"call", "function"}}},
            {"llm", {{"complete", "function"}, {"embed", "function"}}},
        };

        auto it = moduleMembers.find(objectName);
        if (it != moduleMembers.end()) {
            for (auto& [name, kind] : it->second) {
                if (prefix.empty() || name.find(lowerPrefix) == 0) {
                    CompletionItem item;
                    item.label = name;
                    item.kind = (kind == "function") ? 3 : 5;
                    item.detail = kind;
                    item.insertText = name;
                    items.push_back(item);
                }
            }
        }

        // String/array method completions
        if (objectName == "string" || objectName == "str") {
            std::vector<std::pair<std::string, std::string>> stringMethods = {
                {"upper", "(string -> string)"},
                {"lower", "(string -> string)"},
                {"trim", "(string -> string)"},
                {"split", "(string, string -> array)"},
                {"contains", "(string -> bool)"},
                {"replace", "(string, string -> string)"},
                {"substring", "(int, int -> string)"},
                {"charAt", "(int -> string)"},
                {"len", "(int)"},
                {"startsWith", "(string -> bool)"},
                {"endsWith", "(string -> bool)"},
                {"indexOf", "(string -> int)"},
                {"reverse", "(string -> string)"},
            };
            for (auto& [name, sig] : stringMethods) {
                if (prefix.empty() || name.find(lowerPrefix) == 0) {
                    CompletionItem item;
                    item.label = name;
                    item.kind = 3;
                    item.detail = sig;
                    item.insertText = name;
                    items.push_back(item);
                }
            }
        }

        return items;
    }

    // Keywords
    for (auto& kw : keywords_) {
        if (prefix.empty() || kw.find(lowerPrefix) == 0) {
            CompletionItem item;
            item.label = kw;
            item.kind = 14; // Keyword
            item.detail = "keyword";
            item.insertText = kw;
            items.push_back(item);
        }
    }

    // Built-in functions
    for (auto& fn : builtins_) {
        if (prefix.empty() || fn.find(lowerPrefix) == 0) {
            CompletionItem item;
            item.label = fn;
            item.kind = 3; // Function
            item.detail = "builtin function";
            item.insertText = fn;
            items.push_back(item);
        }
    }

    // Module names (after import)
    for (auto& mod : modules_) {
        if (prefix.empty() || mod.find(lowerPrefix) == 0) {
            CompletionItem item;
            item.label = mod;
            item.kind = 9; // Module
            item.detail = "module";
            item.insertText = mod;
            items.push_back(item);
        }
    }

    // Type keywords
    for (auto& tk : forgeTypeKeywords) {
        if (prefix.empty() || tk.find(lowerPrefix) == 0) {
            CompletionItem item;
            item.label = tk;
            item.kind = 22; // TypeParameter (closest to type)
            item.detail = "type";
            item.insertText = tk;
            items.push_back(item);
        }
    }

    // Local symbols from source
    for (auto& sym : symbols) {
        if (prefix.empty() || sym.name.find(lowerPrefix) == 0) {
            CompletionItem item;
            item.label = sym.name;
            if (sym.kind == "function") item.kind = 3;
            else if (sym.kind == "class") item.kind = 4;
            else if (sym.kind == "struct") item.kind = 23; // Struct
            else if (sym.kind == "enum") item.kind = 13; // Enum
            else if (sym.kind == "parameter") item.kind = 6;
            else item.kind = 6; // Variable
            item.detail = sym.kind;
            if (!sym.typeHint.empty()) item.detail += ": " + sym.typeHint;
            item.insertText = sym.name;
            items.push_back(item);
        }
    }

    return items;
}

// ============================================================
// Hover
// ============================================================

HoverResult ForgeLSP::getHover(const std::string& source, Position pos) {
    HoverResult result;
    std::string word = getWordAt(source, pos);
    if (word.empty()) return result;

    int offset = positionToOffset(source, pos);
    result.range.start = offsetToPosition(source, offset - (int)word.size());
    result.range.end = pos;

    auto symbols = collectSymbols(source);

    // Check keywords
    if (forgeKeywords.count(word)) {
        result.contents = "**" + word + "** (keyword)\n\nForge language keyword.";
        return result;
    }

    // Check builtins
    if (forgeBuiltinFunctions.count(word)) {
        result.contents = "**" + word + "** (builtin function)\n\nBuilt-in Forge function.";
        return result;
    }

    // Check type keywords
    if (forgeTypeKeywords.count(word)) {
        result.contents = "**" + word + "** (type)\n\nForge primitive type.";
        return result;
    }

    // Check symbols
    for (auto& sym : symbols) {
        if (sym.name == word) {
            std::string md = "**" + sym.name + "** (" + sym.kind + ")";
            if (!sym.typeHint.empty()) md += ": `" + sym.typeHint + "`";
            md += "\n\nDefined at line " + std::to_string(sym.line);
            result.contents = md;
            return result;
        }
    }

    // Check module names
    for (auto& mod : modules_) {
        if (mod == word) {
            result.contents = "**" + mod + "** (module)\n\nForge standard library module.\n\n`import " + mod + "`";
            return result;
        }
    }

    // Check for dot-access context (e.g., io.print)
    if (offset > 0) {
        int lookback = offset - 1;
        while (lookback >= 0 && (source[lookback] == ' ' || source[lookback] == '\t')) lookback--;
        if (lookback >= 0 && source[lookback] == '.') {
            // Find the module name before the dot
            int objEnd = lookback;
            while (objEnd > 0 && (std::isalnum(source[objEnd - 1]) || source[objEnd - 1] == '_'))
                objEnd--;
            std::string moduleName = source.substr(objEnd, lookback - objEnd);
            result.contents = "**" + moduleName + "." + word + "** (module member)\n\nMember of the `" + moduleName + "` module.";
            result.range.start = offsetToPosition(source, offset - (int)word.size());
            result.range.end = pos;
            return result;
        }
    }

    result.contents = "**" + word + "**";
    return result;
}

// ============================================================
// Definition
// ============================================================

std::vector<Range> ForgeLSP::findDefinition(const std::string& source, const std::string& word) {
    std::vector<Range> results;
    if (word.empty()) return results;

    auto symbols = collectSymbols(source);
    for (auto& sym : symbols) {
        if (sym.name == word) {
            Range r;
            r.start = {sym.line - 1, sym.startCol};
            r.end = {sym.line - 1, sym.endCol};
            results.push_back(r);
        }
    }
    return results;
}

// ============================================================
// References
// ============================================================

std::vector<Range> ForgeLSP::findReferences(const std::string& source, const std::string& word) {
    std::vector<Range> results;
    if (word.empty()) return results;

    auto tokens = tokenizeForge(source);
    for (auto& tok : tokens) {
        if (tok.type == ForgeToken::TOK_IDENT && tok.value == word) {
            Range r;
            r.start = {tok.line - 1, tok.column - 1};
            r.end = {tok.line - 1, tok.column - 1 + (int)word.size()};
            results.push_back(r);
        }
    }
    return results;
}

// ============================================================
// Main Loop (JSON-RPC over stdin/stdout)
// ============================================================

void ForgeLSP::run() {
    std::istream& stdin_stream = std::cin;
    std::string buffer;

    while (true) {
        // Read Content-Length header
        std::string header;
        int contentLength = -1;
        bool headersComplete = false;

        while (std::getline(stdin_stream, header)) {
            // Remove \r if present
            if (!header.empty() && header.back() == '\r')
                header.pop_back();

            if (header.empty()) {
                headersComplete = true;
                break;
            }

            // Parse Content-Length
            if (header.compare(0, 15, "Content-Length: ") == 0) {
                contentLength = std::stoi(header.substr(15));
            }
        }

        // Check for EOF
        if (!headersComplete && stdin_stream.eof()) break;

        if (contentLength < 0) continue;

        // Read the JSON body
        std::string body;
        body.resize(contentLength);
        int totalRead = 0;
        while (totalRead < contentLength) {
            stdin_stream.read(&body[totalRead], contentLength - totalRead);
            int bytesRead = static_cast<int>(stdin_stream.gcount());
            if (bytesRead == 0) break;
            totalRead += bytesRead;
        }
        body.resize(totalRead);

        if (body.empty()) continue;

        // Parse the JSON-RPC message
        JsonValue msg;
        try {
            msg = JsonValue::parse(body);
        } catch (...) {
            continue;
        }

        if (!msg.isObject()) continue;

        std::string method = msg.getString("method");
        int id = -1;
        if (msg.objVal().find("id") != msg.objVal().end() && !msg["id"].isNull()) {
            id = msg["id"].intVal();
        }

        std::string paramsStr;
        if (msg.objVal().find("params") != msg.objVal().end()) {
            paramsStr = msg["params"].stringify();
        }

        // Dispatch
        if (id >= 0) {
            // Request (has id)
            handleRequest(method, id, paramsStr);
        } else {
            // Notification (no id)
            handleNotification(method, paramsStr);
        }
    }
}

void ForgeLSP::handleRequest(const std::string& method, int id, const std::string& params) {
    std::string result;

    if (method == "initialize") {
        result = handleInitialize(id, params);
    } else if (method == "textDocument/completion") {
        result = handleCompletion(id, params);
    } else if (method == "textDocument/hover") {
        result = handleHover(id, params);
    } else if (method == "textDocument/definition") {
        result = handleDefinition(id, params);
    } else if (method == "textDocument/references") {
        result = handleReferences(id, params);
    } else {
        sendError(id, -32601, "Method not found: " + method);
        return;
    }

    sendResponse(id, result);
}

void ForgeLSP::handleNotification(const std::string& method, const std::string& params) {
    if (method == "initialized") {
        handleInitialized(params);
    } else if (method == "textDocument/didOpen") {
        handleDidOpen(params);
    } else if (method == "textDocument/didChange") {
        handleDidChange(params);
    } else if (method == "textDocument/didSave") {
        handleDidSave(params);
    } else if (method == "shutdown") {
        // Respond to shutdown if it arrives as notification
    } else if (method == "exit") {
        std::exit(0);
    }
}

// ============================================================
// JSON-RPC Transport
// ============================================================

void ForgeLSP::sendResponse(int id, const std::string& result) {
    JsonValue response = JsonValue::makeObject();
    response.set("jsonrpc", JsonValue("2.0"));
    response.set("id", JsonValue(id));
    response.set("result", JsonValue::parse(result));

    std::string body = response.stringify();
    std::cout << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    std::cout.flush();
}

void ForgeLSP::sendError(int id, int code, const std::string& message) {
    JsonValue response = JsonValue::makeObject();
    response.set("jsonrpc", JsonValue("2.0"));
    response.set("id", JsonValue(id));

    JsonValue error = JsonValue::makeObject();
    error.set("code", JsonValue(code));
    error.set("message", JsonValue(message));
    response.set("error", error);

    std::string body = response.stringify();
    std::cout << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    std::cout.flush();
}

void ForgeLSP::sendNotification(const std::string& method, const std::string& params) {
    JsonValue notification = JsonValue::makeObject();
    notification.set("jsonrpc", JsonValue("2.0"));
    notification.set("method", JsonValue(method));
    if (!params.empty()) {
        notification.set("params", JsonValue::parse(params));
    }

    std::string body = notification.stringify();
    std::cout << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    std::cout.flush();
}

// ============================================================
// LSP Method Handlers
// ============================================================

std::string ForgeLSP::handleInitialize(int id, const std::string& params) {
    JsonValue result = JsonValue::makeObject();

    JsonValue capabilities = JsonValue::makeObject();

    // Text document sync
    JsonValue textDocSync = JsonValue::makeObject();
    textDocSync.set("openClose", JsonValue(true));
    textDocSync.set("change", JsonValue(1)); // Full sync
    textDocSync.set("save", JsonValue(true));
    capabilities.set("textDocumentSync", textDocSync);

    // Completion
    JsonValue completionProvider = JsonValue::makeObject();
    completionProvider.set("triggerCharacters", JsonValue::parse("[\".\",\":\",\"\"]"));
    completionProvider.set("resolveSupport", JsonValue::parse("{\"properties\":[\"detail\",\"documentation\"]}"));
    capabilities.set("completionProvider", completionProvider);

    // Hover
    capabilities.set("hoverProvider", JsonValue(true));

    // Definition
    capabilities.set("definitionProvider", JsonValue(true));

    // References
    capabilities.set("referencesProvider", JsonValue(true));

    // Publish diagnostics
    capabilities.set("publishDiagnostics", JsonValue::parse("{\"relatedInformation\":true}"));

    result.set("capabilities", capabilities);

    // Server info
    JsonValue serverInfo = JsonValue::makeObject();
    serverInfo.set("name", JsonValue("forge-language-server"));
    serverInfo.set("version", JsonValue("1.0.0"));
    result.set("_serverInfo", serverInfo);

    return result.stringify();
}

void ForgeLSP::handleInitialized(const std::string& params) {
    // Server is ready. No action needed.
}

void ForgeLSP::handleDidOpen(const std::string& params) {
    JsonValue p = JsonValue::parse(params);
    JsonValue textDoc = p["textDocument"];

    Document doc;
    doc.uri = textDoc.getString("uri");
    doc.languageId = textDoc.getString("languageId");
    doc.content = textDoc.getString("text");
    doc.version = textDoc.getInt("version");

    documents_[doc.uri] = doc;

    // Publish diagnostics
    std::vector<Diagnostic> diagnostics = analyzeSource(doc.content, doc.uri);

    JsonValue paramsObj = JsonValue::makeObject();
    paramsObj.set("uri", JsonValue(doc.uri));

    JsonValue diagArray = JsonValue::makeArray();
    for (auto& d : diagnostics) {
        JsonValue diag = JsonValue::makeObject();
        JsonValue range = JsonValue::makeObject();
        JsonValue start = JsonValue::makeObject();
        start.set("line", JsonValue(d.range.start.line));
        start.set("character", JsonValue(d.range.start.character));
        JsonValue end = JsonValue::makeObject();
        end.set("line", JsonValue(d.range.end.line));
        end.set("character", JsonValue(d.range.end.character));
        range.set("start", start);
        range.set("end", end);
        diag.set("range", range);
        diag.set("severity", JsonValue(d.severity));
        diag.set("message", JsonValue(d.message));
        diag.set("source", JsonValue(d.source));
        diagArray.push(diag);
    }
    paramsObj.set("diagnostics", diagArray);

    sendNotification("textDocument/publishDiagnostics", paramsObj.stringify());
}

void ForgeLSP::handleDidChange(const std::string& params) {
    JsonValue p = JsonValue::parse(params);
    JsonValue textDoc = p["textDocument"];

    std::string uri = textDoc.getString("uri");
    int version = textDoc.getInt("version");

    auto it = documents_.find(uri);
    if (it == documents_.end()) return;

    // Full sync: the content is the entire document
    JsonValue contentChanges = p["contentChanges"];
    if (contentChanges.isArray() && contentChanges.arrVal().size() > 0) {
        it->second.content = contentChanges[0].getString("text");
    }
    it->second.version = version;

    // Publish diagnostics
    std::vector<Diagnostic> diagnostics = analyzeSource(it->second.content, uri);

    JsonValue paramsObj = JsonValue::makeObject();
    paramsObj.set("uri", JsonValue(uri));

    JsonValue diagArray = JsonValue::makeArray();
    for (auto& d : diagnostics) {
        JsonValue diag = JsonValue::makeObject();
        JsonValue range = JsonValue::makeObject();
        JsonValue start = JsonValue::makeObject();
        start.set("line", JsonValue(d.range.start.line));
        start.set("character", JsonValue(d.range.start.character));
        JsonValue end = JsonValue::makeObject();
        end.set("line", JsonValue(d.range.end.line));
        end.set("character", JsonValue(d.range.end.character));
        range.set("start", start);
        range.set("end", end);
        diag.set("range", range);
        diag.set("severity", JsonValue(d.severity));
        diag.set("message", JsonValue(d.message));
        diag.set("source", JsonValue(d.source));
        diagArray.push(diag);
    }
    paramsObj.set("diagnostics", diagArray);

    sendNotification("textDocument/publishDiagnostics", paramsObj.stringify());
}

void ForgeLSP::handleDidSave(const std::string& params) {
    JsonValue p = JsonValue::parse(params);
    JsonValue textDoc = p["textDocument"];
    std::string uri = textDoc.getString("uri");

    auto it = documents_.find(uri);
    if (it == documents_.end()) return;

    // Re-analyze on save
    std::vector<Diagnostic> diagnostics = analyzeSource(it->second.content, uri);

    JsonValue paramsObj = JsonValue::makeObject();
    paramsObj.set("uri", JsonValue(uri));

    JsonValue diagArray = JsonValue::makeArray();
    for (auto& d : diagnostics) {
        JsonValue diag = JsonValue::makeObject();
        JsonValue range = JsonValue::makeObject();
        JsonValue start = JsonValue::makeObject();
        start.set("line", JsonValue(d.range.start.line));
        start.set("character", JsonValue(d.range.start.character));
        JsonValue end = JsonValue::makeObject();
        end.set("line", JsonValue(d.range.end.line));
        end.set("character", JsonValue(d.range.end.character));
        range.set("start", start);
        range.set("end", end);
        diag.set("range", range);
        diag.set("severity", JsonValue(d.severity));
        diag.set("message", JsonValue(d.message));
        diag.set("source", JsonValue(d.source));
        diagArray.push(diag);
    }
    paramsObj.set("diagnostics", diagArray);

    sendNotification("textDocument/publishDiagnostics", paramsObj.stringify());
}

std::string ForgeLSP::handleCompletion(int id, const std::string& params) {
    JsonValue p = JsonValue::parse(params);
    JsonValue textDoc = p["textDocument"];
    std::string uri = textDoc.getString("uri");

    int line = p.getInt("line");
    int character = p.getInt("character");

    auto it = documents_.find(uri);
    if (it == documents_.end()) {
        JsonValue empty = JsonValue::makeArray();
        return empty.stringify();
    }

    Position pos = {line, character};
    std::vector<CompletionItem> items = getCompletions(it->second.content, pos);

    JsonValue result = JsonValue::makeArray();
    for (auto& item : items) {
        JsonValue ci = JsonValue::makeObject();
        ci.set("label", JsonValue(item.label));
        ci.set("kind", JsonValue(item.kind));
        if (!item.detail.empty()) ci.set("detail", JsonValue(item.detail));
        if (!item.documentation.empty()) ci.set("documentation", JsonValue(item.documentation));
        if (!item.insertText.empty()) ci.set("insertText", JsonValue(item.insertText));
        result.push(ci);
    }

    return result.stringify();
}

std::string ForgeLSP::handleHover(int id, const std::string& params) {
    JsonValue p = JsonValue::parse(params);
    JsonValue textDoc = p["textDocument"];
    std::string uri = textDoc.getString("uri");

    int line = p.getInt("line");
    int character = p.getInt("character");

    auto it = documents_.find(uri);
    if (it == documents_.end()) {
        JsonValue null;
        return null.stringify();
    }

    Position pos = {line, character};
    HoverResult hr = getHover(it->second.content, pos);

    if (hr.contents.empty()) {
        JsonValue null;
        return null.stringify();
    }

    JsonValue result = JsonValue::makeObject();
    result.set("contents", JsonValue::parse("{\"kind\":\"markdown\",\"value\":\"" + hr.contents + "\"}"));

    JsonValue range = JsonValue::makeObject();
    JsonValue start = JsonValue::makeObject();
    start.set("line", JsonValue(hr.range.start.line));
    start.set("character", JsonValue(hr.range.start.character));
    JsonValue end = JsonValue::makeObject();
    end.set("line", JsonValue(hr.range.end.line));
    end.set("character", JsonValue(hr.range.end.character));
    range.set("start", start);
    range.set("end", end);
    result.set("range", range);

    return result.stringify();
}

std::string ForgeLSP::handleDefinition(int id, const std::string& params) {
    JsonValue p = JsonValue::parse(params);
    JsonValue textDoc = p["textDocument"];
    std::string uri = textDoc.getString("uri");

    int line = p.getInt("line");
    int character = p.getInt("character");

    auto it = documents_.find(uri);
    if (it == documents_.end()) {
        JsonValue null;
        return null.stringify();
    }

    Position pos = {line, character};
    std::string word = getWordAt(it->second.content, pos);
    std::vector<Range> defs = findDefinition(it->second.content, word);

    if (defs.empty()) {
        JsonValue null;
        return null.stringify();
    }

    // Return first definition as Location
    JsonValue location = JsonValue::makeObject();
    location.set("uri", JsonValue(uri));

    JsonValue range = JsonValue::makeObject();
    JsonValue start = JsonValue::makeObject();
    start.set("line", JsonValue(defs[0].start.line));
    start.set("character", JsonValue(defs[0].start.character));
    JsonValue end = JsonValue::makeObject();
    end.set("line", JsonValue(defs[0].end.line));
    end.set("character", JsonValue(defs[0].end.character));
    range.set("start", start);
    range.set("end", end);
    location.set("range", range);

    return location.stringify();
}

std::string ForgeLSP::handleReferences(int id, const std::string& params) {
    JsonValue p = JsonValue::parse(params);
    JsonValue textDoc = p["textDocument"];
    std::string uri = textDoc.getString("uri");

    int line = p.getInt("line");
    int character = p.getInt("character");

    auto it = documents_.find(uri);
    if (it == documents_.end()) {
        JsonValue empty = JsonValue::makeArray();
        return empty.stringify();
    }

    Position pos = {line, character};
    std::string word = getWordAt(it->second.content, pos);
    std::vector<Range> refs = findReferences(it->second.content, word);

    JsonValue result = JsonValue::makeArray();
    for (auto& r : refs) {
        JsonValue location = JsonValue::makeObject();
        location.set("uri", JsonValue(uri));
        JsonValue range = JsonValue::makeObject();
        JsonValue start = JsonValue::makeObject();
        start.set("line", JsonValue(r.start.line));
        start.set("character", JsonValue(r.start.character));
        JsonValue end = JsonValue::makeObject();
        end.set("line", JsonValue(r.end.line));
        end.set("character", JsonValue(r.end.character));
        range.set("start", start);
        range.set("end", end);
        location.set("range", range);
        result.push(location);
    }

    return result.stringify();
}

} // namespace forge::lsp
