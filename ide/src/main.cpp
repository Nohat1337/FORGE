#include "terminal.hpp"
#include "editor.hpp"
#include "file_explorer.hpp"
#include "repl.hpp"
#include "syntax.hpp"
#include "theme.hpp"
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <climits>
#include <signal.h>
#include <ctime>
#include <sys/wait.h>
#include <vector>
#include <algorithm>

// App state
static bool running = true;
static Editor editor;
static FileExplorer fileExplorer;
static Repl repl;
static TerminalSize termSize;

// Layout constants
static const int MENU_HEIGHT = 1;
static const int STATUS_HEIGHT = 1;
static const int EXPLORER_WIDTH = 28;
static const int REPL_HEIGHT_DEFAULT = 12;

// Menu bar
enum MenuItem { MENU_NONE, MENU_FILE, MENU_RUN, MENU_HELP };
enum FileAction { FILE_NEW, FILE_OPEN, FILE_SAVE, FILE_SAVEAS, FILE_CLOSE };
enum RunAction { RUN_FILE };
enum HelpAction { HELP_ABOUT };

static MenuItem activeMenu = MENU_NONE;
static int menuSelection = 0;

// Prompt mode
enum PromptMode { PROMPT_NONE, PROMPT_OPEN, PROMPT_SAVEAS, PROMPT_GOTO, PROMPT_SEARCH };
static PromptMode promptMode = PROMPT_NONE;
static std::string promptBuffer;
static int promptCursor = 0;
static std::string promptTitle;

// Status message
static std::string statusMessage;
static double statusMessageTime = 0;

// REPL panel state
static bool replVisible = false;
static bool minimapVisible = false;

// Theme
static Theme::Type currentTheme = Theme::Type::DARK;

// Forward declarations
static void render();
static void handleInput(int key);
static void handleMenuInput(int key);
static void handlePromptInput(int key);
static void openFile(const std::string& path);
static void saveCurrentFile();
static void newFile();
static void showAbout();
static void runCurrentFile();
static void setStatusMessage(const std::string& msg);
static std::string findForgeBinary();
static double getTimeSeconds();

int main(int argc, char* argv[]) {
    auto& term = Terminal::instance();
    term.enableRawMode();
    term.enterAlternateScreen();
    term.enableMouse();

    termSize = term.getSize();

    // Apply default theme
    Theme::set(currentTheme);
    editor.setTheme(currentTheme);

    fileExplorer.refresh();

    std::string forgePath = findForgeBinary();
    repl.setForgePath(forgePath);

    if (argc > 1) {
        editor.loadFile(argv[1]);
    }

    // Main loop
    while (running) {
        if (Terminal::resizeFlag) {
            Terminal::resizeFlag = 0;
            termSize = term.getSize();
        }

        render();

        int key = term.readKey();
        if (key != KEY_NONE) {
            handleInput(key);
        }
    }

    term.disableMouse();
    term.exitAlternateScreen();
    term.disableRawMode();

    return 0;
}

static int getEditorHeight() {
    int replH = replVisible ? REPL_HEIGHT_DEFAULT : 0;
    return termSize.rows - MENU_HEIGHT - STATUS_HEIGHT - replH;
}

static int getEditorWidth() {
    return termSize.cols - EXPLORER_WIDTH;
}

static int getEditorStartCol() {
    return EXPLORER_WIDTH + 1;
}

static int getReplStartRow() {
    int replH = replVisible ? REPL_HEIGHT_DEFAULT : 0;
    if (replH == 0) return 0;
    return termSize.rows - STATUS_HEIGHT - replH;
}

static void render() {
    auto& term = Terminal::instance();
    std::string output;
    output.reserve(termSize.rows * termSize.cols * 4);

    const Theme::Colors& theme = Theme::getCurrent();

    output += ansi::hideCursor();

    // Clear screen with theme background
    output += ansi::move(1, 1);
    output += theme.bg_editor;
    for (int i = 0; i < termSize.rows; i++) {
        output += ansi::move(i + 1, 1);
        output += ansi::clearLine();
    }

    // Layout calculations
    int replH = replVisible ? REPL_HEIGHT_DEFAULT : 0;
    int explorerHeight = termSize.rows - MENU_HEIGHT - STATUS_HEIGHT - replH;
    int editorHeight = explorerHeight;
    int editorStartRow = MENU_HEIGHT + 1;
    int editorStartCol = EXPLORER_WIDTH + 1;
    int editorWidth = termSize.cols - EXPLORER_WIDTH;
    int minimapW = minimapVisible ? 18 : 0;
    int textWidth = editorWidth - 7 - minimapW - 1; // 7 = gutter (5 + 1 + 1)

    // ========== MENU BAR ==========
    output += ansi::move(1, 1);
    output += theme.bg_menu + theme.fg_editor + ansi::BOLD;
    output += "  FILE  RUN  HELP  ";
    output += ansi::RESET;
    // Fill rest of menu bar
    output += theme.bg_menu;
    for (int i = 17; i < termSize.cols; i++) output += ' ';
    output += ansi::RESET;

    // Active menu highlight
    if (activeMenu != MENU_NONE) {
        int col = 0;
        std::vector<std::string> menus = {"FILE", "RUN", "HELP"};
        for (size_t i = 0; i < menus.size(); i++) {
            col += 2;
            if ((activeMenu == MENU_FILE && i == 0) ||
                (activeMenu == MENU_RUN && i == 1) ||
                (activeMenu == MENU_HELP && i == 2)) {
                output += ansi::move(1, col);
                output += theme.accent1 + ansi::BOLD + " " + menus[i] + " " + ansi::RESET + theme.bg_menu;
            }
            col += (int)menus[i].size() + 2;
        }
    }

    // Theme indicator on menu bar right
    std::string themeName = 
        currentTheme == Theme::Type::DARK ? "Dark" :
        currentTheme == Theme::Type::DRACULA ? "Dracula" :
        currentTheme == Theme::Type::NORD ? "Nord" :
        currentTheme == Theme::Type::ONEDARK ? "OneDark" :
        currentTheme == Theme::Type::MONOKAI ? "Monokai" :
        currentTheme == Theme::Type::GRUVBOX_DARK ? "Gruvbox" :
        currentTheme == Theme::Type::SOLARIZED_DARK ? "Solarized" : "Light";
    
    output += ansi::move(1, termSize.cols - 20);
    output += theme.fg_comment + "Theme: " + themeName + ansi::RESET;

    // Dropdown menus
    if (activeMenu != MENU_NONE) {
        output += ansi::move(2, 1);
        output += theme.bg_panel + theme.fg_editor;

        if (activeMenu == MENU_FILE) {
            std::vector<std::string> items = {"New  Ctrl+N", "Open  Ctrl+O", "Save  Ctrl+S", "Save As", "Close"};
            for (int i = 0; i < (int)items.size(); i++) {
                output += ansi::move(2 + i, 1);
                output += ansi::clearLine();
                if (i == menuSelection) {
                    output += theme.accent1 + ansi::BOLD;
                } else {
                    output += theme.fg_editor;
                }
                output += " " + items[i] + " ";
                output += ansi::RESET;
            }
        } else if (activeMenu == MENU_RUN) {
            output += ansi::move(2, 1);
            output += ansi::clearLine();
            if (menuSelection == 0) {
                output += theme.accent1 + ansi::BOLD;
            } else {
                output += theme.fg_editor;
            }
            output += " Run File  F9  ";
            output += ansi::RESET;
        } else if (activeMenu == MENU_HELP) {
            output += ansi::move(2, 1);
            output += ansi::clearLine();
            if (menuSelection == 0) {
                output += theme.accent1 + ansi::BOLD;
            } else {
                output += theme.fg_editor;
            }
            output += " About ";
            output += ansi::RESET;
        }
    }

    // ========== FILE EXPLORER ==========
    int explorerStartRow = MENU_HEIGHT + 1;
    fileExplorer.render(explorerStartRow, 1, explorerHeight, EXPLORER_WIDTH, fileExplorer.getSelectedIndex());

    // Separator line
    std::string sep;
    for (int i = 0; i < explorerHeight; i++) {
        sep += ansi::move(MENU_HEIGHT + 1 + i, EXPLORER_WIDTH + 1);
        sep += theme.bg_panel + theme.fg_comment + "│" + ansi::RESET;
    }
    output += sep;

    // ========== EDITOR ==========
    editor.render(MENU_HEIGHT + 1, getEditorStartCol(), editorHeight, editorWidth, minimapVisible);

    // ========== REPL ==========
    if (replVisible) {
        int replStartRow = editorStartRow + editorHeight;
        repl.render(replStartRow, 1, replH, termSize.cols);

        // Separator above REPL
        std::string replSep;
        replSep += ansi::move(replStartRow - 1, 1);
        replSep += theme.accent3 + ansi::BOLD;
        for (int i = 0; i < termSize.cols; i++) replSep += '=';
        replSep += ansi::RESET;
        output += replSep;
    }

    // ========== STATUS BAR ==========
    int statusRow = termSize.rows;
    output += ansi::move(statusRow, 1);
    output += theme.bg_status + theme.fg_editor + ansi::BOLD;

    // Left side: modified indicator, filename, language, position
    std::string mode = editor.isModified() ? "● " : "  ";
    std::string filename = editor.getFileName();
    std::string lang = editor.getLanguage();
    char posBuf[128];
    snprintf(posBuf, sizeof(posBuf), "Ln %d, Col %d", editor.getCursorLine() + 1, editor.getCursorCol() + 1);
    std::string total = std::to_string(editor.getTotalLines()) + " lines";

    std::string left = " " + mode + filename + " | " + lang + " | " + posBuf + " | " + total + " ";
    output += left;

    // Status message (center)
    if (!statusMessage.empty()) {
        double elapsed = getTimeSeconds() - statusMessageTime;
        if (elapsed < 3.0) {
            int remaining = termSize.cols - (int)left.size() - 20;
            if (remaining > (int)statusMessage.size() + 2) {
                int pad = remaining / 2;
                for (int i = 0; i < pad; i++) output += ' ';
                output += theme.accent1 + statusMessage + ansi::RESET + theme.bg_status + theme.fg_editor + ansi::BOLD;
            }
        }
    }

    // Right side - time and theme
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    char timeBuf[16];
    strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", tm_info);
    output += ansi::move(statusRow, termSize.cols - 12);
    output += theme.fg_comment + std::string(timeBuf) + ansi::RESET;

    // ========== PROMPT OVERLAY ==========
    if (promptMode != PROMPT_NONE) {
        int promptRow = termSize.rows / 2;
        int boxWidth = (int)promptTitle.size() + 40;
        int boxHeight = 5;
        int boxRow = promptRow - 2;
        int boxCol = (termSize.cols - boxWidth) / 2;
        
        // Background box
        for (int i = 0; i < boxHeight; i++) {
            output += ansi::move(boxRow + i, boxCol);
            output += theme.bg_panel;
            output += ansi::clearLine();
            for (int j = 0; j < boxWidth; j++) output += ' ';
        }
        
        // Title
        output += ansi::move(boxRow, boxCol + 2);
        output += theme.accent1 + ansi::BOLD + promptTitle + ansi::RESET;
        
        // Input field
        output += ansi::move(boxRow + 2, boxCol + 2);
        output += theme.bg_panel + theme.fg_editor + " " + promptBuffer + "_" + std::string(boxWidth - 4 - promptBuffer.size(), ' ') + " " + ansi::RESET;
        
        // Hint
        output += ansi::move(boxRow + 3, boxCol + 2);
        output += theme.fg_comment + "Enter=confirm  Esc=cancel" + ansi::RESET;
    }

    term.write(output);

    // ========== CURSOR POSITIONING ==========
    if (promptMode != PROMPT_NONE) {
        int promptRow = termSize.rows / 2;
        int boxWidth = (int)promptTitle.size() + 40;
        int boxCol = (termSize.cols - boxWidth) / 2;
        int cursorPos = boxCol + 4 + promptCursor;
        term.write(ansi::move(promptRow + 2, cursorPos));
        term.write(ansi::showCursor());
    } else if (replVisible && repl.isActive()) {
        int replStart = getReplStartRow();
        int replInputRow = replStart + REPL_HEIGHT_DEFAULT - 1;
        int promptLen = 7; // "forge> "
        term.write(ansi::move(replInputRow, promptLen + repl.getPrompt().size() + 1 + promptCursor));
        term.write(ansi::showCursor());
    } else {
        // Editor cursor
        int cursorRow = MENU_HEIGHT + 1 + editor.getCursorLine() - editor.getScrollOffset();
        int cursorCol = getEditorStartCol() + 7 + editor.getCursorCol() - editor.getScrollOffset();
        if (cursorCol < getEditorStartCol() + 7) cursorCol = getEditorStartCol() + 7;
        term.write(ansi::move(cursorRow, cursorCol));
        term.write(ansi::showCursor());
    }

    term.flush();
}

static void setStatusMessage(const std::string& msg) {
    statusMessage = msg;
    statusMessageTime = getTimeSeconds();
}

static double getTimeSeconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void handleInput(int key) {
    // Prompt mode
    if (promptMode != PROMPT_NONE) {
        handlePromptInput(key);
        return;
    }

    // Menu mode
    if (activeMenu != MENU_NONE) {
        handleMenuInput(key);
        return;
    }

    // Global keybindings
    bool ctrl = false;
    int actualKey = key;

    // Check for Ctrl combinations (key < 32, not tab/enter/escape)
    if (key < 32 && key != KEY_ENTER && key != KEY_TAB && key != KEY_ESCAPE) {
        ctrl = true;
        actualKey = key;
    }

    if (ctrl) {
        switch (actualKey) {
            case 's' - 'a' + 1: // Ctrl+S
                saveCurrentFile();
                return;
            case 'n' - 'a' + 1: // Ctrl+N
                newFile();
                return;
            case 'o' - 'a' + 1: // Ctrl+O
                promptMode = PROMPT_OPEN;
                promptBuffer.clear();
                promptCursor = 0;
                promptTitle = "Open File";
                return;
            case 'q' - 'a' + 1: // Ctrl+Q
                running = false;
                return;
            case 'z' - 'a' + 1: // Ctrl+Z
                editor.undo();
                return;
            case 'y' - 'a' + 1: // Ctrl+Y
                editor.redo();
                return;
            case 'a' - 'a' + 1: // Ctrl+A
                editor.moveHome();
                return;
            case 'e' - 'a' + 1: // Ctrl+E
                editor.moveEnd();
                return;
            case 'f' - 'a' + 1: // Ctrl+F
                promptMode = PROMPT_SEARCH;
                promptBuffer.clear();
                promptCursor = 0;
                promptTitle = "Search";
                return;
            case 'g' - 'a' + 1: // Ctrl+G - goto line
                promptMode = PROMPT_GOTO;
                promptBuffer.clear();
                promptCursor = 0;
                promptTitle = "Go to Line";
                return;
            case 't' - 'a' + 1: // Ctrl+T - toggle theme
                {
                    static std::vector<Theme::Type> themes = {
                        Theme::Type::DARK, Theme::Type::DRACULA, Theme::Type::NORD,
                        Theme::Type::ONEDARK, Theme::Type::MONOKAI, Theme::Type::GRUVBOX_DARK,
                        Theme::Type::SOLARIZED_DARK
                    };
                    auto it = std::find(themes.begin(), themes.end(), currentTheme);
                    if (it != themes.end() && ++it != themes.end()) {
                        currentTheme = *it;
                    } else {
                        currentTheme = themes[0];
                    }
                    Theme::set(currentTheme);
                    editor.setTheme(currentTheme);
                    setStatusMessage("Theme: " + std::string(
                        currentTheme == Theme::Type::DARK ? "Dark" :
                        currentTheme == Theme::Type::DRACULA ? "Dracula" :
                        currentTheme == Theme::Type::NORD ? "Nord" :
                        currentTheme == Theme::Type::ONEDARK ? "OneDark" :
                        currentTheme == Theme::Type::MONOKAI ? "Monokai" :
                        currentTheme == Theme::Type::GRUVBOX_DARK ? "Gruvbox" : "Solarized"));
                }
                return;
            case 'm' - 'a' + 1: // Ctrl+M - toggle minimap
                minimapVisible = !minimapVisible;
                setStatusMessage(minimapVisible ? "Minimap: ON" : "Minimap: OFF");
                return;
            case 'r' - 'a' + 1: // Ctrl+R - toggle REPL
                replVisible = !replVisible;
                if (replVisible) repl.activate();
                setStatusMessage(replVisible ? "REPL opened (F5 to toggle)" : "REPL closed");
                return;
        }
    }

    // Function keys
    if (key == KEY_F5) {
        replVisible = !replVisible;
        if (replVisible) repl.activate();
        setStatusMessage(replVisible ? "REPL opened" : "REPL closed");
        return;
    }
    if (key == KEY_F9) {
        runCurrentFile();
        return;
    }
    if (key == KEY_F10) {
        // Quick theme cycle
        static std::vector<Theme::Type> themes = {
            Theme::Type::DARK, Theme::Type::DRACULA, Theme::Type::NORD,
            Theme::Type::ONEDARK, Theme::Type::MONOKAI, Theme::Type::GRUVBOX_DARK,
            Theme::Type::SOLARIZED_DARK
        };
        auto it = std::find(themes.begin(), themes.end(), currentTheme);
        if (it != themes.end() && ++it != themes.end()) {
            currentTheme = *it;
        } else {
            currentTheme = themes[0];
        }
        Theme::set(currentTheme);
        editor.setTheme(currentTheme);
        return;
    }
    if (key == KEY_F11) {
        minimapVisible = !minimapVisible;
        return;
    }

    // If REPL is active and visible, route input there
    if (replVisible && repl.isActive()) {
        if (key == KEY_ESCAPE) {
            repl.deactivate();
            return;
        }
        repl.handleKey(key);
        return;
    }

    // Otherwise, editor gets the input
    switch (key) {
        case KEY_UP:    editor.moveUp(); break;
        case KEY_DOWN:  editor.moveDown(); break;
        case KEY_LEFT:  editor.moveLeft(); break;
        case KEY_RIGHT: editor.moveRight(); break;
        case KEY_HOME:  editor.moveHome(); break;
        case KEY_END:   editor.moveEnd(); break;
        case KEY_PAGE_UP:   editor.pageUp(getEditorHeight()); break;
        case KEY_PAGE_DOWN: editor.pageDown(getEditorHeight()); break;
        case KEY_DELETE:    editor.deleteChar(); break;
        case KEY_BACKSPACE: editor.backspace(); break;
        case KEY_ENTER:     editor.insertNewline(); break;
        case KEY_TAB:       editor.insertTab(); break;
        case KEY_ESCAPE:    activeMenu = MENU_NONE; break;
        case ' ' ... '~':   editor.insertChar((char)key); break;
        default: break;
    }
}

static void handleMenuInput(int key) {
    if (key == KEY_ESCAPE || key == KEY_LEFT || key == KEY_RIGHT) {
        if (key == KEY_LEFT) {
            if (activeMenu == MENU_RUN) activeMenu = MENU_FILE;
            else if (activeMenu == MENU_HELP) activeMenu = MENU_RUN;
        } else if (key == KEY_RIGHT) {
            if (activeMenu == MENU_FILE) activeMenu = MENU_RUN;
            else if (activeMenu == MENU_RUN) activeMenu = MENU_HELP;
        } else {
            activeMenu = MENU_NONE;
        }
        menuSelection = 0;
        return;
    }
    if (key == KEY_UP) {
        menuSelection = (menuSelection - 1 + 5) % 5;
        return;
    }
    if (key == KEY_DOWN) {
        menuSelection = (menuSelection + 1) % 5;
        return;
    }
    if (key == KEY_ENTER) {
        if (activeMenu == MENU_FILE) {
            switch (menuSelection) {
                case 0: newFile(); break;
                case 1: 
                    promptMode = PROMPT_OPEN;
                    promptBuffer.clear();
                    promptCursor = 0;
                    promptTitle = "Open File";
                    break;
                case 2: saveCurrentFile(); break;
                case 3:
                    promptMode = PROMPT_SAVEAS;
                    promptBuffer = editor.getFilePath();
                    promptCursor = promptBuffer.size();
                    promptTitle = "Save As";
                    break;
                case 4: newFile(); break;
            }
        } else if (activeMenu == MENU_RUN && menuSelection == 0) {
            runCurrentFile();
        } else if (activeMenu == MENU_HELP) {
            showAbout();
        }
        activeMenu = MENU_NONE;
        menuSelection = 0;
    }
}

static void handlePromptInput(int key) {
    if (key == KEY_ESCAPE) {
        promptMode = PROMPT_NONE;
        promptBuffer.clear();
        return;
    }
    if (key == KEY_ENTER) {
        std::string result = promptBuffer;
        if (promptMode == PROMPT_OPEN) {
            openFile(result);
        } else if (promptMode == PROMPT_SAVEAS) {
            editor.saveFile(result);
            editor.setFilePath(result);
            editor.setModified(false);
            setStatusMessage("Saved to " + result);
        } else if (promptMode == PROMPT_GOTO) {
            int line = std::stoi(result);
            editor.moveToLine(line);
        } else if (promptMode == PROMPT_SEARCH) {
            editor.findNext(result);
        }
        promptMode = PROMPT_NONE;
        promptBuffer.clear();
        return;
    }
    if (key == KEY_BACKSPACE) {
        if (promptCursor > 0) {
            promptBuffer.erase(promptCursor - 1, 1);
            promptCursor--;
        }
        return;
    }
    if (key == KEY_LEFT) {
        if (promptCursor > 0) promptCursor--;
        return;
    }
    if (key == KEY_RIGHT) {
        if (promptCursor < (int)promptBuffer.size()) promptCursor++;
        return;
    }
    if (key >= 32 && key <= 126) {
        promptBuffer.insert(promptCursor, 1, (char)key);
        promptCursor++;
        return;
    }
}

static void openFile(const std::string& path) {
    editor.loadFile(path);
    setStatusMessage("Opened: " + path);
}

static void saveCurrentFile() {
    if (editor.getFilePath().empty()) {
        promptMode = PROMPT_SAVEAS;
        promptBuffer.clear();
        promptCursor = 0;
        promptTitle = "Save As";
        return;
    }
    editor.saveFile(editor.getFilePath());
    setStatusMessage("Saved: " + editor.getFilePath());
}

static void newFile() {
    editor.newFile();
    setStatusMessage("New file");
}

static void showAbout() {
    promptMode = PROMPT_NONE;
    promptTitle = "About";
    promptBuffer = "Forge Studio v1.0\nForge Programming Language IDE\nBuilt with C++20";
    promptCursor = 0;
}

static void runCurrentFile() {
    if (editor.getFilePath().empty()) {
        saveCurrentFile();
        if (editor.getFilePath().empty()) return;
    }
    if (editor.isModified()) {
        editor.saveFile(editor.getFilePath());
    }
    std::string cmd = "./forge " + editor.getFilePath();
    if (replVisible) {
        repl.runCommand(cmd);
    } else {
        // Show output in a temporary REPL
        replVisible = true;
        repl.activate();
        repl.runCommand(cmd);
        setStatusMessage("Running: " + editor.getFilePath());
    }
}

static std::string findForgeBinary() {
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len != -1) {
        exePath[len] = '\0';
        std::string dir = exePath;
        size_t lastSlash = dir.rfind('/');
        if (lastSlash != std::string::npos) {
            dir = dir.substr(0, lastSlash);
            std::string forgePath = dir + "/forge";
            if (access(forgePath.c_str(), X_OK) == 0) {
                return forgePath;
            }
            forgePath = dir + "/../forge";
            if (access(forgePath.c_str(), X_OK) == 0) {
                return forgePath;
            }
        }
    }
    return "forge";
}