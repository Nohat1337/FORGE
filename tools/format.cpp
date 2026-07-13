#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include "lexer.hpp"

static bool isKeyword(TokenType t) {
    switch (t) {
        case TokenType::LET: case TokenType::CONST: case TokenType::FN:
        case TokenType::RETURN: case TokenType::IF: case TokenType::ELSE:
        case TokenType::WHILE: case TokenType::FOR: case TokenType::IN:
        case TokenType::TRUE: case TokenType::FALSE: case TokenType::NIL:
        case TokenType::AND: case TokenType::OR: case TokenType::NOT:
        case TokenType::CLASS: case TokenType::THIS: case TokenType::SUPER:
        case TokenType::EXTENDS: case TokenType::MATCH: case TokenType::CASE:
        case TokenType::TRY: case TokenType::CATCH: case TokenType::THROW:
        case TokenType::IMPORT: case TokenType::EXTERN:
            return true;
        default:
            return false;
    }
}

static bool isOp(TokenType t) {
    switch (t) {
        case TokenType::PLUS: case TokenType::MINUS: case TokenType::STAR:
        case TokenType::SLASH: case TokenType::PERCENT:
        case TokenType::EQUAL: case TokenType::EQUAL_EQUAL: case TokenType::BANG_EQUAL:
        case TokenType::LESS: case TokenType::LESS_EQUAL:
        case TokenType::GREATER: case TokenType::GREATER_EQUAL:
        case TokenType::PLUS_ASSIGN: case TokenType::MINUS_ASSIGN:
            return true;
        default:
            return false;
    }
}

static bool isLiteral(TokenType t) {
    return t == TokenType::INTEGER || t == TokenType::FLOAT ||
           t == TokenType::STRING_PART || t == TokenType::TRUE ||
           t == TokenType::FALSE || t == TokenType::NIL;
}

static bool isIdentifierOrLiteral(TokenType t) {
    return t == TokenType::IDENTIFIER || isLiteral(t);
}

static bool noSpaceBefore(TokenType t) {
    return t == TokenType::RBRACE || t == TokenType::RPAREN ||
           t == TokenType::RBRACKET || t == TokenType::SEMICOLON ||
           t == TokenType::COMMA || t == TokenType::DOT;
}

static bool noSpaceAfter(TokenType t) {
    return t == TokenType::LBRACE || t == TokenType::LPAREN ||
           t == TokenType::LBRACKET || t == TokenType::DOT;
}

static bool needsSpaceBetween(TokenType prev, TokenType next) {
    if (prev == TokenType::EOF_TOKEN || next == TokenType::EOF_TOKEN) return false;
    if (prev == TokenType::NEWLINE || next == TokenType::NEWLINE) return false;
    if (prev == TokenType::COMMENT || next == TokenType::COMMENT) return false;
    if (noSpaceAfter(prev) || noSpaceBefore(next)) return false;
    if (prev == TokenType::IDENTIFIER && next == TokenType::LPAREN) return false;
    if (prev == TokenType::IDENTIFIER && next == TokenType::LBRACKET) return false;
    if (prev == TokenType::COLON && next != TokenType::LBRACE) return false;
    if (next == TokenType::COLON) return false;
    if (prev == TokenType::RPAREN && next == TokenType::LPAREN) return false;
    if (prev == TokenType::RPAREN && next == TokenType::LBRACKET) return false;
    if (prev == TokenType::RBRACKET && next == TokenType::LPAREN) return false;
    if (prev == TokenType::ARROW) return true;
    if (next == TokenType::ARROW) return true;
    if (prev == TokenType::COMMA) return true;
    if (prev == TokenType::LBRACE) return false;
    if (next == TokenType::LBRACE) return true;
    if (prev == TokenType::RBRACE) return true;
    if (prev == TokenType::RPAREN && next == TokenType::LBRACE) return true;
    return true;
}

std::string formatSource(const std::vector<Token>& tokens) {
    std::string out;
    int indent = 0;
    bool pendingNewline = false;
    bool lastWasNewline = true;
    int interpDepth = 0;
    int parenDepth = 0;

    auto emitIndent = [&]() {
        for (int j = 0; j < indent; j++) out += "    ";
    };

    for (size_t i = 0; i < tokens.size(); i++) {
        const Token& tok = tokens[i];

        if (tok.type == TokenType::DOLLAR_LBRACE) {
            interpDepth++;
            out += "${";
            lastWasNewline = false;
            continue;
        }
        if (tok.type == TokenType::RBRACE && interpDepth > 0) {
            interpDepth--;
            out += "}";
            lastWasNewline = false;
            continue;
        }
        if (interpDepth > 0) {
            out += tok.value;
            lastWasNewline = false;
            continue;
        }

        if (tok.type == TokenType::NEWLINE) {
            pendingNewline = true;
            continue;
        }

        if (tok.type == TokenType::COMMENT) {
            if (pendingNewline) {
                out += "\n";
                emitIndent();
                pendingNewline = false;
                lastWasNewline = false;
            }
            out += tok.value;
            continue;
        }

        if (tok.type == TokenType::LBRACE) {
            out += " {";
            indent++;
            pendingNewline = true;
            lastWasNewline = true;
            continue;
        }

        if (tok.type == TokenType::RBRACE) {
            indent--;
            if (indent < 0) indent = 0;
            out += "\n";
            emitIndent();
            out += "}";
            pendingNewline = false;
            lastWasNewline = false;
            bool nextIsElseCatch = false;
            for (size_t k = i + 1; k < tokens.size(); k++) {
                if (tokens[k].type != TokenType::NEWLINE && tokens[k].type != TokenType::COMMENT) {
                    nextIsElseCatch = (tokens[k].type == TokenType::ELSE || tokens[k].type == TokenType::CATCH || tokens[k].type == TokenType::SEMICOLON);
                    break;
                }
            }
            if (!nextIsElseCatch) {
                pendingNewline = true;
            }
            continue;
        }

        if (tok.type == TokenType::SEMICOLON) {
            out += ";";
            if (parenDepth == 0) {
                pendingNewline = true;
            }
            lastWasNewline = false;
            continue;
        }

        if (pendingNewline) {
            out += "\n";
            emitIndent();
            pendingNewline = false;
            lastWasNewline = false;
        }

        if (tok.type == TokenType::STRING_PART) {
            if (!out.empty() && !lastWasNewline) {
                char last = out.back();
                if (last != ' ' && last != '\n' && last != '(' && last != '[' &&
                    last != '{' && last != ';') {
                    out += " ";
                }
            }
            out += "\"" + tok.value + "\"";
            lastWasNewline = false;
            continue;
        }

        if (tok.type == TokenType::STRING_END) {
            continue;
        }

        bool lineStart = false;
        if (!out.empty()) {
            size_t pos = out.size();
            while (pos > 0 && out[pos - 1] == ' ') pos--;
            if (pos > 0 && out[pos - 1] == '\n') lineStart = true;
        }

        TokenType prevType = TokenType::EOF_TOKEN;
        if (!lineStart) {
            if (!out.empty()) {
                for (int j = (int)i - 1; j >= 0; j--) {
                    if (tokens[j].type != TokenType::NEWLINE && tokens[j].type != TokenType::COMMENT) {
                        prevType = tokens[j].type;
                        break;
                    }
                }
            }

            if (prevType != TokenType::EOF_TOKEN && needsSpaceBetween(prevType, tok.type)) {
                out += " ";
            }
        }

        out += tok.value;
        if (tok.type == TokenType::LPAREN) parenDepth++;
        if (tok.type == TokenType::RPAREN) parenDepth--;
        lastWasNewline = false;
    }

    if (!out.empty() && out.back() != '\n') {
        out += "\n";
    }

    return out;
}

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file: " << path << "\n";
        exit(1);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: forge-format <file.fge>\n";
        return 1;
    }

    std::string source = readFile(argv[1]);
    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        std::cout << formatSource(tokens);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
