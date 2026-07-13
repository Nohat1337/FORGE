#include "syntax.hpp"
#include "terminal.hpp"
#include <cctype>

SyntaxHighlighter::SyntaxHighlighter() {
    keywords_ = {
        "let", "const", "fn", "return", "if", "else", "while", "for",
        "in", "true", "false", "nil", "and", "or", "not",
        "class", "this", "super", "extends",
        "match", "case", "try", "catch", "throw",
        "import", "extern", "break", "continue", "loop",
        "pub", "static", "final", "new", "as", "is",
    };

    builtins_ = {
        "print", "println", "len", "str", "int", "float",
        "input", "type", "typeof", "range", "assert",
        "panic", "error", "exit", "open", "close",
        "read", "write", "string", "list", "map", "set",
        "push", "pop", "insert", "remove", "contains",
        "keys", "values", "append", "substr", "contains",
        "substr", "split", "join", "trim", "upper", "lower",
        "abs", "min", "max", "sqrt", "pow", "random",
        "clock", "now", "sleep",
    };

    types_ = {
        "int", "float", "string", "bool", "list", "map",
        "set", "nil", "any", "void",
    };
}

bool SyntaxHighlighter::isKeyword(const std::string& word) const {
    return keywords_.count(word) > 0;
}

bool SyntaxHighlighter::isBuiltin(const std::string& word) const {
    return builtins_.count(word) > 0;
}

bool SyntaxHighlighter::isType(const std::string& word) const {
    return types_.count(word) > 0;
}

std::vector<HighlightSpan> SyntaxHighlighter::highlightLine(const std::string& line) {
    std::vector<HighlightSpan> spans;
    int i = 0;
    int len = (int)line.size();

    while (i < len) {
        char c = line[i];

        // Line comment
        if (c == '/' && i + 1 < len && line[i + 1] == '/') {
            spans.push_back({i, len - i, TokenType::COMMENT});
            break;
        }

        // Block comment start
        if (c == '/' && i + 1 < len && line[i + 1] == '*') {
            int start = i;
            i += 2;
            while (i < len - 1) {
                if (line[i] == '*' && line[i + 1] == '/') {
                    i += 2;
                    break;
                }
                i++;
            }
            spans.push_back({start, i - start, TokenType::COMMENT});
            continue;
        }

        // String (double quote)
        if (c == '"') {
            int start = i;
            i++;
            while (i < len && line[i] != '"') {
                if (line[i] == '\\' && i + 1 < len) i++;
                i++;
            }
            if (i < len) i++;
            spans.push_back({start, i - start, TokenType::STRING});
            continue;
        }

        // String (single quote)
        if (c == '\'') {
            int start = i;
            i++;
            while (i < len && line[i] != '\'') {
                if (line[i] == '\\' && i + 1 < len) i++;
                i++;
            }
            if (i < len) i++;
            spans.push_back({start, i - start, TokenType::STRING});
            continue;
        }

        // Number
        if (std::isdigit(c) || (c == '.' && i + 1 < len && std::isdigit(line[i + 1]))) {
            int start = i;
            while (i < len && (std::isdigit(line[i]) || line[i] == '.')) i++;
            spans.push_back({start, i - start, TokenType::NUMBER});
            continue;
        }

        // Interpolation ${...}
        if (c == '$' && i + 1 < len && line[i + 1] == '{') {
            spans.push_back({i, 2, TokenType::PREPROC});
            i += 2;
            continue;
        }

        // Identifier / keyword
        if (std::isalpha(c) || c == '_') {
            int start = i;
            while (i < len && (std::isalnum(line[i]) || line[i] == '_')) i++;
            std::string word = line.substr(start, i - start);

            if (isKeyword(word)) {
                spans.push_back({start, i - start, TokenType::KEYWORD});
            } else if (isBuiltin(word)) {
                spans.push_back({start, i - start, TokenType::BUILTIN});
            } else if (isType(word)) {
                spans.push_back({start, i - start, TokenType::TYPE});
            } else {
                // Check if followed by '(' -> function name
                int j = i;
                while (j < len && line[j] == ' ') j++;
                if (j < len && line[j] == '(') {
                    spans.push_back({start, i - start, TokenType::FUNCTION_NAME});
                } else {
                    spans.push_back({start, i - start, TokenType::VARIABLE});
                }
            }
            continue;
        }

        // Operators
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
            c == '=' || c == '!' || c == '<' || c == '>' ||
            c == '&' || c == '|' || c == '^' || c == '~') {
            int start = i;
            i++;
            if (i < len && (line[i] == '=' || line[i] == '>' || line[i] == '<')) i++;
            spans.push_back({start, i - start, TokenType::OPERATOR});
            continue;
        }

        // Punctuation
        if (c == '(' || c == ')') {
            spans.push_back({i, 1, TokenType::PAREN});
            i++; continue;
        }
        if (c == '{' || c == '}') {
            spans.push_back({i, 1, TokenType::BRACE});
            i++; continue;
        }
        if (c == '[' || c == ']') {
            spans.push_back({i, 1, TokenType::BRACKET});
            i++; continue;
        }
        if (c == ';') {
            spans.push_back({i, 1, TokenType::SEMICOLON});
            i++; continue;
        }
        if (c == ':') {
            spans.push_back({i, 1, TokenType::COLON});
            i++; continue;
        }
        if (c == ',') {
            spans.push_back({i, 1, TokenType::COMMA});
            i++; continue;
        }
        if (c == '.') {
            spans.push_back({i, 1, TokenType::DOT});
            i++; continue;
        }

        // Plain character (space, tab, etc.)
        i++;
    }

    return spans;
}

std::string SyntaxHighlighter::colorForToken(TokenType type) {
    switch (type) {
        case TokenType::KEYWORD:      return ansi::FG_BOLD_CYAN;
        case TokenType::STRING:       return ansi::FG_GREEN;
        case TokenType::NUMBER:       return ansi::FG_YELLOW;
        case TokenType::COMMENT:      return ansi::FG_GRAY;
        case TokenType::OPERATOR:     return ansi::FG_MAGENTA;
        case TokenType::FUNCTION_NAME:return ansi::FG_BOLD_BLUE;
        case TokenType::BUILTIN:      return ansi::FG_BOLD_YELLOW;
        case TokenType::TYPE:         return ansi::FG_CYAN;
        case TokenType::PAREN:        return ansi::FG_WHITE;
        case TokenType::BRACE:        return ansi::FG_WHITE;
        case TokenType::BRACKET:      return ansi::FG_WHITE;
        case TokenType::SEMICOLON:    return ansi::FG_WHITE;
        case TokenType::COLON:        return ansi::FG_WHITE;
        case TokenType::COMMA:        return ansi::FG_WHITE;
        case TokenType::DOT:          return ansi::FG_WHITE;
        case TokenType::VARIABLE:     return ansi::FG_WHITE;
        case TokenType::PREPROC:      return ansi::FG_MAGENTA;
        case TokenType::PLAIN:        return ansi::FG_WHITE;
    }
    return ansi::FG_WHITE;
}
