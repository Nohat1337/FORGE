#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include "lexer.hpp"

struct LintWarning {
    int line;
    std::string message;
};

class Linter {
public:
    Linter(const std::string& filename, const std::string& source)
        : filename_(filename), source_(source) {}

    std::vector<LintWarning> lint() {
        Lexer lexer(source_);
        tokens_ = lexer.tokenize();
        warnings_.clear();
        declaredVars_.clear();
        usedVars_.clear();
        importedModules_.clear();

        scanTopLevel();
        checkUnusedVars();
        checkDebugPrints();
        checkEmptyCatchBlocks();
        checkDeepNesting();
        checkUnusedImports();
        checkDeadCode();

        return warnings_;
    }

private:
    std::string filename_;
    std::string source_;
    std::vector<Token> tokens_;
    std::vector<LintWarning> warnings_;
    std::map<int, std::set<std::string>> declaredVars_;
    std::map<int, std::set<std::string>> usedVars_;
    std::set<std::string> importedModules_;
    std::set<std::string> importedModuleNames_;

    void warn(int line, const std::string& msg) {
        warnings_.push_back({line, msg});
    }

    void scanTopLevel() {
        size_t i = 0;
        int depth = 0;
        std::set<std::string> currentScopeVars;
        std::vector<std::set<std::string>> scopeStack;
        bool afterReturn = false;
        int returnLine = 0;

        while (i < tokens_.size()) {
            const Token& tok = tokens_[i];

            if (tok.type == TokenType::LBRACE) {
                depth++;
                if (afterReturn && depth > 0) {
                    warn(returnLine, "Dead code after return/throw");
                    afterReturn = false;
                }
            }
            if (tok.type == TokenType::RBRACE) {
                depth--;
                if (depth < 0) depth = 0;
            }

            if (depth == 0 && tok.type == TokenType::NEWLINE) {
                afterReturn = false;
            }

            if (tok.type == TokenType::IMPORT) {
                i++;
                while (i < tokens_.size() && tokens_[i].type == TokenType::NEWLINE) i++;
                if (i < tokens_.size() && tokens_[i].type == TokenType::STRING_PART) {
                    importedModules_.insert(tokens_[i].value);
                    i++;
                    continue;
                }
                if (i < tokens_.size() && tokens_[i].type == TokenType::IDENTIFIER) {
                    importedModules_.insert(tokens_[i].value);
                    i++;
                    continue;
                }
            }

            if (tok.type == TokenType::LET || tok.type == TokenType::CONST) {
                i++;
                while (i < tokens_.size() && tokens_[i].type == TokenType::NEWLINE) i++;
                if (i < tokens_.size() && tokens_[i].type == TokenType::IDENTIFIER) {
                    std::string varName = tokens_[i].value;
                    int varLine = tok.line;

                    if (currentScopeVars.count(varName) > 0) {
                        warn(varLine, "Variable '" + varName + "' shadows an outer variable");
                    }

                    currentScopeVars.insert(varName);
                    declaredVars_[depth].insert(varName);
                    declaredVars_[varLine].insert(varName);
                    i++;

                    if (i < tokens_.size() && tokens_[i].type == TokenType::ASSIGN) {
                        i++;
                        trackExpressionUsage(i, depth, currentScopeVars);
                    }
                    if (i < tokens_.size() && tokens_[i].type == TokenType::SEMICOLON) i++;
                    continue;
                }
            }

            if (tok.type == TokenType::FN) {
                i++;
                while (i < tokens_.size() && tokens_[i].type == TokenType::NEWLINE) i++;
                std::string fnName;
                if (i < tokens_.size() && tokens_[i].type == TokenType::IDENTIFIER) {
                    fnName = tokens_[i].value;
                    i++;
                }
                int paramCount = 0;
                if (i < tokens_.size() && tokens_[i].type == TokenType::LPAREN) {
                    i++;
                    while (i < tokens_.size() && tokens_[i].type != TokenType::RPAREN) {
                        if (tokens_[i].type == TokenType::COMMA) {
                            paramCount++;
                        } else if (tokens_[i].type == TokenType::IDENTIFIER) {
                            paramCount++;
                        }
                        i++;
                    }
                    if (i < tokens_.size() && tokens_[i].type == TokenType::RPAREN) i++;
                }
                if (paramCount > 5) {
                    warn(tok.line, "Function '" + fnName + "' has too many parameters (" + std::to_string(paramCount) + " > 5)");
                }
                if (!fnName.empty()) {
                    currentScopeVars.insert(fnName);
                }
                afterReturn = false;
                continue;
            }

            if (tok.type == TokenType::RETURN || tok.type == TokenType::THROW) {
                afterReturn = true;
                returnLine = tok.line;
                i++;
                if (tok.type == TokenType::RETURN) {
                    while (i < tokens_.size() && tokens_[i].type == TokenType::NEWLINE) i++;
                    trackExpressionUsage(i, depth, currentScopeVars);
                }
                continue;
            }

            if (tok.type == TokenType::IDENTIFIER) {
                std::string name = tok.value;
                if (depth > 0) {
                    usedVars_[tok.line].insert(name);
                }
            }

            i++;
        }
    }

    void trackExpressionUsage(size_t& i, int depth, const std::set<std::string>& currentScopeVars) {
        while (i < tokens_.size()) {
            const Token& t = tokens_[i];
            if (t.type == TokenType::SEMICOLON || t.type == TokenType::NEWLINE || t.type == TokenType::EOF_TOKEN) {
                break;
            }
            if (t.type == TokenType::IDENTIFIER) {
                if (depth > 0) {
                    usedVars_[t.line].insert(t.value);
                }
            }
            if (t.type == TokenType::LBRACE || t.type == TokenType::RBRACE) break;
            i++;
        }
    }

    void checkUnusedVars() {
        for (auto& [line, vars] : declaredVars_) {
            for (const auto& var : vars) {
                bool used = false;
                for (auto& [uline, uvars] : usedVars_) {
                    if (uvars.count(var) > 0 && uline > line) {
                        used = true;
                        break;
                    }
                }
                if (!used) {
                    bool found = false;
                    for (auto& [l2, v2] : declaredVars_) {
                        if (l2 != line && v2.count(var) > 0) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        warn(line, "Unused variable '" + var + "'");
                    }
                }
            }
        }
    }

    void checkDebugPrints() {
        for (const auto& tok : tokens_) {
            if (tok.type == TokenType::IDENTIFIER && (tok.value == "print" || tok.value == "console")) {
                size_t idx = &tok - &tokens_[0];
                if (idx + 2 < tokens_.size() && tokens_[idx + 1].type == TokenType::DOT &&
                    tokens_[idx + 2].type == TokenType::IDENTIFIER &&
                    (tokens_[idx + 2].value == "log" || tokens_[idx + 2].value == "print")) {
                    warn(tok.line, "Debug print/console.log left in code");
                }
            }
        }
    }

    void checkEmptyCatchBlocks() {
        for (size_t i = 0; i < tokens_.size(); i++) {
            if (tokens_[i].type == TokenType::CATCH) {
                size_t j = i + 1;
                while (j < tokens_.size() && tokens_[j].type == TokenType::NEWLINE) j++;
                if (j < tokens_.size() && tokens_[j].type == TokenType::LPAREN) {
                    j++;
                    while (j < tokens_.size() && tokens_[j].type != TokenType::RPAREN) j++;
                    if (j < tokens_.size() && tokens_[j].type == TokenType::RPAREN) j++;
                }
                while (j < tokens_.size() && tokens_[j].type == TokenType::NEWLINE) j++;
                if (j < tokens_.size() && tokens_[j].type == TokenType::LBRACE) {
                    j++;
                    while (j < tokens_.size() && tokens_[j].type == TokenType::NEWLINE) j++;
                    if (j < tokens_.size() && tokens_[j].type == TokenType::RBRACE) {
                        warn(tokens_[i].line, "Empty catch block");
                    }
                }
            }
        }
    }

    void checkDeepNesting() {
        int depth = 0;
        for (const auto& tok : tokens_) {
            if (tok.type == TokenType::LBRACE) {
                depth++;
                if (depth > 4) {
                    warn(tok.line, "Deep nesting detected (" + std::to_string(depth) + " levels deep, max recommended is 4)");
                }
            }
            if (tok.type == TokenType::RBRACE) {
                depth--;
                if (depth < 0) depth = 0;
            }
        }
    }

    void checkTooManyParams() {
        for (size_t i = 0; i < tokens_.size(); i++) {
            if (tokens_[i].type == TokenType::FN) {
                size_t j = i + 1;
                while (j < tokens_.size() && tokens_[j].type == TokenType::NEWLINE) j++;
                std::string fnName = "(anonymous)";
                if (j < tokens_.size() && tokens_[j].type == TokenType::IDENTIFIER) {
                    fnName = tokens_[j].value;
                    j++;
                }
                while (j < tokens_.size() && tokens_[j].type == TokenType::NEWLINE) j++;
                if (j < tokens_.size() && tokens_[j].type == TokenType::LPAREN) {
                    j++;
                    int count = 0;
                    while (j < tokens_.size() && tokens_[j].type != TokenType::RPAREN) {
                        if (tokens_[j].type == TokenType::IDENTIFIER) count++;
                        j++;
                    }
                    if (count > 5) {
                        warn(tokens_[i].line, "Function '" + fnName + "' has too many parameters (" + std::to_string(count) + " > 5)");
                    }
                }
            }
        }
    }

    void checkUnusedImports() {
        for (const auto& tok : tokens_) {
            if (tok.type == TokenType::IDENTIFIER && importedModules_.count(tok.value) > 0) {
                importedModuleNames_.insert(tok.value);
            }
        }
        for (const auto& mod : importedModules_) {
            if (importedModuleNames_.count(mod) == 0) {
                for (const auto& tok : tokens_) {
                    if (tok.type == TokenType::IMPORT && tok.value == mod) {
                        warn(tok.line, "Unused import '" + mod + "'");
                        break;
                    }
                }
            }
        }
    }

    void checkDeadCode() {
        for (size_t i = 0; i + 1 < tokens_.size(); i++) {
            if (tokens_[i].type == TokenType::RETURN || tokens_[i].type == TokenType::THROW) {
                size_t j = i + 1;
                while (j < tokens_.size() && tokens_[j].type == TokenType::NEWLINE) j++;
                if (j < tokens_.size() && tokens_[j].type != TokenType::RBRACE &&
                    tokens_[j].type != TokenType::EOF_TOKEN) {
                    if (tokens_[j].type == TokenType::IDENTIFIER ||
                        tokens_[j].type == TokenType::LET ||
                        tokens_[j].type == TokenType::IF ||
                        tokens_[j].type == TokenType::WHILE ||
                        tokens_[j].type == TokenType::FOR ||
                        tokens_[j].type == TokenType::FN) {
                        warn(tokens_[i].line, "Dead code after return/throw");
                    }
                }
            }
        }
    }
};

static std::string readFile(const std::string& path) {
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
        std::cerr << "Usage: forge-lint <file.fge> [file2.fge ...]\n";
        return 1;
    }

    int totalWarnings = 0;
    for (int f = 1; f < argc; f++) {
        std::string filename = argv[f];
        std::string source = readFile(filename);
        Linter linter(filename, source);
        auto warnings = linter.lint();
        for (const auto& w : warnings) {
            std::cout << filename << ":" << w.line << ": WARNING: " << w.message << "\n";
            totalWarnings++;
        }
    }

    if (totalWarnings == 0) {
        std::cout << "No warnings found.\n";
    } else {
        std::cout << "\n" << totalWarnings << " warning(s) found.\n";
    }

    return totalWarnings > 0 ? 1 : 0;
}
