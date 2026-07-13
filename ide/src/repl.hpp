#pragma once

#include <string>
#include <vector>
#include <functional>

struct ReplEntry {
    enum Type { INPUT, OUTPUT, ERROR };
    Type type;
    std::string text;
};

class Repl {
public:
    Repl();
    ~Repl();

    void setForgePath(const std::string& path) { forgePath_ = path; }

    int render(int startRow, int startCol, int height, int width);
    void handleKey(int key);

    bool isActive() const { return active_; }
    void toggle() { active_ = !active_; }
    void activate() { active_ = true; }
    void deactivate() { active_ = false; }

    std::string getPrompt() const { return "forge> "; }

    void runCommand(const std::string& cmd);
    void clear();

private:
    bool active_ = false;
    std::string inputBuffer_;
    int inputCursor_ = 0;
    std::vector<ReplEntry> entries_;
    std::vector<std::string> history_;
    int historyIdx_ = -1;
    std::string forgePath_ = "forge";
    int scrollOffset_ = 0;

    void executeCommand(const std::string& cmd);
    void addEntry(ReplEntry::Type type, const std::string& text);
};
