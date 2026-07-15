#pragma once

#include "sdl2_ui.hpp"
#include <string>
#include <vector>
#include <functional>

namespace forge::fvm {

// ============================================================
// Forge IDE — Full SDL2-based development environment
//
// Features:
//   - File explorer panel (left)
//   - Code editor with syntax highlighting (center)
//   - Terminal/output panel (bottom)
//   - Toolbar with buttons (top)
//   - Tab system for multiple open files
//   - Status bar (bottom)
// ============================================================

struct IDEFile {
    std::string path;
    std::string name;
    std::string content;
    std::vector<std::string> lines;
    int cursorRow = 0;
    int cursorCol = 0;
    int scrollY = 0;
    int scrollX = 0;
    bool modified = false;
    bool operator==(const IDEFile& o) const { return path == o.path; }
};

struct IDEFileEntry {
    std::string name;
    std::string fullPath;
    bool isDir;
    int depth;
    bool expanded = false;
    std::vector<IDEFileEntry> children;
};

struct IDETheme {
    uint8_t bgR, bgG, bgB;
    uint8_t panelR, panelG, panelB;
    uint8_t fgR, fgG, fgB;
    uint8_t accentR, accentG, accentB;
    uint8_t lineNumR, lineNumG, lineNumB;
    uint8_t selR, selG, selB;
    uint8_t toolbarR, toolbarG, toolbarB;
    uint8_t statusR, statusG, statusB;
    uint8_t dirR, dirG, dirB;
    uint8_t fileR, fileG, fileB;
    uint8_t keywordR, keywordG, keywordB;
    uint8_t stringR, stringG, stringB;
    uint8_t numberR, numberG, numberB;
    uint8_t commentR, commentG, commentB;
};

class ForgeIDE {
public:
    ForgeIDE();
    ~ForgeIDE();

    bool init(int width = 1200, int height = 800);
    void run();
    void shutdown();

private:
    SDL2UI& ui_;
    bool running_ = false;

    // Panels
    static constexpr int TOOLBAR_H = 36;
    static constexpr int STATUS_H = 22;
    static constexpr int FILE_EXPLORER_W = 220;
    static constexpr int TERMINAL_H = 180;
    static constexpr int TAB_H = 28;

    // State
    std::vector<IDEFile> openFiles_;
    int activeFileIndex_ = -1;
    std::vector<IDEFileEntry> fileTree_;
    std::string currentDir_;
    std::string terminalOutput_;
    std::string terminalInput_;
    bool terminalFocused_ = false;
    bool editorFocused_ = true;
    bool explorerFocused_ = false;
    int mouseX_ = 0, mouseY_ = 0;
    bool mousePressed_ = false;
    bool mouseDown_ = false;
    int lastKey_ = 0;
    char lastChar_ = 0;
    bool keyShift_ = false, keyCtrl_ = false, keyAlt_ = false;

    // Syntax highlighting colors per character
    struct SyntaxColor { uint8_t r, g, b; };

    IDETheme theme_;

    // File explorer
    void buildFileTree(const std::string& dir, std::vector<IDEFileEntry>& entries, int depth = 0);
    void renderFileExplorer(int x, int y, int w, int h);
    void handleFileExplorerClick(int mx, int my, int x, int y, int w, int h);

    // Editor
    void renderEditor(int x, int y, int w, int h);
    void handleEditorInput(int key, char ch, bool shift, bool ctrl);
    void handleEditorClick(int mx, int my, int x, int y, int w, int h);
    std::vector<SyntaxColor> highlightLine(const std::string& line);

    // Tabs
    void renderTabs(int x, int y, int w);
    void handleTabClick(int mx, int my, int x, int y, int w);

    // Toolbar
    void renderToolbar(int x, int y, int w);
    void handleToolbarClick(int mx, int my, int x, int y, int w);

    // Terminal
    void renderTerminal(int x, int y, int w, int h);
    void handleTerminalInput(int key, char ch);
    void runCommand(const std::string& cmd);

    // Status bar
    void renderStatusBar(int x, int y, int w);

    // Helpers
    std::string readFileContent(const std::string& path);
    void writeFileContent(const std::string& path, const std::string& content);
    void openFile(const std::string& path);
    void closeFile(int index);
    int charAtPosition(const std::string& line, int px, int textX);
    static IDETheme defaultTheme();
};

// Entry point: launch the Forge IDE
int runForgeIDE(int width = 1200, int height = 800);

} // namespace forge::fvm
