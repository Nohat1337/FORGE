#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

enum class TokenType {
    PLAIN,
    KEYWORD,
    KEYWORD_CONTROL,   // if, else, while, for, return, break, continue
    KEYWORD_DECL,      // let, const, fn, class, extends
    KEYWORD_OTHER,     // true, false, nil, this, super
    STRING,
    STRING_INTERP,     // ${...} inside string
    NUMBER,
    NUMBER_HEX,
    NUMBER_BIN,
    COMMENT,
    COMMENT_DOC,       /// doc comment
    OPERATOR,
    OPERATOR_COMPOUND, // +=, -=, *=, etc.
    FUNCTION_NAME,
    FUNCTION_CALL,
    BUILTIN,
    BUILITN_CONST,     // builtin constants
    PAREN,
    BRACE,
    BRACKET,
    SEMICOLON,
    COLON,
    COMMA,
    DOT,
    ARROW,             // =>
    TYPE,
    VARIABLE,
    PREPROC,           // import
    ERROR,             // unclosed string, etc.
};

struct HighlightSpan {
    int start;
    int length;
    TokenType type;
};

class SyntaxHighlighter {
public:
    SyntaxHighlighter();

    std::vector<HighlightSpan> highlightLine(const std::string& line, int lineNumber = 0);

    static std::string colorForToken(TokenType type);

private:
    std::unordered_set<std::string> keywordsControl_;
    std::unordered_set<std::string> keywordsDecl_;
    std::unordered_set<std::string> keywordsOther_;
    std::unordered_set<std::string> builtins_;
    std::unordered_set<std::string> builtinConsts_;
    std::unordered_set<std::string> types_;

    bool isKeywordControl(const std::string& word) const;
    bool isKeywordDecl(const std::string& word) const;
    bool isKeywordOther(const std::string& word) const;
    bool isBuiltin(const std::string& word) const;
    bool isBuiltinConst(const std::string& word) const;
    bool isType(const std::string& word) const;
    
    enum class State { NORMAL, IN_STRING, IN_COMMENT, IN_INTERPOLATION };
};
