#include "editor.hpp"
#include "terminal.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <climits>
#include <sys/stat.h>

Editor::Editor() : highlighter_() {
    lines_.push_back("");
}

Editor::~Editor() {}

void Editor::newFile() {
    lines_.clear();
    lines_.push_back("");
    filePath_ = "";
    modified_ = false;
    cursorLine_ = 0;
    cursorCol_ = 0;
    scrollOffset_ = 0;
    scrollCol_ = 0;
    undoStack_.clear();
    redoStack_.clear();
}

void Editor::loadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        lines_.clear();
        lines_.push_back("");
        filePath_ = path;
        modified_ = false;
        cursorLine_ = 0;
        cursorCol_ = 0;
        scrollOffset_ = 0;
        return;
    }

    lines_.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines_.push_back(line);
    }
    if (lines_.empty()) lines_.push_back("");

    filePath_ = path;
    modified_ = false;
    cursorLine_ = 0;
    cursorCol_ = 0;
    scrollOffset_ = 0;
    scrollCol_ = 0;
    undoStack_.clear();
    redoStack_.clear();
}

void Editor::saveFile(const std::string& path) {
    std::string savePath = path.empty() ? filePath_ : path;
    if (savePath.empty()) return;

    std::ofstream file(savePath);
    for (size_t i = 0; i < lines_.size(); i++) {
        file << lines_[i];
        if (i < lines_.size() - 1) file << '\n';
    }
    file.close();

    filePath_ = savePath;
    modified_ = false;
}

int Editor::render(int startRow, int startCol, int height, int width) {
    auto& term = Terminal::instance();
    std::string output;
    output.reserve(height * 256);

    int lineNumWidth = 5;
    int gutterWidth = lineNumWidth + 1;
    int textWidth = width - gutterWidth;

    // Adjust scroll to keep cursor visible
    if (cursorLine_ < scrollOffset_) scrollOffset_ = cursorLine_;
    if (cursorLine_ >= scrollOffset_ + height) scrollOffset_ = cursorLine_ - height + 1;
    if (scrollOffset_ < 0) scrollOffset_ = 0;

    for (int i = 0; i < height; i++) {
        int lineIdx = i + scrollOffset_;
        output += ansi::move(startRow + i, startCol);
        output += ansi::CLEAR;

        // Line number gutter
        if (lineIdx < (int)lines_.size()) {
            bool isCurrentLine = (lineIdx == cursorLine_);
            if (isCurrentLine) {
                output += ansi::BG_BLUE + ansi::FG_BOLD_WHITE;
            } else {
                output += ansi::BG_PANEL + ansi::FG_GRAY;
            }

            char numBuf[16];
            snprintf(numBuf, sizeof(numBuf), "%*d ", lineNumWidth, lineIdx + 1);
            output += numBuf;
            output += ansi::RESET;

            // Text area background
            output += ansi::BG_BLACK;

            // Render with syntax highlighting
            const std::string& line = lines_[lineIdx];
            auto spans = highlighter_.highlightLine(line);

            // Handle scroll column
            int displayCol = 0;
            int srcCol = 0;
            for (auto& span : spans) {
                for (int j = 0; j < span.length; j++) {
                    if (displayCol >= scrollCol_ && displayCol < scrollCol_ + textWidth) {
                        std::string color = SyntaxHighlighter::colorForToken(span.type);
                        output += color;
                        output += line[srcCol];
                        output += ansi::RESET + ansi::BG_BLACK;
                    }
                    displayCol++;
                    srcCol++;
                }
            }

            // Cursor highlight
            if (isCurrentLine) {
                int cursorDisplayCol = cursorCol_ - scrollCol_;
                if (cursorDisplayCol >= 0 && cursorDisplayCol < textWidth) {
                    // Position cursor and show it
                    output += ansi::move(startRow + i, startCol + gutterWidth + cursorDisplayCol);
                    output += ansi::BG_WHITE + ansi::FG_BLACK;
                    if (cursorCol_ < (int)line.size()) {
                        output += line[cursorCol_];
                    } else {
                        output += ' ';
                    }
                    output += ansi::RESET + ansi::BG_BLACK;
                }
            }

            // Fill rest of line
            if (displayCol < scrollCol_ + textWidth) {
                for (int j = displayCol - scrollCol_; j < textWidth; j++) output += ' ';
            }
        } else {
            output += ansi::BG_PANEL + ansi::FG_GRAY;
            for (int j = 0; j < gutterWidth; j++) output += '~';
            output += ansi::RESET;
            output += ansi::BG_BLACK;
            for (int j = 0; j < textWidth; j++) output += ' ';
        }
    }

    term.write(output);
    return scrollOffset_;
}

void Editor::ensureCursorValid() {
    clampCursor();
}

void Editor::clampCursor() {
    if (cursorLine_ < 0) cursorLine_ = 0;
    if (cursorLine_ >= (int)lines_.size()) cursorLine_ = (int)lines_.size() - 1;
    if (cursorCol_ < 0) cursorCol_ = 0;
    if (cursorCol_ > (int)lines_[cursorLine_].size()) cursorCol_ = (int)lines_[cursorLine_].size();
}

void Editor::pushUndo(UndoAction::Type type, int line, int col, int count, const std::string& text) {
    undoStack_.push_back({type, line, col, count, text});
    if (undoStack_.size() > 1000) undoStack_.erase(undoStack_.begin());
    redoStack_.clear();
}

void Editor::insertChar(char c) {
    pushUndo(UndoAction::INSERT, cursorLine_, cursorCol_, 1, std::string(1, c));
    lines_[cursorLine_].insert(cursorCol_, 1, c);
    cursorCol_++;
    modified_ = true;
}

void Editor::insertString(const std::string& s) {
    for (char c : s) {
        if (c == '\n') {
            insertNewline();
        } else {
            insertChar(c);
        }
    }
}

void Editor::deleteChar() {
    if (cursorLine_ >= (int)lines_.size() - 1 && cursorCol_ >= (int)lines_[cursorLine_].size()) return;

    if (cursorCol_ >= (int)lines_[cursorLine_].size()) {
        // Merge with next line
        pushUndo(UndoAction::DELETE_LINE, cursorLine_, cursorCol_, 0, lines_[cursorLine_ + 1]);
        lines_[cursorLine_] += lines_[cursorLine_ + 1];
        lines_.erase(lines_.begin() + cursorLine_ + 1);
    } else {
        char deleted = lines_[cursorLine_][cursorCol_];
        pushUndo(UndoAction::DELETE_CHARS, cursorLine_, cursorCol_, 1, std::string(1, deleted));
        lines_[cursorLine_].erase(cursorCol_, 1);
    }
    modified_ = true;
}

void Editor::backspace() {
    if (cursorCol_ == 0 && cursorLine_ == 0) return;

    if (cursorCol_ == 0) {
        // Merge with previous line
        pushUndo(UndoAction::DELETE_LINE, cursorLine_ - 1, (int)lines_[cursorLine_ - 1].size(), 0, lines_[cursorLine_]);
        cursorCol_ = (int)lines_[cursorLine_ - 1].size();
        lines_[cursorLine_ - 1] += lines_[cursorLine_];
        lines_.erase(lines_.begin() + cursorLine_);
        cursorLine_--;
    } else {
        char deleted = lines_[cursorLine_][cursorCol_ - 1];
        pushUndo(UndoAction::DELETE_CHARS, cursorLine_, cursorCol_ - 1, 1, std::string(1, deleted));
        lines_[cursorLine_].erase(cursorCol_ - 1, 1);
        cursorCol_--;
    }
    modified_ = true;
}

void Editor::insertNewline() {
    std::string after = lines_[cursorLine_].substr(cursorCol_);
    lines_[cursorLine_] = lines_[cursorLine_].substr(0, cursorCol_);

    // Auto-indent: copy leading whitespace
    std::string indent;
    for (char c : lines_[cursorLine_]) {
        if (c == ' ' || c == '\t') indent += c;
        else break;
    }

    cursorLine_++;
    lines_.insert(lines_.begin() + cursorLine_, indent + after);
    cursorCol_ = (int)indent.size();

    pushUndo(UndoAction::INSERT, cursorLine_, 0, 0, "\n");
    modified_ = true;
}

void Editor::moveUp() {
    if (cursorLine_ > 0) {
        cursorLine_--;
        clampCursor();
    }
}

void Editor::moveDown() {
    if (cursorLine_ < (int)lines_.size() - 1) {
        cursorLine_++;
        clampCursor();
    }
}

void Editor::moveLeft() {
    if (cursorCol_ > 0) {
        cursorCol_--;
    } else if (cursorLine_ > 0) {
        cursorLine_--;
        cursorCol_ = (int)lines_[cursorLine_].size();
    }
}

void Editor::moveRight() {
    if (cursorCol_ < (int)lines_[cursorLine_].size()) {
        cursorCol_++;
    } else if (cursorLine_ < (int)lines_.size() - 1) {
        cursorLine_++;
        cursorCol_ = 0;
    }
}

void Editor::moveHome() {
    cursorCol_ = 0;
}

void Editor::moveEnd() {
    cursorCol_ = (int)lines_[cursorLine_].size();
}

void Editor::pageUp(int height) {
    cursorLine_ -= height;
    if (cursorLine_ < 0) cursorLine_ = 0;
    clampCursor();
}

void Editor::pageDown(int height) {
    cursorLine_ += height;
    if (cursorLine_ >= (int)lines_.size()) cursorLine_ = (int)lines_.size() - 1;
    clampCursor();
}

void Editor::moveToLine(int line) {
    cursorLine_ = line - 1;
    clampCursor();
}

void Editor::moveToStart() {
    cursorLine_ = 0;
    cursorCol_ = 0;
}

void Editor::moveToEnd() {
    cursorLine_ = (int)lines_.size() - 1;
    cursorCol_ = (int)lines_[cursorLine_].size();
}

void Editor::deleteLine() {
    if (lines_.size() <= 1) {
        pushUndo(UndoAction::DELETE_LINE, 0, 0, 0, lines_[0]);
        lines_[0] = "";
        cursorCol_ = 0;
    } else {
        pushUndo(UndoAction::DELETE_LINE, cursorLine_, 0, 0, lines_[cursorLine_]);
        lines_.erase(lines_.begin() + cursorLine_);
        clampCursor();
    }
    modified_ = true;
}

void Editor::insertTab() {
    for (int i = 0; i < 4; i++) insertChar(' ');
}

void Editor::undo() {
    if (undoStack_.empty()) return;
    auto action = undoStack_.back();
    undoStack_.pop_back();

    switch (action.type) {
        case UndoAction::INSERT: {
            if (!action.text.empty() && action.text[0] == '\n') {
                // Undo newline: merge lines
                std::string content = lines_[action.line];
                if (action.line + 1 < (int)lines_.size()) {
                    lines_[action.line] = lines_[action.line].substr(0, action.col);
                    lines_[action.line] += lines_[action.line + 1];
                    lines_.erase(lines_.begin() + action.line + 1);
                }
                cursorLine_ = action.line;
                cursorCol_ = action.col;
            } else {
                // Undo char insert
                lines_[action.line].erase(action.col, 1);
                cursorLine_ = action.line;
                cursorCol_ = action.col;
            }
            break;
        }
        case UndoAction::DELETE_CHARS: {
            lines_[action.line].insert(action.col, action.text);
            cursorLine_ = action.line;
            cursorCol_ = action.col + 1;
            break;
        }
        case UndoAction::DELETE_LINE: {
            lines_.insert(lines_.begin() + action.line, action.text);
            cursorLine_ = action.line;
            cursorCol_ = action.col;
            break;
        }
    }

    redoStack_.push_back(action);
    modified_ = true;
    clampCursor();
}

void Editor::redo() {
    if (redoStack_.empty()) return;
    auto action = redoStack_.back();
    redoStack_.pop_back();

    switch (action.type) {
        case UndoAction::INSERT: {
            if (!action.text.empty() && action.text[0] == '\n') {
                std::string after = lines_[cursorLine_].substr(action.col);
                lines_[cursorLine_] = lines_[cursorLine_].substr(0, action.col);
                lines_.insert(lines_.begin() + cursorLine_ + 1, after);
                cursorLine_++;
                cursorCol_ = 0;
            } else {
                lines_[action.line].insert(action.col, action.text);
                cursorLine_ = action.line;
                cursorCol_ = action.col + 1;
            }
            break;
        }
        case UndoAction::DELETE_CHARS: {
            lines_[action.line].erase(action.col, 1);
            cursorLine_ = action.line;
            cursorCol_ = action.col;
            break;
        }
        case UndoAction::DELETE_LINE: {
            lines_.erase(lines_.begin() + action.line);
            clampCursor();
            break;
        }
    }

    undoStack_.push_back(action);
    modified_ = true;
    clampCursor();
}

int Editor::findNext(const std::string& query) {
    if (query.empty()) return -1;
    for (int i = cursorLine_; i < (int)lines_.size(); i++) {
        size_t start = (i == cursorLine_) ? cursorCol_ + 1 : 0;
        size_t pos = lines_[i].find(query, start);
        if (pos != std::string::npos) {
            cursorLine_ = i;
            cursorCol_ = (int)pos;
            return i;
        }
    }
    // Wrap around
    for (int i = 0; i <= cursorLine_; i++) {
        size_t pos = lines_[i].find(query);
        if (pos != std::string::npos) {
            cursorLine_ = i;
            cursorCol_ = (int)pos;
            return i;
        }
    }
    return -1;
}

void Editor::replaceCurrent(const std::string& replacement) {
    lines_[cursorLine_].replace(cursorCol_, replacement.size(), replacement);
    modified_ = true;
}

std::string Editor::getSelectedText() const {
    return "";
}

void Editor::renderLine(int row, int lineIdx, int width) {
    // Used internally, rendering is done in render()
}
