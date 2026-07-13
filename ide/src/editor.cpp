#include "editor.hpp"
#include "terminal.hpp"
#include "theme.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <climits>
#include <sys/stat.h>
#include <filesystem>

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
    selection_ = Selection{};
    clipboard_ = "";
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
        scrollCol_ = 0;
        undoStack_.clear();
        redoStack_.clear();
        selection_ = Selection{};
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
    selection_ = Selection{};
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

int Editor::render(int startRow, int startCol, int height, int width, bool showMinimap) {
    auto& term = Terminal::instance();
    std::string output;
    output.reserve(height * 512);

    const Theme::Colors& theme = Theme::getCurrent();

    int minimapWidth = showMinimap ? std::min(20, width / 5) : 0;
    int lineNumWidth = 5;
    int gutterWidth = lineNumWidth + 1;
    int textWidth = width - gutterWidth - minimapWidth - 1;

    // Adjust scroll to keep cursor visible
    if (cursorLine_ < scrollOffset_) scrollOffset_ = cursorLine_;
    if (cursorLine_ >= scrollOffset_ + height) scrollOffset_ = cursorLine_ - height + 1;
    if (scrollOffset_ < 0) scrollOffset_ = 0;

    // Render main editor area
    for (int i = 0; i < height; i++) {
        int lineIdx = i + scrollOffset_;
        output += ansi::move(startRow + i, startCol);
        output += ansi::clearLine();

        if (lineIdx < (int)lines_.size()) {
            bool isCurrentLine = (lineIdx == cursorLine_);
            bool hasSelectionOnLine = selection_.active &&
                lineIdx >= selection_.startLine && lineIdx <= selection_.endLine;

            // Line number gutter
            if (isCurrentLine) {
                output += theme.bg_gutter_current + theme.fg_gutter_current;
            } else {
                output += theme.bg_gutter + theme.fg_gutter;
            }

            char numBuf[16];
            snprintf(numBuf, sizeof(numBuf), "%*d ", lineNumWidth, lineIdx + 1);
            output += numBuf;
            output += ansi::RESET;

            // Text area background
            if (isCurrentLine) {
                output += theme.bg_line_current;
            } else {
                output += theme.bg_editor;
            }

            // Render with syntax highlighting
            const std::string& line = lines_[lineIdx];
            auto spans = highlighter_.highlightLine(line, lineIdx);

            // Handle horizontal scroll
            int displayCol = 0;
            int srcCol = 0;
            for (auto& span : spans) {
                for (int j = 0; j < span.length; j++) {
                    if (displayCol >= scrollCol_ && displayCol < scrollCol_ + textWidth) {
                        // Check if character is selected
                        bool isSelected = hasSelectionOnLine &&
                            displayCol >= (lineIdx == selection_.startLine ? selection_.startCol : 0) &&
                            displayCol < (lineIdx == selection_.endLine ? selection_.endCol : (int)line.size());

                        if (isSelected) {
                            output += theme.bg_selection;
                        }

                        std::string color = SyntaxHighlighter::colorForToken(span.type);
                        output += color;
                        output += line[srcCol];
                        output += ansi::RESET;

                        if (isCurrentLine) output += theme.bg_line_current;
                        else output += theme.bg_editor;

                        if (isSelected) output += ansi::RESET + theme.bg_editor;
                    }
                    displayCol++;
                    srcCol++;
                }
            }

            // Cursor highlight on current line
            if (isCurrentLine && !hasSelectionOnLine) {
                int cursorDisplayCol = cursorCol_ - scrollCol_;
                if (cursorDisplayCol >= 0 && cursorDisplayCol < textWidth) {
                    output += ansi::move(startRow + i, startCol + gutterWidth + cursorDisplayCol);
                    output += ansi::REVERSE;
                    if (cursorCol_ < (int)line.size()) {
                        output += line[cursorCol_];
                    } else {
                        output += ' ';
                    }
                    output += ansi::RESET;
                }
            }

            // Fill rest of line
            if (displayCol < scrollCol_ + textWidth) {
                for (int j = displayCol - scrollCol_; j < textWidth; j++) output += ' ';
            }
        } else {
            // Empty lines (~)
            output += theme.bg_gutter + theme.fg_gutter;
            for (int j = 0; j < gutterWidth; j++) output += '~';
            output += ansi::RESET;
            output += theme.bg_editor;
            for (int j = 0; j < textWidth; j++) output += ' ';
        }
    }

    // Render minimap if enabled
    if (showMinimap && minimapWidth > 0) {
        renderMinimap(startRow, startCol + gutterWidth + textWidth + 1, height, minimapWidth);
    }

    term.write(output);
    return scrollOffset_;
}

void Editor::renderMinimap(int startRow, int startCol, int height, int width) {
    auto& term = Terminal::instance();
    const Theme::Colors& theme = Theme::getCurrent();
    int totalLines = (int)lines_.size();

    std::string output;
    output.reserve(height * (width + 10));

    for (int i = 0; i < height; i++) {
        int lineIdx = (int)((double)i / height * totalLines);
        if (lineIdx >= totalLines) break;

        output += ansi::move(startRow + i, startCol);
        output += ansi::clearLine();
        output += theme.bg_panel;

        const std::string& line = lines_[lineIdx];
        bool isCurrentLine = (lineIdx == cursorLine_);
        bool hasSel = selection_.active && lineIdx >= selection_.startLine && lineIdx <= selection_.endLine;

        if (isCurrentLine) {
            output += theme.accent1 + "█" + ansi::RESET;
        } else if (hasSel) {
            output += theme.accent2 + "░" + ansi::RESET;
        } else if (!line.empty()) {
            output += theme.fg_comment + "░" + ansi::RESET;
        } else {
            output += " ";
        }
    }

    term.write(output);
}

std::string Editor::getMinimapColor(int lineIdx) const {
    const Theme::Colors& theme = Theme::getCurrent();
    if (lineIdx == cursorLine_) return theme.accent1;
    if (selection_.active && lineIdx >= selection_.startLine && lineIdx <= selection_.endLine) return theme.accent2;
    return theme.fg_comment;
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

// Editing
void Editor::insertChar(char c) {
    if (selection_.active) {
        deleteSelection();
    }
    pushUndo(UndoAction::INSERT, cursorLine_, cursorCol_, 1, std::string(1, c));
    lines_[cursorLine_].insert(cursorCol_, 1, c);
    cursorCol_++;
    modified_ = true;
}

void Editor::insertString(const std::string& s) {
    for (char c : s) {
        if (c == '\n') insertNewline();
        else insertChar(c);
    }
}

void Editor::deleteChar() {
    if (selection_.active) {
        deleteSelection();
        return;
    }
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
    if (selection_.active) {
        deleteSelection();
        return;
    }
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

void Editor::insertTab() {
    if (selection_.active) deleteSelection();
    pushUndo(UndoAction::INSERT, cursorLine_, cursorCol_, 4, "    ");
    lines_[cursorLine_].insert(cursorCol_, "    ");
    cursorCol_ += 4;
    modified_ = true;
}

void Editor::insertNewline() {
    if (selection_.active) deleteSelection();

    std::string after = lines_[cursorLine_].substr(cursorCol_);
    lines_[cursorLine_] = lines_[cursorLine_].substr(0, cursorCol_);

    // Auto-indent
    std::string indent;
    for (char c : lines_[cursorLine_]) {
        if (c == ' ' || c == '\t') indent += c;
        else break;
    }

    // Add extra indent after { or :
    std::string trimmed = lines_[cursorLine_];
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    if (!trimmed.empty() && (trimmed.back() == '{' || trimmed.back() == ':')) {
        indent += "    ";
    }

    cursorLine_++;
    lines_.insert(lines_.begin() + cursorLine_, indent + after);
    cursorCol_ = (int)indent.size();

    pushUndo(UndoAction::INSERT, cursorLine_, 0, 0, "\n");
    modified_ = true;
}

// Navigation
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
    // Smart home - first non-whitespace, then column 0
    const std::string& line = lines_[cursorLine_];
    int firstNonSpace = 0;
    while (firstNonSpace < (int)line.size() && (line[firstNonSpace] == ' ' || line[firstNonSpace] == '\t')) {
        firstNonSpace++;
    }
    if (cursorCol_ == firstNonSpace) cursorCol_ = 0;
    else cursorCol_ = firstNonSpace;
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

// Selection
void Editor::startSelection() {
    if (!selection_.active) {
        selection_.startLine = cursorLine_;
        selection_.startCol = cursorCol_;
        selection_.endLine = cursorLine_;
        selection_.endCol = cursorCol_;
        selection_.active = true;
    }
}

void Editor::clearSelection() {
    selection_ = Selection{};
}

void Editor::deleteSelection() {
    if (!selection_.active) return;

    int startLine = selection_.startLine;
    int startCol = selection_.startCol;
    int endLine = selection_.endLine;
    int endCol = selection_.endCol;

    // Normalize
    if (startLine > endLine || (startLine == endLine && startCol > endCol)) {
        std::swap(startLine, endLine);
        std::swap(startCol, endCol);
    }

    std::string deletedText;
    for (int i = startLine; i <= endLine; i++) {
        int s = (i == startLine) ? startCol : 0;
        int e = (i == endLine) ? endCol : (int)lines_[i].size();
        deletedText += lines_[i].substr(s, e - s);
        if (i < endLine) deletedText += "\n";
    }

    pushUndo(UndoAction::DELETE_CHARS, startLine, startCol, endLine - startLine + 1, deletedText);

    if (startLine == endLine) {
        lines_[startLine].erase(startCol, endCol - startCol);
    } else {
        std::string before = lines_[startLine].substr(0, startCol);
        std::string after = lines_[endLine].substr(endCol);
        lines_[startLine] = before + after;
        lines_.erase(lines_.begin() + startLine + 1, lines_.begin() + endLine + 1);
    }

    cursorLine_ = startLine;
    cursorCol_ = startCol;
    clearSelection();
    modified_ = true;
}

void Editor::copySelection() {
    if (!selection_.active) return;

    int startLine = selection_.startLine;
    int startCol = selection_.startCol;
    int endLine = selection_.endLine;
    int endCol = selection_.endCol;

    if (startLine > endLine || (startLine == endLine && startCol > endCol)) {
        std::swap(startLine, endLine);
        std::swap(startCol, endCol);
    }

    clipboard_.clear();
    for (int i = startLine; i <= endLine; i++) {
        int s = (i == startLine) ? startCol : 0;
        int e = (i == endLine) ? endCol : (int)lines_[i].size();
        clipboard_ += lines_[i].substr(s, e - s);
        if (i < endLine) clipboard_ += "\n";
    }
}

void Editor::cutSelection() {
    copySelection();
    deleteSelection();
}

void Editor::paste() {
    if (clipboard_.empty()) return;
    if (selection_.active) deleteSelection();
    insertString(clipboard_);
}

// Undo/Redo
void Editor::undo() {
    if (undoStack_.empty()) return;
    auto action = undoStack_.back();
    undoStack_.pop_back();

    clearSelection();

    switch (action.type) {
        case UndoAction::INSERT: {
            if (!action.text.empty() && action.text[0] == '\n') {
                // Undo newline
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

    clearSelection();

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

// Search
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

int Editor::findPrev(const std::string& query) {
    if (query.empty()) return -1;
    for (int i = cursorLine_; i >= 0; i--) {
        size_t end = (i == cursorLine_) ? cursorCol_ : std::string::npos;
        size_t pos = lines_[i].rfind(query, end);
        if (pos != std::string::npos && (i != cursorLine_ || pos < (size_t)cursorCol_)) {
            cursorLine_ = i;
            cursorCol_ = (int)pos;
            return i;
        }
    }
    // Wrap around
    for (int i = (int)lines_.size() - 1; i >= cursorLine_; i--) {
        size_t pos = lines_[i].rfind(query);
        if (pos != std::string::npos) {
            cursorLine_ = i;
            cursorCol_ = (int)pos;
            return i;
        }
    }
    return -1;
}

void Editor::replaceCurrent(const std::string& replacement) {
    if (selection_.active) {
        deleteSelection();
    }
    insertString(replacement);
}

void Editor::replaceAll(const std::string& find, const std::string& replace) {
    if (find.empty()) return;
    int count = 0;
    for (size_t i = 0; i < lines_.size(); i++) {
        size_t pos = 0;
        while ((pos = lines_[i].find(find, pos)) != std::string::npos) {
            lines_[i].replace(pos, find.size(), replace);
            pos += replace.size();
            count++;
        }
    }
    if (count > 0) modified_ = true;
}

// Cursor
void Editor::setCursor(int line, int col) {
    cursorLine_ = line;
    cursorCol_ = col;
    clampCursor();
}

// File info
std::string Editor::getFileName() const {
    if (filePath_.empty()) return "untitled";
    size_t pos = filePath_.find_last_of("/\\");
    return pos == std::string::npos ? filePath_ : filePath_.substr(pos + 1);
}

std::string Editor::getLanguage() const {
    if (filePath_.empty()) return "Forge";
    size_t pos = filePath_.find_last_of('.');
    if (pos == std::string::npos) return "Forge";
    std::string ext = filePath_.substr(pos + 1);
    if (ext == "fge") return "Forge";
    if (ext == "cpp" || ext == "cc" || ext == "cxx" || ext == "hpp" || ext == "h") return "C++";
    if (ext == "c" || ext == "h") return "C";
    if (ext == "py") return "Python";
    if (ext == "js" || ext == "ts") return "JavaScript/TypeScript";
    if (ext == "rs") return "Rust";
    if (ext == "go") return "Go";
    return "Text";
}

// Bracket matching
std::pair<int, int> Editor::findMatchingBracket(int line, int col) const {
    if (line >= (int)lines_.size() || col >= (int)lines_[line].size()) return {-1, -1};
    char c = lines_[line][col];
    char match = 0;
    if (c == '(') match = ')';
    else if (c == '[') match = ']';
    else if (c == '{') match = '}';
    else if (c == ')') match = '(';
    else if (c == ']') match = '[';
    else if (c == '}') match = '{';
    else return {-1, -1};

    int dir = (c == '(' || c == '[' || c == '{') ? 1 : -1;
    int count = 1;
    int curLine = line;
    int curCol = col + dir;

    while (true) {
        if (curLine < 0 || curLine >= (int)lines_.size()) break;
        const std::string& l = lines_[curLine];
        if (curCol < 0) {
            if (curLine == 0) break;
            curLine--;
            curCol = (int)lines_[curLine].size();
            continue;
        }
        if (curCol >= (int)l.size()) {
            if (curLine == (int)lines_.size() - 1) break;
            curLine++;
            curCol = 0;
            continue;
        }

        char ch = l[curCol];
        if (ch == c) count++;
        else if (ch == match) count--;
        if (count == 0) return {curLine, curCol};

        curCol += dir;
    }
    return {-1, -1};
}