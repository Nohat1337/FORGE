#include "syntax.hpp"
#include "terminal.hpp"
#include <cctype>

SyntaxHighlighter::SyntaxHighlighter() {
    // Control flow keywords
    keywordsControl_ = {
        "if", "else", "while", "for", "in", "return", "break", "continue",
        "match", "case", "try", "catch", "throw", "finally",
        "and", "or", "not", "loop",
    };

    // Declaration keywords
    keywordsDecl_ = {
        "let", "const", "fn", "class", "extends", "super", "this",
        "pub", "static", "final", "new", "as", "is", "extern",
    };

    // Other keywords
    keywordsOther_ = {
        "true", "false", "nil", "import",
    };

    // Built-in functions
    builtins_ = {
        "print", "println", "len", "str", "int", "float",
        "input", "type", "typeof", "range", "assert",
        "panic", "error", "exit",
        "push", "pop", "insert", "remove", "contains",
        "keys", "values", "append", "substr", "split", "join",
        "trim", "upper", "lower", "replace", "contains",
        "abs", "min", "max", "sqrt", "pow", "random", "randomInt",
        "floor", "ceil", "round",
        "clock", "now", "sleep",
        "execute", "env", "cwd", "args", "time",
        "parseInt", "toFloat", "charAt", "substring",
        "keys", "values", "entries", "has", "clone",
        "upper", "lower", "trim", "split", "replace",
        "parseInt", "toFloat", "substring", "charAt",
    };

    // Built-in constants
    builtinConsts_ = {
        "PI", "E", "TAU",
    };

    // Type names
    types_ = {
        "int", "float", "string", "bool", "list", "map",
        "set", "nil", "any", "void", "fn",
    };
}

bool SyntaxHighlighter::isKeywordControl(const std::string& word) const {
    return keywordsControl_.count(word) > 0;
}

bool SyntaxHighlighter::isKeywordDecl(const std::string& word) const {
    return keywordsDecl_.count(word) > 0;
}

bool SyntaxHighlighter::isKeywordOther(const std::string& word) const {
    return keywordsOther_.count(word) > 0;
}

bool SyntaxHighlighter::isBuiltin(const std::string& word) const {
    return builtins_.count(word) > 0;
}

bool SyntaxHighlighter::isBuiltinConst(const std::string& word) const {
    return builtinConsts_.count(word) > 0;
}

bool SyntaxHighlighter::isType(const std::string& word) const {
    return types_.count(word) > 0;
}

std::vector<HighlightSpan> SyntaxHighlighter::highlightLine(const std::string& line, int lineNumber) {
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

        // String (double quote) with interpolation support
        if (c == '"') {
            int start = i;
            i++;
            bool inInterp = false;
            int interpDepth = 0;
            while (i < len) {
                if (line[i] == '\\' && i + 1 < len) {
                    i += 2;
                } else if (line[i] == '"' && interpDepth == 0) {
                    i++;
                    break;
                } else if (line[i] == '$' && i + 1 < len && line[i + 1] == '{' && interpDepth == 0) {
                    inInterp = true;
                    interpDepth = 1;
                    i += 2;
                } else if (line[i] == '{' && inInterp) {
                    interpDepth++;
                    i++;
                } else if (line[i] == '}' && inInterp) {
                    interpDepth--;
                    if (interpDepth == 0) inInterp = false;
                    i++;
                } else {
                    i++;
                }
            }
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

        // Hex number
        if (c == '0' && i + 1 < len && (line[i + 1] == 'x' || line[i + 1] == 'X')) {
            int start = i;
            i += 2;
            while (i < len && std::isxdigit(line[i])) i++;
            spans.push_back({start, i - start, TokenType::NUMBER_HEX});
            continue;
        }

        // Binary number
        if (c == '0' && i + 1 < len && (line[i + 1] == 'b' || line[i + 1] == 'B')) {
            int start = i;
            i += 2;
            while (i < len && (line[i] == '0' || line[i] == '1')) i++;
            spans.push_back({start, i - start, TokenType::NUMBER_BIN});
            continue;
        }

        // Number (including float)
        if (std::isdigit(c) || (c == '.' && i + 1 < len && std::isdigit(line[i + 1]))) {
            int start = i;
            bool hasDot = false;
            while (i < len && (std::isdigit(line[i]) || line[i] == '.')) {
                if (line[i] == '.') {
                    if (hasDot) break;
                    hasDot = true;
                }
                i++;
            }
            if (hasDot) {
                spans.push_back({start, i - start, TokenType::NUMBER});
            } else {
                spans.push_back({start, i - start, TokenType::NUMBER});
            }
            continue;
        }

        // Interpolation start ${...}
        if (c == '$' && i + 1 < len && line[i + 1] == '{') {
            spans.push_back({i, 2, TokenType::STRING_INTERP});
            i += 2;
            continue;
        }

        // Interpolation end }
        if (c == '}') {
            spans.push_back({i, 1, TokenType::STRING_INTERP});
            i++;
            continue;
        }

        // Identifier / keyword
        if (std::isalpha(c) || c == '_') {
            int start = i;
            while (i < len && (std::isalnum(line[i]) || line[i] == '_')) i++;
            std::string word = line.substr(start, i - start);

            if (isKeywordControl(word)) {
                spans.push_back({start, i - start, TokenType::KEYWORD_CONTROL});
            } else if (isKeywordDecl(word)) {
                spans.push_back({start, i - start, TokenType::KEYWORD_DECL});
            } else if (isKeywordOther(word)) {
                spans.push_back({start, i - start, TokenType::KEYWORD_OTHER});
            } else if (isBuiltinConst(word)) {
                spans.push_back({start, i - start, TokenType::BUILITN_CONST});
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

        // Compound operators (+=, -=, *=, /=, %=, ==, !=, <=, >=, =>, ++, --, &&, ||)
        if ((c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '=' ||
             c == '!' || c == '<' || c == '>' || c == '&' || c == '|') && i + 1 < len) {
            char next = line[i + 1];
            if ((c == '+' && (next == '=' || next == '+')) ||
                (c == '-' && (next == '=' || next == '-')) ||
                (c == '*' && next == '=') ||
                (c == '/' && next == '=') ||
                (c == '%' && next == '=') ||
                (c == '=' && (next == '=' || next == '>')) ||
                (c == '!' && next == '=') ||
                (c == '<' && (next == '=' || next == '<')) ||
                (c == '>' && (next == '=' || next == '>')) ||
                (c == '&' && next == '&') ||
                (c == '|' && next == '|')) {
                spans.push_back({i, 2, TokenType::OPERATOR_COMPOUND});
                i += 2;
                continue;
            }
        }

        // Arrow =>
        if (c == '=' && i + 1 < len && line[i + 1] == '>') {
            spans.push_back({i, 2, TokenType::ARROW});
            i += 2;
            continue;
        }

        // Operators
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
            c == '=' || c == '!' || c == '<' || c == '>' ||
            c == '&' || c == '|' || c == '^' || c == '~' || c == '?') {
            int start = i;
            i++;
            if (i < len && line[i] == '=') i++;
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
            // Check for .. range operator
            if (i + 1 < len && line[i + 1] == '.') {
                spans.push_back({i, 2, TokenType::OPERATOR});
                i += 2;
            } else {
                spans.push_back({i, 1, TokenType::DOT});
                i++;
            }
            continue;
        }

        // Plain character (space, tab, etc.)
        i++;
    }

    return spans;
}

std::string SyntaxHighlighter::colorForToken(TokenType type) {
    // Dark theme colors (256-color for better visual quality)
    switch (type) {
        case TokenType::KEYWORD_CONTROL:  return ansi::fg256(208) + ansi::BOLD;   // Orange bold
        case TokenType::KEYWORD_DECL:     return ansi::fg256(208) + ansi::BOLD;   // Orange bold
        case TokenType::KEYWORD_OTHER:    return ansi::fg256(214) + ansi::BOLD;   // Gold bold
        case TokenType::STRING:           return ansi::fg256(114);                // Green
        case TokenType::STRING_INTERP:    return ansi::fg256(214) + ansi::BOLD;   // Gold bold
        case TokenType::NUMBER:           return ansi::fg256(222);                // Yellow
        case TokenType::NUMBER_HEX:       return ansi::fg256(222) + ansi::DIM;    // Yellow dim
        case TokenType::NUMBER_BIN:       return ansi::fg256(222) + ansi::DIM;    // Yellow dim
        case TokenType::COMMENT:          return ansi::FG_GRAY;                   // Gray
        case TokenType::OPERATOR:         return ansi::fg256(198);                // Pink
        case TokenType::OPERATOR_COMPOUND:return ansi::fg256(198) + ansi::BOLD;   // Pink bold
        case TokenType::ARROW:            return ansi::fg256(198) + ansi::BOLD;   // Pink bold
        case TokenType::FUNCTION_NAME:    return ansi::fg256(81) + ansi::BOLD;    // Bright blue bold
        case TokenType::BUILTIN:          return ansi::fg256(226) + ansi::BOLD;   // Bright yellow bold
        case TokenType::BUILITN_CONST:    return ansi::fg256(226) + ansi::DIM;    // Bright yellow dim
        case TokenType::TYPE:             return ansi::fg256(75) + ansi::BOLD;    // Blue bold
        case TokenType::VARIABLE:         return ansi::fg256(253);                // White
        case TokenType::PAREN:            return ansi::fg256(244);                // Light gray
        case TokenType::BRACE:            return ansi::fg256(244);                // Light gray
        case TokenType::BRACKET:          return ansi::fg256(244);                // Light gray
        case TokenType::SEMICOLON:        return ansi::fg256(244);                // Light gray
        case TokenType::COLON:            return ansi::fg256(244);                // Light gray
        case TokenType::COMMA:            return ansi::fg256(244);                // Light gray
        case TokenType::DOT:              return ansi::fg256(244);                // Light gray
        case TokenType::PREPROC:          return ansi::fg256(198);                // Pink
        case TokenType::ERROR:            return ansi::fg256(196) + ansi::BOLD;   // Red bold
        case TokenType::PLAIN:            return ansi::fg256(253);                // White
    }
    return ansi::fg256(253);
}