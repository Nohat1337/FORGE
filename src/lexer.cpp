#include "lexer.hpp"
#include <cctype>
#include <stdexcept>

const std::unordered_map<std::string, TokenType> Lexer::keywords_ = {
    {"let", TokenType::LET}, {"const", TokenType::CONST},
    {"fn", TokenType::FN}, {"return", TokenType::RETURN},
    {"if", TokenType::IF}, {"else", TokenType::ELSE},
    {"while", TokenType::WHILE}, {"for", TokenType::FOR}, {"in", TokenType::IN},
    {"true", TokenType::TRUE}, {"false", TokenType::FALSE},
    {"nil", TokenType::NIL}, {"and", TokenType::AND},
    {"or", TokenType::OR}, {"not", TokenType::NOT},
    {"class", TokenType::CLASS}, {"this", TokenType::THIS},
    {"super", TokenType::SUPER}, {"extends", TokenType::EXTENDS},
    {"struct", TokenType::STRUCT}, {"impl", TokenType::IMPL}, {"enum", TokenType::ENUM},
    {"match", TokenType::MATCH}, {"case", TokenType::CASE},
    {"_", TokenType::WILDCARD},
    {"try", TokenType::TRY}, {"catch", TokenType::CATCH},
    {"throw", TokenType::THROW},
    {"import", TokenType::IMPORT}, {"extern", TokenType::EXTERN},
    {"break", TokenType::BREAK}, {"continue", TokenType::CONTINUE},
};

Lexer::Lexer(const std::string& source) : source_(source), pos_(0), line_(1), column_(1) {}

char Lexer::peek() const { return pos_ >= (int)source_.size() ? '\0' : source_[pos_]; }
char Lexer::peekNext() const { return pos_ + 1 >= (int)source_.size() ? '\0' : source_[pos_ + 1]; }

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') { line_++; column_ = 1; } else { column_++; }
    return c;
}

void Lexer::skipWhitespace() {
    while (pos_ < (int)source_.size()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') { advance(); }
        else if (c == '/' && peekNext() == '/') { while (pos_ < (int)source_.size() && peek() != '\n') advance(); }
        else if (c == '/' && peekNext() == '*') {
            advance(); advance();
            while (pos_ < (int)source_.size()) {
                if (peek() == '*' && peekNext() == '/') { advance(); advance(); break; }
                advance();
            }
        } else break;
    }
}

Token Lexer::readNumber() {
    int startCol = column_;
    std::string num;
    bool isFloat = false;
    bool isHex = false;
    bool isBinary = false;
    
    // Check for hex (0x) or binary (0b) prefix
    if (peek() == '0' && pos_ + 1 < (int)source_.size()) {
        char next = peekNext();
        if (next == 'x' || next == 'X') {
            isHex = true;
            num += advance(); // '0'
            num += advance(); // 'x' or 'X'
        } else if (next == 'b' || next == 'B') {
            isBinary = true;
            num += advance(); // '0'
            num += advance(); // 'b' or 'B'
        }
    }
    
    if (isHex) {
        while (pos_ < (int)source_.size() && std::isxdigit(peek())) num += advance();
        Token tok;
        tok.type = TokenType::INTEGER;
        tok.value = num;
        tok.line = line_;
        tok.column = startCol;
        return tok;
    }
    
    if (isBinary) {
        while (pos_ < (int)source_.size() && (peek() == '0' || peek() == '1')) num += advance();
        Token tok;
        tok.type = TokenType::INTEGER;
        tok.value = num;
        tok.line = line_;
        tok.column = startCol;
        return tok;
    }
    
    // Regular decimal/float parsing
    while (pos_ < (int)source_.size() && std::isdigit(peek())) num += advance();
    if (peek() == '.' && std::isdigit(peekNext())) {
        isFloat = true;
        num += advance();
        while (pos_ < (int)source_.size() && std::isdigit(peek())) num += advance();
    }
    Token tok;
    tok.type = isFloat ? TokenType::FLOAT : TokenType::INTEGER;
    tok.value = num;
    tok.line = line_;
    tok.column = startCol;
    return tok;
}

void Lexer::readString(std::vector<Token>& tokens) {
    int startCol = column_;
    int startLine = line_;
    advance();
    std::string str;
    while (pos_ < (int)source_.size() && peek() != '"') {
        if (peek() == '$' && peekNext() == '{') {
            if (!str.empty()) {
                tokens.push_back({TokenType::STRING_PART, str, startLine, startCol});
                str.clear();
            }
            advance(); advance();
            tokens.push_back({TokenType::DOLLAR_LBRACE, "${", line_, column_ - 2});
            int braceDepth = 1;
            while (pos_ < (int)source_.size() && braceDepth > 0) {
                if (peek() == '{') braceDepth++;
                else if (peek() == '}') { braceDepth--; if (braceDepth == 0) break; }
                skipWhitespace();
                if (pos_ >= (int)source_.size()) break;
                char c = peek();
                if (std::isdigit(c)) tokens.push_back(readNumber());
                else if (c == '"') { readString(tokens); }
                else if (std::isalpha(c) || c == '_') tokens.push_back(readIdentifier());
                else {
                    int sc = column_;
                    switch (c) {
                        case '+': advance(); tokens.push_back({TokenType::PLUS,"+",line_,sc}); break;
                        case '-': advance(); tokens.push_back({TokenType::MINUS,"-",line_,sc}); break;
                        case '*': advance(); tokens.push_back({TokenType::STAR,"*",line_,sc}); break;
                        case '/': advance(); tokens.push_back({TokenType::SLASH,"/",line_,sc}); break;
                        case '%': advance(); tokens.push_back({TokenType::PERCENT,"%",line_,sc}); break;
                        case '=': advance(); if (peek()=='=') { advance(); tokens.push_back({TokenType::EQUAL_EQUAL,"==",line_,sc}); } else tokens.push_back({TokenType::ASSIGN,"=",line_,sc}); break;
                        case '!': advance(); if (peek()=='=') { advance(); tokens.push_back({TokenType::BANG_EQUAL,"!=",line_,sc}); } else tokens.push_back({TokenType::NOT,"!",line_,sc}); break;
                        case '<': advance(); if (peek()=='=') { advance(); tokens.push_back({TokenType::LESS_EQUAL,"<=",line_,sc}); } else tokens.push_back({TokenType::LESS,"<",line_,sc}); break;
                        case '>': advance(); if (peek()=='=') { advance(); tokens.push_back({TokenType::GREATER_EQUAL,">=",line_,sc}); } else tokens.push_back({TokenType::GREATER,">",line_,sc}); break;
                        case '(': advance(); tokens.push_back({TokenType::LPAREN,"(",line_,sc}); break;
                        case ')': advance(); tokens.push_back({TokenType::RPAREN,")",line_,sc}); break;
                        case '[': advance(); tokens.push_back({TokenType::LBRACKET,"[",line_,sc}); break;
                        case ']': advance(); tokens.push_back({TokenType::RBRACKET,"]",line_,sc}); break;
                        case ',': advance(); tokens.push_back({TokenType::COMMA,",",line_,sc}); break;
                        case '.': advance(); tokens.push_back({TokenType::DOT,".",line_,sc}); break;
                        default: advance(); break;
                    }
                }
            }
            if (pos_ < (int)source_.size() && peek() == '}') {
                tokens.push_back({TokenType::RBRACE, "}", line_, column_});
                advance();
            }
            continue;
        }
        if (peek() == '\\') {
            advance();
            switch (peek()) {
                case 'n': str += '\n'; break; case 't': str += '\t'; break;
                case '\\': str += '\\'; break; case '"': str += '"'; break;
                case '0': str += '\0'; break;
                default: str += peek(); break;
            }
        } else str += peek();
        advance();
    }
    if (pos_ >= (int)source_.size())
        throw std::runtime_error("Unterminated string at line " + std::to_string(startLine));
    advance();
if (!str.empty()) {
            tokens.push_back({TokenType::STRING_PART, str, startLine, startCol});
        }
        
        tokens.push_back({TokenType::STRING_END, "", startLine, column_});
}

void Lexer::readChar(std::vector<Token>& tokens) {
    int startCol = column_;
    int startLine = line_;
    advance(); // skip opening '
    
    // Read UTF-8 character
    std::string charStr;
    char first = advance();
    charStr += first;
    
    // Check if it's a multi-byte UTF-8 character
    unsigned char firstByte = static_cast<unsigned char>(charStr[0]);
    int expectedBytes = 1;
    if ((firstByte & 0x80) != 0) {
        if ((firstByte & 0xE0) == 0xC0) expectedBytes = 2;
        else if ((firstByte & 0xF0) == 0xE0) expectedBytes = 3;
        else if ((firstByte & 0xF8) == 0xF0) expectedBytes = 4;
        
        for (int i = 1; i < expectedBytes; i++) {
            if (pos_ < (int)source_.size()) {
                charStr += advance();
            }
        }
    }
    
    // Handle escape sequences for single-byte chars
    if (charStr.size() == 1 && charStr[0] == '\\') {
        char c = advance();
        switch (c) {
            case 'n': charStr = "\n"; break;
            case 't': charStr = "\t"; break;
            case '\\': charStr = "\\"; break;
            case '\'': charStr = "'"; break;
            case '0': charStr = "\0"; break;
            default: break;
        }
    }
    
    if (pos_ >= (int)source_.size() || peek() != '\'')
        throw std::runtime_error("Unterminated character literal at line " + std::to_string(startLine));
    advance(); // skip closing '
    tokens.push_back({TokenType::CHAR, charStr, startLine, startCol});
}

Token Lexer::readIdentifier() {
    int startCol = column_;
    std::string id;
    while (pos_ < (int)source_.size() && (std::isalnum(peek()) || peek() == '_')) id += advance();
    Token tok;
    tok.line = line_; tok.column = startCol;
    auto it = keywords_.find(id);
    tok.type = (it != keywords_.end()) ? it->second : TokenType::IDENTIFIER;
    tok.value = id;
    return tok;
}

Token Lexer::makeToken(TokenType type, const std::string& value) {
    return {type, value, line_, column_};
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (pos_ < (int)source_.size()) {
        skipWhitespace();
        if (pos_ >= (int)source_.size()) break;
        char c = peek();
        if (std::isdigit(c)) { tokens.push_back(readNumber()); }
        else if (c == '"') { readString(tokens); }
        else if (c == '\'') { readChar(tokens); }
        else if (std::isalpha(c) || c == '_') { tokens.push_back(readIdentifier()); }
        else {
            int startCol = column_;
            switch (c) {
                case '+': advance(); if (peek()=='=') { advance(); tokens.push_back({TokenType::PLUS_ASSIGN,"+=",line_,startCol}); } else tokens.push_back({TokenType::PLUS,"+",line_,startCol}); break;
                case '-': advance(); if (peek()=='=') { advance(); tokens.push_back({TokenType::MINUS_ASSIGN,"-=",line_,startCol}); } else if (peek()=='>') { advance(); tokens.push_back({TokenType::ARROW,"->",line_,startCol}); } else tokens.push_back({TokenType::MINUS,"-",line_,startCol}); break;
                case '*': advance(); tokens.push_back({TokenType::STAR,"*",line_,startCol}); break;
                case '/': advance(); tokens.push_back({TokenType::SLASH,"/",line_,startCol}); break;
                case '%': advance(); tokens.push_back({TokenType::PERCENT,"%",line_,startCol}); break;
                case '=': advance(); if (peek()=='=') { advance(); tokens.push_back({TokenType::EQUAL_EQUAL,"==",line_,startCol}); } else tokens.push_back({TokenType::ASSIGN,"=",line_,startCol}); break;
                case '!': advance(); if (peek()=='=') { advance(); tokens.push_back({TokenType::BANG_EQUAL,"!=",line_,startCol}); } else { tokens.push_back({TokenType::NOT,"!",line_,startCol}); } break;
                case '<': advance(); if (peek()=='=') { advance(); tokens.push_back({TokenType::LESS_EQUAL,"<=",line_,startCol}); } else tokens.push_back({TokenType::LESS,"<",line_,startCol}); break;
                case '>': advance(); if (peek()=='=') { advance(); tokens.push_back({TokenType::GREATER_EQUAL,">=",line_,startCol}); } else tokens.push_back({TokenType::GREATER,">",line_,startCol}); break;
                case '(': advance(); tokens.push_back({TokenType::LPAREN,"(",line_,startCol}); break;
                case ')': advance(); tokens.push_back({TokenType::RPAREN,")",line_,startCol}); break;
                case '{': advance(); tokens.push_back({TokenType::LBRACE,"{",line_,startCol}); break;
                case '}': advance(); tokens.push_back({TokenType::RBRACE,"}",line_,startCol}); break;
                case '[': advance(); tokens.push_back({TokenType::LBRACKET,"[",line_,startCol}); break;
                case ']': advance(); tokens.push_back({TokenType::RBRACKET,"]",line_,startCol}); break;
                case ';': advance(); tokens.push_back({TokenType::SEMICOLON,";",line_,startCol}); break;
                case ':': advance(); tokens.push_back({TokenType::COLON,":",line_,startCol}); break;
                case '&': advance(); if (peek()=='&') { advance(); tokens.push_back({TokenType::AND,"&&",line_,startCol}); } else { tokens.push_back({TokenType::AMPERSAND,"&",line_,startCol}); } break;
                case '|': advance(); if (peek()=='|') { advance(); tokens.push_back({TokenType::OR,"||",line_,startCol}); } else { tokens.push_back({TokenType::BAR,"|",line_,startCol}); } break;
                case '^': advance(); tokens.push_back({TokenType::CARET,"^",line_,startCol}); break;
                case ',': advance(); tokens.push_back({TokenType::COMMA,",",line_,startCol}); break;
                case '.': advance(); tokens.push_back({TokenType::DOT,".",line_,startCol}); break;
                case '?': advance(); tokens.push_back({TokenType::QUESTION,"?",line_,startCol}); break;
                case '$': advance(); if (peek()=='{') { advance(); tokens.push_back({TokenType::DOLLAR_LBRACE,"${",line_,startCol}); } else { tokens.push_back({TokenType::IDENTIFIER,"$",line_,startCol}); } break;
                case '\n': advance(); tokens.push_back({TokenType::NEWLINE,"\\n",line_,startCol}); break;
                default: throw std::runtime_error(std::string("Unexpected character '") + c + "' at line " + std::to_string(line_));
            }
        }
    }
    tokens.push_back({TokenType::EOF_TOKEN, "", line_, column_});
    return tokens;
}
