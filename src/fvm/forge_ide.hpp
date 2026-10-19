#pragma once

#include "sdl2_ui.hpp"
#include <string>
#include <vector>
#include <functional>

namespace forge::fvm {

// ============================================================
// Forge IDE — Full SDL2-based development environment
//
// VS Code-familiar layout:
//   +------+--------+--------------------------+---------+
//   | Act. | Title  / toolbar                  |         |
//   | Bar  |--------+--------------------------|         |
//   | icons| Side   | Tabs [name ×] [name ●]   |         |
//   |      | bar    |--------------------------|         |
//   |      | (tree/ | Editor: gutter, active   | Minimap |
//   |      | filter)| line hl, bracket match,  |         |
//   |      |        | syntax highlighting      |         |
//   |------+----------------------------------+---------+
//   | Terminal [TERMINAL|OUTPUT]  (wheel scroll)         |
//   |----------------------------------------------------|
//   | Status bar (blue): Ln, Col · spaces · Forge · UTF-8|
//   +----------------------------------------------------+
//
// Keyboard: Ctrl+P command palette · Ctrl+S save · Ctrl+W close tab
//           Ctrl+N new file · F5 run · Ctrl+C/X/V clipboard
// ============================================================

struct IDEFile {
    std::string path;
    std::string name;
    std::string content;
    std::vector<std::string> lines;
    int cursorRow = 0;
    int cursorCol = 0;
    int scrollY = 0;
    int desiredCol = -1;      // sticky column for vertical navigation
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

// Palette entry for quick-open
struct PaletteItem {
    std::string path;       // full path
    std::string label;      // file name
    std::string detail;     // parent dir
    int score = 0;
};

struct IDETheme {
    // Core surfaces
    uint8_t bgR, bgG, bgB;              // editor background
    uint8_t panelR, panelG, panelB;     // side panels
    uint8_t activityR, activityG, activityB;  // activity bar
    uint8_t titleR, titleG, titleB;     // title/toolbar
    uint8_t statusR, statusG, statusB;  // status bar
    uint8_t statusFgR, statusFgG, statusFgB;

    // Text & accents
    uint8_t fgR, fgG, fgB;
    uint8_t accentR, accentG, accentB;  // focus accent (borders, cursor)
    uint8_t lineNumR, lineNumG, lineNumB;
    uint8_t lineNumActiveR, lineNumActiveG, lineNumActiveB;
    uint8_t selR, selG, selB;           // selection / hover fill
    uint8_t curLineR, curLineG, curLineB, curLineA;  // current-line band

    // Tabs
    uint8_t tabBarR, tabBarG, tabBarB;  // tab strip background
    uint8_t tabActiveR, tabActiveG, tabActiveB;
    uint8_t tabInactiveR, tabInactiveG, tabInactiveB;

    // Tree colors
    uint8_t dirR, dirG, dirB;
    uint8_t fileR, fileG, fileB;

    // Syntax
    uint8_t keywordR, keywordG, keywordB;    // declarations
    uint8_t controlR, controlG, controlB;    // if/else/return...
    uint8_t typeR, typeG, typeB;             // class/struct types
    uint8_t funcR, funcG, funcB;             // call expressions
    uint8_t stringR, stringG, stringB;
    uint8_t numberR, numberG, numberB;
    uint8_t commentR, commentG, commentB;

    // Extras
    uint8_t minimapR, minimapG, minimapB;    // minimap code bars
    uint8_t borderR, borderG, borderB;       // subtle separators
};

class ForgeIDE {
public:
    ForgeIDE();
    ~ForgeIDE();

    bool init(int width = 1280, int height = 840);
    void run();
    void shutdown();

private:
    SDL2UI& ui_;
    bool running_ = false;

    // Layout metrics
    static constexpr int ACTIVITY_W = 44;
    static constexpr int TOOLBAR_H = 34;
    static constexpr int STATUS_H = 24;
    static constexpr int SIDEBAR_W = 230;
    static constexpr int TERMINAL_H = 190;
    static constexpr int TAB_H = 30;
    static constexpr int MINIMAP_W = 64;
    static constexpr int LINE_H = 16;
    static constexpr int GUTTER_W = 56;
    static constexpr int CH_W = 8;
    static constexpr int CH_H = 8;

    enum class SideView { Explorer, Filter, Settings };
    enum class TermTab { Terminal, Output };

    // State
    std::vector<IDEFile> openFiles_;
    int activeFileIndex_ = -1;
    std::vector<IDEFileEntry> fileTree_;
    std::string currentDir_;
    std::vector<std::string> allFiles_;     // recursive list for palette

    std::string terminalOutput_;
    std::vector<std::string> outputLines_;  // split cache for scrolling
    int terminalScroll_ = 0;                // lines scrolled up from bottom
    std::string terminalInput_;
    bool terminalFocused_ = false;
    bool editorFocused_ = true;
    bool sidebarFocused_ = false;
    SideView sideView_ = SideView::Explorer;
    TermTab termTab_ = TermTab::Terminal;

    // Explorer
    int explorerScroll_ = 0;
    std::string filterInput_;
    bool filterFocused_ = false;

    // Command palette
    bool paletteOpen_ = false;
    std::string paletteInput_;
    std::vector<PaletteItem> paletteResults_;
    int paletteSelected_ = 0;

    // Settings
    int selectedTheme_ = 0;

    // Input state
    int mouseX_ = 0, mouseY_ = 0;
    bool mousePressed_ = false;
    int lastKey_ = 0;
    char lastChar_ = 0;
    bool keyShift_ = false, keyCtrl_ = false, keyAlt_ = false;
    uint32_t blinkTick_ = 0;

    IDETheme theme_;

    struct SyntaxColor { uint8_t r, g, b; };

    // Immediate-mode hit-test geometry (rebuilt each frame)
    std::vector<std::pair<int,int>> tabGeom_;               // x, width per rendered tab
    std::vector<std::pair<int,int>> termTabGeom_;           // terminal panel tabs
    struct RowHit { int y; std::string path; bool isDir; };
    std::vector<RowHit> entryRowMap_;                       // explorer rows

    // Theme management
    static IDETheme themeVscodeDark();
    static IDETheme themeForgeNight();
    static IDETheme themeLight();
    void applyTheme(int index);

    // Project scan / file tree
    void buildFileTree(const std::string& dir, std::vector<IDEFileEntry>& entries, int depth = 0);
    void scanProjectFiles(const std::string& dir);

    // Main regions
    void renderActivityBar(int x, int y, int w, int h);
    void handleActivityClick(int mx, int my);
    void renderToolbar(int x, int y, int w);
    void renderTabs(int x, int y, int w);
    void handleTabClick(int mx, int my, int x, int y, int w);
    void renderSidebar(int x, int y, int w, int h);
    void handleSidebarClick(int mx, int my, int x, int y, int w, int h);
    void renderEditor(int x, int y, int w, int h);
    void renderMinimap(int x, int y, int w, int h, IDEFile& file);
    void handleEditorClick(int mx, int my, int x, int y, int w, int h);
    void renderTerminal(int x, int y, int w, int h);
    void handleTerminalClick(int mx, int my, int x, int y);
    void handleTerminalInput(int key, char ch);
    void renderStatusBar(int x, int y, int w);
    void renderWelcome(int x, int y, int w, int h);
    void renderPalette();
    void updatePalette();
    void handlePaletteKey(int key, char ch);

    // Editor behavior
    void handleEditorInput(int key, char ch, bool shift, bool ctrl);
    std::vector<SyntaxColor> highlightLine(const std::string& line);
    uint8_t typeBlueGuard();
    void ensureCursorVisible();
    bool findMatchingBracket(int& outRow, int& outCol);
    void scrollToCursor();

    // Helpers
    std::string readFileContent(const std::string& path);
    void writeFileContent(const std::string& path, const std::string& content);
    void openFile(const std::string& path);
    void closeFile(int index);
    void saveActiveFile();
    void syncContent(IDEFile& f);
    void appendOutput(const std::string& text);
    void runCommand(const std::string& cmd);
    static bool subsequenceMatch(const std::string& pattern, const std::string& text, int* score = nullptr);
    static IDETheme defaultTheme() { return themeVscodeDark(); }
};

// Entry point: launch the Forge IDE
int runForgeIDE(int width = 1280, int height = 840);

} // namespace forge::fvm
