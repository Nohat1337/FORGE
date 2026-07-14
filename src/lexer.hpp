#pragma once

#include <string>
#include <vector>
#include <unordered_map>

enum class TokenType {
    INTEGER, FLOAT, CHAR, STRING, IDENTIFIER,
    STRING_PART, STRING_END,
    LET, CONST, FN, RETURN, IF, ELSE, WHILE, FOR, IN, TRUE, FALSE, NIL,
    AND, OR, XOR, NOT,
    CLASS, THIS, SUPER, EXTENDS,
    STRUCT, IMPL, ENUM,
    MATCH, CASE, WILDCARD,
    TRY, CATCH, THROW,
    IMPORT, EXTERN, AS, FROM,
    GEN, YIELD,
    DOTDOT,
    PLUS, MINUS, STAR, SLASH, PERCENT,
    EQUAL, EQUAL_EQUAL, BANG_EQUAL,
    LESS, LESS_EQUAL, GREATER, GREATER_EQUAL,
    ASSIGN, PLUS_ASSIGN, MINUS_ASSIGN,
    QUESTION, QUESTION_DOT,
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    SEMICOLON, COLON, COMMA, DOT, ARROW,
    DOLLAR_LBRACE,
    EOF_TOKEN, NEWLINE, COMMENT,
    AMPERSAND,
    BAR,
    CARET,
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;
};

class Lexer {
public:
    Lexer(const std::string& source);
    std::vector<Token> tokenize();
private:
    std::string source_;
    int pos_;
    int line_;
    int column_;

    char peek() const;
    char peekNext() const;
    char advance();
    void skipWhitespace();
    Token readNumber();
    void readString(std::vector<Token>& tokens);
    void readChar(std::vector<Token>& tokens);
    Token readIdentifier();

    Token makeToken(TokenType type, const std::string& value);

    static const std::unordered_map<std::string, TokenType> keywords_;
};
