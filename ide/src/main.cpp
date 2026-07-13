#include "terminal.hpp"
#include "editor.hpp"
#include "file_explorer.hpp"
#include "repl.hpp"
#include "syntax.hpp"
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <climits>
#include <signal.h>
#include <ctime>
#include <sys/wait.h>

// App state
static bool running = true;
static Editor editor;
static FileExplorer fileExplorer;
static Repl repl;
static TerminalSize termSize;

// Layout constants
static const int MENU_HEIGHT = 1;
static const int STATUS_HEIGHT = 1;
static const int EXPLORER_WIDTH = 26;
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
static void updateLayout();

static double getTimeSeconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void setStatusMessage(const std::string& msg) {
    statusMessage = msg;
    statusMessageTime = getTimeSeconds();
}

// Detect forge binary path relative to IDE binary
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
            // Try parent build dir
            forgePath = dir + "/../forge";
            if (access(forgePath.c_str(), X_OK) == 0) {
                return forgePath;
            }
        }
    }
    return "forge";
}

int main(int argc, char* argv[]) {
    auto& term = Terminal::instance();
    term.enableRawMode();
    term.enterAlternateScreen();
    term.enableMouse();

    termSize = term.getSize();

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

    output += ansi::hideCursor();

    // Clear screen
    output += ansi::move(1, 1);
    output += ansi::BG_BLACK;
    for (int i = 0; i < termSize.rows; i++) {
        output += ansi::move(i + 1, 1);
        output += ansi::clearLine();
    }

    // Menu bar
    output += ansi::move(1, 1);
    output += ansi::BG_STATUS + ansi::FG_WHITE + ansi::BOLD;
    output += " FILE  RUN  HELP ";
    output += ansi::RESET;
    // Fill rest of menu bar
    output += ansi::BG_STATUS + ansi::FG_WHITE;
    for (int i = 18; i < termSize.cols; i++) output += ' ';
    output += ansi::RESET;

    // File name in menu bar
    if (!editor.getFilePath().empty()) {
        std::string fname = editor.getFilePath();
        size_t lastSlash = fname.rfind('/');
        if (lastSlash != std::string::npos) fname = fname.substr(lastSlash + 1);
        if (editor.isModified()) fname += " *";
        output += ansi::move(1, termSize.cols - (int)fname.size() - 1);
        output += ansi::BG_STATUS + ansi::FG_BOLD_YELLOW;
        output += fname;
        output += ansi::RESET;
    }

    // Dropdown menu
    if (activeMenu != MENU_NONE) {
        output += ansi::move(2, 1);
        output += ansi::BG_BLACK + ansi::FG_WHITE;

        if (activeMenu == MENU_FILE) {
            std::vector<std::string> items = {"New  Ctrl+N", "Open  Ctrl+O", "Save  Ctrl+S", "Save As", "Close"};
            for (int i = 0; i < (int)items.size(); i++) {
                output += ansi::move(2 + i, 1);
                output += ansi::clearLine();
                if (i == menuSelection) {
                    output += ansi::BG_BLUE + ansi::FG_WHITE;
                } else {
                    output += ansi::BG_BLACK + ansi::FG_WHITE;
                }
                output += " " + items[i] + " ";
                output += ansi::RESET;
            }
        } else if (activeMenu == MENU_RUN) {
            output += ansi::move(2, 1);
            output += ansi::clearLine();
            if (menuSelection == 0) {
                output += ansi::BG_BLUE + ansi::FG_WHITE;
            } else {
                output += ansi::BG_BLACK + ansi::FG_WHITE;
            }
            output += " Run File  F9  ";
            output += ansi::RESET;
        } else if (activeMenu == MENU_HELP) {
            output += ansi::move(2, 1);
            output += ansi::clearLine();
            if (menuSelection == 0) {
                output += ansi::BG_BLUE + ansi::FG_WHITE;
            } else {
                output += ansi::BG_BLACK + ansi::FG_WHITE;
            }
            output += " About ";
            output += ansi::RESET;
        }
    }

    term.write(output);

    // File explorer
    int explorerHeight = getEditorHeight();
    fileExplorer.render(MENU_HEIGHT + 1, 1, explorerHeight, EXPLORER_WIDTH, fileExplorer.getSelectedIndex());

    // Separator line
    std::string sep;
    for (int i = 0; i < explorerHeight; i++) {
        sep += ansi::move(MENU_HEIGHT + 1 + i, EXPLORER_WIDTH + 1);
        sep += ansi::BG_PANEL + ansi::FG_GRAY + "|" + ansi::RESET;
    }
    term.write(sep);

    // Editor
    int editorHeight = getEditorHeight();
    editor.render(MENU_HEIGHT + 1, getEditorStartCol(), editorHeight, getEditorWidth());

    // REPL
    if (replVisible) {
        int replStart = getReplStartRow();
        repl.render(replStart, 1, REPL_HEIGHT_DEFAULT, termSize.cols);

        // Separator above REPL
        std::string replSep;
        replSep += ansi::move(replStart - 1, 1);
        replSep += ansi::BG_CYAN + ansi::FG_BOLD_WHITE;
        for (int i = 0; i < termSize.cols; i++) replSep += '=';
        replSep += ansi::RESET;
        term.write(replSep);
    }

    // Status bar
    int statusRow = termSize.rows;
    std::string status;
    status += ansi::move(statusRow, 1);
    status += ansi::BG_STATUS + ansi::FG_WHITE + ansi::BOLD;

    // Left side: position info
    char posBuf[128];
    snprintf(posBuf, sizeof(posBuf), " Ln %d, Col %d  |  %d lines ",
             editor.getCursorLine() + 1, editor.getCursorCol() + 1, editor.getLineCount());
    status += posBuf;

    // Status message (center)
    if (!statusMessage.empty()) {
        double elapsed = getTimeSeconds() - statusMessageTime;
        if (elapsed < 3.0) {
            int remaining = termSize.cols - (int)strlen(posBuf) - 20;
            if (remaining > (int)statusMessage.size() + 2) {
                int pad = remaining / 2;
                for (int i = 0; i < pad; i++) status += ' ';
                status += ansi::FG_BOLD_YELLOW + statusMessage + ansi::RESET + ansi::BG_STATUS + ansi::FG_WHITE + ansi::BOLD;
            }
        }
    }

    // Right side
    std::string right = "Forge Studio v1.0 ";
    int usedLen = (int)strlen(posBuf) + (int)right.size();
    int fillLen = termSize.cols - usedLen;
    if (fillLen > 0 && !statusMessage.empty()) {
        double elapsed = getTimeSeconds() - statusMessageTime;
        if (elapsed < 3.0) {
            // Already centered message takes some space
            fillLen = 0;
        }
    }
    for (int i = 0; i < fillLen; i++) status += ' ';
    status += right;

    status += ansi::RESET;
    status += ansi::clearToEnd();
    term.write(status);

    // Position cursor
    if (promptMode != PROMPT_NONE) {
        // Show prompt at bottom
        int promptRow = termSize.rows - 1;
        std::string promptLine;
        promptLine += ansi::move(promptRow, 1);
        promptLine += ansi::BG_INPUT + ansi::FG_BOLD_WHITE;
        promptLine += " " + promptTitle + ": " + promptBuffer;
        promptLine += ansi::RESET + ansi::clearToEnd();
        term.write(promptLine);
        // Move terminal cursor
        int cursorPos = (int)promptTitle.size() + 4 + promptCursor;
        term.write(ansi::move(promptRow, cursorPos + 1));
        term.write(ansi::showCursor());
    } else if (replVisible && repl.isActive()) {
        // REPL is active, show cursor there
        int replStart = getReplStartRow();
        int replInputRow = replStart + REPL_HEIGHT_DEFAULT - 1;
        int promptLen = 7; // "forge> "
        term.write(ansi::move(replInputRow, promptLen + repl.getPrompt().size() + 1 + promptCursor));
        term.write(ansi::showCursor());
    } else {
        // Editor cursor
        int cursorRow = MENU_HEIGHT + 1 + editor.getCursorLine() - editor.getScrollOffset();
        int cursorCol = getEditorStartCol() + 5 + editor.getCursorCol();
        term.write(ansi::move(cursorRow, cursorCol));
        term.write(ansi::showCursor());
    }

    term.flush();
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
            case 'a' - 'a' + 1: // Ctrl+A - go to start of line
                editor.moveHome();
                return;
            case 'e' - 'a' + 1: // Ctrl+E - go to end of line
                editor.moveEnd();
                return;
            case 'f' - 'a' + 1: // Ctrl+F - search
                promptMode = PROMPT_SEARCH;
                promptBuffer.clear();
                promptCursor = 0;
                promptTitle = "Search";
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
        if (menuSelection > 0) menuSelection--;
        return;
    }
    if (key == KEY_DOWN) {
        menuSelection++;
        if (activeMenu == MENU_FILE && menuSelection > 4) menuSelection = 4;
        if (activeMenu == MENU_RUN && menuSelection > 0) menuSelection = 0;
        if (activeMenu == MENU_HELP && menuSelection > 0) menuSelection = 0;
        return;
    }

    if (key == KEY_ENTER) {
        if (activeMenu == MENU_FILE) {
            switch (menuSelection) {
                case FILE_NEW:    newFile(); break;
                case FILE_OPEN:
                    promptMode = PROMPT_OPEN;
                    promptBuffer.clear();
                    promptCursor = 0;
                    promptTitle = "Open File";
                    break;
                case FILE_SAVE:   saveCurrentFile(); break;
                case FILE_SAVEAS:
                    promptMode = PROMPT_SAVEAS;
                    promptBuffer = editor.getFilePath();
                    promptCursor = (int)promptBuffer.size();
                    promptTitle = "Save As";
                    break;
                case FILE_CLOSE:  newFile(); break;
            }
        } else if (activeMenu == MENU_RUN) {
            runCurrentFile();
        } else if (activeMenu == MENU_HELP) {
            showAbout();
        }
        activeMenu = MENU_NONE;
        menuSelection = 0;
        return;
    }

    // Quick keys for file menu
    if (key == 'f' || key == 'F') { activeMenu = MENU_FILE; menuSelection = 0; return; }
    if (key == 'r' || key == 'R') { activeMenu = MENU_RUN; menuSelection = 0; return; }
    if (key == 'h' || key == 'H') { activeMenu = MENU_HELP; menuSelection = 0; return; }
}

static void handlePromptInput(int key) {
    switch (key) {
        case KEY_ENTER: {
            std::string result = promptBuffer;
            PromptMode mode = promptMode;
            promptMode = PROMPT_NONE;

            if (mode == PROMPT_OPEN) {
                if (!result.empty()) openFile(result);
            } else if (mode == PROMPT_SAVEAS) {
                if (!result.empty()) {
                    editor.saveFile(result);
                    setStatusMessage("Saved: " + result);
                }
            } else if (mode == PROMPT_SEARCH) {
                if (!result.empty()) {
                    int found = editor.findNext(result);
                    if (found >= 0) {
                        setStatusMessage("Found: " + result);
                    } else {
                        setStatusMessage("Not found: " + result);
                    }
                }
            } else if (mode == PROMPT_GOTO) {
                int line = std::atoi(result.c_str());
                if (line > 0) {
                    editor.moveToLine(line);
                    setStatusMessage("Line " + result);
                }
            }
            break;
        }
        case KEY_BACKSPACE: {
            if (promptCursor > 0) {
                promptBuffer.erase(promptCursor - 1, 1);
                promptCursor--;
            }
            break;
        }
        case KEY_DELETE: {
            if (promptCursor < (int)promptBuffer.size()) {
                promptBuffer.erase(promptCursor, 1);
            }
            break;
        }
        case KEY_LEFT: {
            if (promptCursor > 0) promptCursor--;
            break;
        }
        case KEY_RIGHT: {
            if (promptCursor < (int)promptBuffer.size()) promptCursor++;
            break;
        }
        case KEY_HOME: promptCursor = 0; break;
        case KEY_END:  promptCursor = (int)promptBuffer.size(); break;
        case KEY_ESCAPE: promptMode = PROMPT_NONE; break;
        default: {
            if (key >= 32 && key < 127) {
                promptBuffer.insert(promptCursor, 1, (char)key);
                promptCursor++;
            }
            break;
        }
    }
}

static void openFile(const std::string& path) {
    std::string resolvedPath = path;

    // If not absolute, resolve relative to CWD
    if (resolvedPath.empty() || resolvedPath[0] != '/') {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd))) {
            resolvedPath = std::string(cwd) + "/" + path;
        }
    }

    editor.loadFile(resolvedPath);
    setStatusMessage("Opened: " + path);
    fileExplorer.refresh();
}

static void saveCurrentFile() {
    if (editor.getFilePath().empty()) {
        promptMode = PROMPT_SAVEAS;
        promptBuffer.clear();
        promptCursor = 0;
        promptTitle = "Save As";
    } else {
        editor.saveFile(editor.getFilePath());
        setStatusMessage("Saved: " + editor.getFilePath());
    }
}

static void newFile() {
    editor.newFile();
    setStatusMessage("New file");
}

static void showAbout() {
    setStatusMessage("Forge Studio v1.0 - A terminal IDE for Forge");
}

static void runCurrentFile() {
    if (editor.getFilePath().empty()) {
        setStatusMessage("No file to run. Save first.");
        return;
    }

    // Save first
    editor.saveFile(editor.getFilePath());

    std::string forgePath = findForgeBinary();
    std::string cmd = forgePath + " " + editor.getFilePath();

    // Open REPL to show output
    replVisible = true;
    repl.activate();
    repl.clear();
    repl.runCommand("Running: " + editor.getFilePath());
    repl.runCommand(cmd);
}
