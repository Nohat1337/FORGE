#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace forge::lsp {

struct Position {
    int line = 0;
    int character = 0;
};

struct Range {
    Position start;
    Position end;
};

struct TextEdit {
    Range range;
    std::string newText;
};

struct CompletionItem {
    std::string label;
    int kind = 1; // 1=Text, 2=Method, 3=Function, 4=Constructor, 5=Field, 6=Variable
    std::string detail;
    std::string documentation;
    std::string insertText;
};

struct Diagnostic {
    Range range;
    int severity = 1; // 1=Error, 2=Warning, 3=Information, 4=Hint
    std::string message;
    std::string source = "forge";
};

struct HoverResult {
    std::string contents;
    Range range;
};

class ForgeLSP {
public:
    ForgeLSP();
    void run();  // Main loop reading JSON-RPC from stdin, writing to stdout
    
private:
    // JSON-RPC message handling
    void handleRequest(const std::string& method, int id, const std::string& params);
    void handleNotification(const std::string& method, const std::string& params);
    void sendResponse(int id, const std::string& result);
    void sendError(int id, int code, const std::string& message);
    void sendNotification(const std::string& method, const std::string& params);
    
    // LSP methods
    std::string handleInitialize(int id, const std::string& params);
    void handleInitialized(const std::string& params);
    void handleDidOpen(const std::string& params);
    void handleDidChange(const std::string& params);
    void handleDidSave(const std::string& params);
    std::string handleCompletion(int id, const std::string& params);
    std::string handleHover(int id, const std::string& params);
    std::string handleDefinition(int id, const std::string& params);
    std::string handleReferences(int id, const std::string& params);
    
    // Forge source analysis
    std::vector<Diagnostic> analyzeSource(const std::string& source, const std::string& uri);
    std::vector<CompletionItem> getCompletions(const std::string& source, Position pos);
    HoverResult getHover(const std::string& source, Position pos);
    std::vector<Range> findDefinition(const std::string& source, const std::string& word);
    std::vector<Range> findReferences(const std::string& source, const std::string& word);
    
    // Document storage
    struct Document {
        std::string uri;
        std::string languageId;
        std::string content;
        int version = 0;
    };
    std::map<std::string, Document> documents_;
    
    // Keywords and builtins for completion
    std::vector<std::string> keywords_ = {
        "let", "const", "fn", "return", "if", "else", "while", "for",
        "class", "extends", "this", "super", "new", "import", "as",
        "match", "case", "throw", "try", "catch", "finally", "gen",
        "yield", "break", "continue", "struct", "enum", "impl", "extern",
        "true", "false", "nil", "and", "or", "not", "in"
    };
    
    std::vector<std::string> builtins_ = {
        "print", "len", "push", "str", "int", "float", "type", "error",
        "keys", "values", "has", "entries", "clone", "upper", "lower",
        "trim", "split", "contains", "replace", "substring", "charAt",
        "parseInt", "abs", "min", "max", "sqrt", "pow", "floor", "ceil",
        "round", "random", "randomInt"
    };
    
    std::vector<std::string> modules_ = {
        "io", "os", "json", "path", "system", "ui", "sdl2", "test", "fs",
        "ffi", "concurrent", "llm", "net"
    };
    
    std::string getWordAt(const std::string& source, Position pos);
    int getLineStart(const std::string& source, int line);
    Position offsetToPosition(const std::string& source, int offset);
    int positionToOffset(const std::string& source, Position pos);
};

} // namespace forge::lsp
