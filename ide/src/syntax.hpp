#pragma once

#include <string>
#include <vector>
#include <unordered_set>

enum class TokenType {
    PLAIN,
    KEYWORD,
    STRING,
    NUMBER,
    COMMENT,
    OPERATOR,
    FUNCTION_NAME,
    BUILTIN,
    PAREN,
    BRACE,
    BRACKET,
    SEMICOLON,
    COLON,
    COMMA,
    DOT,
    TYPE,
    VARIABLE,
    PREPROC,
};

struct HighlightSpan {
    int start;
    int length;
    TokenType type;
};

class SyntaxHighlighter {
public:
    SyntaxHighlighter();

    std::vector<HighlightSpan> highlightLine(const std::string& line);

    static std::string colorForToken(TokenType type);

private:
    std::unordered_set<std::string> keywords_;
    std::unordered_set<std::string> builtins_;
    std::unordered_set<std::string> types_;

    bool isKeyword(const std::string& word) const;
    bool isBuiltin(const std::string& word) const;
    bool isType(const std::string& word) const;
};
