#include "repl.hpp"
#include "terminal.hpp"
#include <cstdio>
#include <memory>
#include <array>
#include <cstring>

Repl::Repl() {}

Repl::~Repl() {}

int Repl::render(int startRow, int startCol, int height, int width) {
    auto& term = Terminal::instance();
    std::string output;
    output.reserve(height * 256);

    // Title bar
    output += ansi::move(startRow, startCol);
    output += ansi::BG_CYAN + ansi::FG_BOLD_WHITE + ansi::BOLD;
    std::string title = " REPL (F5 to toggle) ";
    int pad = width - (int)title.size();
    output += title;
    for (int i = 0; i < pad; i++) output += ' ';
    output += ansi::RESET;

    int contentHeight = height - 2;
    if (contentHeight < 1) contentHeight = 1;

    // Build display lines from entries
    std::vector<std::string> displayLines;
    for (auto& e : entries_) {
        std::string prefix;
        switch (e.type) {
            case ReplEntry::INPUT:  prefix = ansi::FG_BOLD_CYAN + std::string("forge> ") + ansi::RESET + ansi::FG_WHITE; break;
            case ReplEntry::OUTPUT: prefix = ansi::FG_WHITE; break;
            case ReplEntry::ERROR:  prefix = ansi::FG_BOLD_RED; break;
        }
        displayLines.push_back(prefix + e.text + ansi::RESET);
    }

    // Current input line
    std::string inputLine = ansi::FG_BOLD_CYAN + std::string("forge> ") + ansi::RESET + ansi::FG_BOLD_WHITE + inputBuffer_ + ansi::RESET;
    displayLines.push_back(inputLine);

    // Scroll to show latest
    if ((int)displayLines.size() > contentHeight) {
        scrollOffset_ = (int)displayLines.size() - contentHeight;
    } else {
        scrollOffset_ = 0;
    }

    for (int i = 0; i < contentHeight; i++) {
        int lineIdx = i + scrollOffset_;
        output += ansi::move(startRow + 1 + i, startCol);
        output += ansi::BG_BLACK;

        if (lineIdx < (int)displayLines.size()) {
            output += displayLines[lineIdx];
        }

        output += ansi::clearToEnd();
    }

    term.write(output);
    return 0;
}

void Repl::handleKey(int key) {
    switch (key) {
        case KEY_ENTER: {
            if (!inputBuffer_.empty()) {
                addEntry(ReplEntry::INPUT, inputBuffer_);
                history_.push_back(inputBuffer_);
                historyIdx_ = (int)history_.size();
                executeCommand(inputBuffer_);
            }
            inputBuffer_.clear();
            inputCursor_ = 0;
            break;
        }
        case KEY_BACKSPACE: {
            if (inputCursor_ > 0) {
                inputBuffer_.erase(inputCursor_ - 1, 1);
                inputCursor_--;
            }
            break;
        }
        case KEY_DELETE: {
            if (inputCursor_ < (int)inputBuffer_.size()) {
                inputBuffer_.erase(inputCursor_, 1);
            }
            break;
        }
        case KEY_LEFT: {
            if (inputCursor_ > 0) inputCursor_--;
            break;
        }
        case KEY_RIGHT: {
            if (inputCursor_ < (int)inputBuffer_.size()) inputCursor_++;
            break;
        }
        case KEY_HOME: {
            inputCursor_ = 0;
            break;
        }
        case KEY_END: {
            inputCursor_ = (int)inputBuffer_.size();
            break;
        }
        case KEY_UP: {
            if (!history_.empty() && historyIdx_ > 0) {
                historyIdx_--;
                inputBuffer_ = history_[historyIdx_];
                inputCursor_ = (int)inputBuffer_.size();
            }
            break;
        }
        case KEY_PAGE_UP: {
            if (!history_.empty() && historyIdx_ > 0) {
                historyIdx_ = 0;
                inputBuffer_ = history_[0];
                inputCursor_ = (int)inputBuffer_.size();
            }
            break;
        }
        case KEY_DOWN: {
            if (historyIdx_ < (int)history_.size() - 1) {
                historyIdx_++;
                inputBuffer_ = history_[historyIdx_];
                inputCursor_ = (int)inputBuffer_.size();
            } else {
                historyIdx_ = (int)history_.size();
                inputBuffer_.clear();
                inputCursor_ = 0;
            }
            break;
        }
        case KEY_PAGE_DOWN: {
            historyIdx_ = (int)history_.size();
            inputBuffer_.clear();
            inputCursor_ = 0;
            break;
        }
        default: {
            if (key >= 32 && key < 127) {
                inputBuffer_.insert(inputCursor_, 1, (char)key);
                inputCursor_++;
            }
            break;
        }
    }
}

void Repl::executeCommand(const std::string& cmd) {
    // Handle special commands
    if (cmd == "clear" || cmd == "cls") {
        entries_.clear();
        return;
    }

    if (cmd == "help") {
        addEntry(ReplEntry::OUTPUT, "Forge REPL commands:");
        addEntry(ReplEntry::OUTPUT, "  clear / cls  - Clear screen");
        addEntry(ReplEntry::OUTPUT, "  help         - Show this help");
        addEntry(ReplEntry::OUTPUT, "  exit         - Close REPL");
        addEntry(ReplEntry::OUTPUT, "");
        addEntry(ReplEntry::OUTPUT, "Type Forge code to evaluate it.");
        return;
    }

    if (cmd == "exit") {
        active_ = false;
        return;
    }

    // Execute via forge interpreter
    std::string fullCmd = forgePath_ + " -e \"" + cmd + "\" 2>&1";

    std::array<char, 4096> buffer;
    std::string result;

    FILE* pipe = popen(fullCmd.c_str(), "r");
    if (!pipe) {
        addEntry(ReplEntry::ERROR, "Error: Failed to run forge");
        return;
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    int status = pclose(pipe);

    // Remove trailing newline
    while (!result.empty() && result.back() == '\n') result.pop_back();

    if (!result.empty()) {
        if (status != 0) {
            addEntry(ReplEntry::ERROR, result);
        } else {
            addEntry(ReplEntry::OUTPUT, result);
        }
    }
}

void Repl::runCommand(const std::string& cmd) {
    addEntry(ReplEntry::INPUT, cmd);
    executeCommand(cmd);
}

void Repl::clear() {
    entries_.clear();
}

void Repl::addEntry(ReplEntry::Type type, const std::string& text) {
    entries_.push_back({type, text});
    if (entries_.size() > 500) {
        entries_.erase(entries_.begin(), entries_.begin() + 100);
    }
}
