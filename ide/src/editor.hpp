#pragma once

#include <string>
#include <vector>
#include "syntax.hpp"
#include "theme.hpp"

struct UndoAction {
    enum Type { INSERT, DELETE_LINE, DELETE_CHARS };
    Type type;
    int line;
    int col;
    int count;
    std::string text;
};

struct Selection {
    int startLine = -1;
    int startCol = -1;
    int endLine = -1;
    int endCol = -1;
    bool active = false;
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

    int render(int startRow, int startCol, int height, int width, bool showMinimap = false);

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

    // Selection
    void startSelection();
    void clearSelection();
    bool hasSelection() const { return selection_.active; }
    Selection getSelection() const { return selection_; }

    // Editing operations
    void deleteLine();
    void insertTab();
    void deleteSelection();
    void copySelection();
    void cutSelection();
    void paste();

    // Undo/Redo
    void undo();
    void redo();

    // Search
    int findNext(const std::string& query);
    int findPrev(const std::string& query);
    void replaceCurrent(const std::string& replacement);
    void replaceAll(const std::string& find, const std::string& replace);

    // Cursor
    int getCursorLine() const { return cursorLine_; }
    int getCursorCol() const { return cursorCol_; }
    void setCursor(int line, int col);
    void setScrollOffset(int s) { scrollOffset_ = s; }
    int getScrollOffset() const { return scrollOffset_; }

    // Line access
    int getLineCount() const { return (int)lines_.size(); }
    const std::string& getLine(int idx) const { return lines_[idx]; }

    // Status
    int getTotalLines() const { return (int)lines_.size(); }
    std::string getFileName() const;
    std::string getLanguage() const;

    void newFile();
    void setTheme(Theme::Type theme) { currentTheme_ = theme; }
    Theme::Type getTheme() const { return currentTheme_; }

    // Brackets matching
    std::pair<int, int> findMatchingBracket(int line, int col) const;

private:
    std::vector<std::string> lines_;
    std::string filePath_;
    bool modified_ = false;
    int cursorLine_ = 0;
    int cursorCol_ = 0;
    int scrollOffset_ = 0;
    int scrollCol_ = 0;

    SyntaxHighlighter highlighter_;
    Theme::Type currentTheme_ = Theme::Type::DARK;

    std::vector<UndoAction> undoStack_;
    std::vector<UndoAction> redoStack_;
    Selection selection_;
    std::string clipboard_;

    void ensureCursorValid();
    void clampCursor();
    void pushUndo(UndoAction::Type type, int line, int col, int count, const std::string& text = "");
    void renderLine(int row, int lineIdx, int width, bool isCurrentLine);
    void renderMinimap(int startRow, int startCol, int height, int width);
    std::string getMinimapColor(int lineIdx) const;
    void applySelection(UndoAction::Type type, int line, int col, int count, const std::string& text = "");
};
