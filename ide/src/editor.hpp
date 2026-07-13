#pragma once

#include <string>
#include <vector>
#include "syntax.hpp"

struct UndoAction {
    enum Type { INSERT, DELETE_LINE, DELETE_CHARS };
    Type type;
    int line;
    int col;
    int count;
    std::string text;
};

class Editor {
public:
    Editor();
    ~Editor();

    void loadFile(const std::string& path);
    void saveFile(const std::string& path);
    bool isModified() const { return modified_; }
    const std::string& getFilePath() const { return filePath_; }
    void setFilePath(const std::string& path) { filePath_ = path; }
    void setModified(bool m) { modified_ = m; }

    int render(int startRow, int startCol, int height, int width);

    // Editing
    void insertChar(char c);
    void insertString(const std::string& s);
    void deleteChar();
    void backspace();
    void insertNewline();

    // Navigation
    void moveUp();
    void moveDown();
    void moveLeft();
    void moveRight();
    void moveHome();
    void moveEnd();
    void pageUp(int height);
    void pageDown(int height);
    void moveToLine(int line);
    void moveToStart();
    void moveToEnd();

    // Editing operations
    void deleteLine();
    void insertTab();

    // Undo/Redo
    void undo();
    void redo();

    // Selection (basic)
    // Search
    int findNext(const std::string& query);
    void replaceCurrent(const std::string& replacement);

    int getCursorLine() const { return cursorLine_; }
    int getCursorCol() const { return cursorCol_; }
    int getLineCount() const { return (int)lines_.size(); }
    int getScrollOffset() const { return scrollOffset_; }
    const std::string& getLine(int idx) const { return lines_[idx]; }

    void setScrollOffset(int s) { scrollOffset_ = s; }

    std::string getSelectedText() const;

    // For status bar
    int getTotalLines() const { return (int)lines_.size(); }

    void newFile();

private:
    std::vector<std::string> lines_;
    std::string filePath_;
    bool modified_ = false;
    int cursorLine_ = 0;
    int cursorCol_ = 0;
    int scrollOffset_ = 0;
    int scrollCol_ = 0;

    SyntaxHighlighter highlighter_;

    std::vector<UndoAction> undoStack_;
    std::vector<UndoAction> redoStack_;

    void ensureCursorValid();
    void clampCursor();
    void pushUndo(UndoAction::Type type, int line, int col, int count, const std::string& text = "");
    void renderLine(int row, int lineIdx, int width);
};
