#include "forge_ide.hpp"
#include <SDL2/SDL.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>
#include <filesystem>
#include <iostream>

namespace forge::fvm {

namespace fs = std::filesystem;

// ============================================================
// Theme
// ============================================================

IDETheme ForgeIDE::defaultTheme() {
    IDETheme t;
    t.bgR = 30; t.bgG = 30; t.bgB = 46;
    t.panelR = 24; t.panelG = 24; t.panelB = 37;
    t.fgR = 205; t.fgG = 214; t.fgB = 244;
    t.accentR = 137; t.accentG = 180; t.accentB = 250;
    t.lineNumR = 88; t.lineNumG = 91; t.lineNumB = 112;
    t.selR = 69; t.selG = 71; t.selB = 90;
    t.toolbarR = 17; t.toolbarG = 17; t.toolbarB = 27;
    t.statusR = 17; t.statusG = 17; t.statusB = 27;
    t.dirR = 250; t.dirG = 179; t.dirB = 135;
    t.fileR = 166; t.fileG = 227; t.fileB = 161;
    t.keywordR = 137; t.keywordG = 180; t.keywordB = 250;
    t.stringR = 166; t.stringG = 227; t.stringB = 161;
    t.numberR = 250; t.numberG = 179; t.numberB = 135;
    t.commentR = 88; t.commentG = 91; t.commentB = 112;
    return t;
}

// ============================================================
// Lifecycle
// ============================================================

ForgeIDE::ForgeIDE() : ui_(SDL2UI::instance()), theme_(defaultTheme()) {}

ForgeIDE::~ForgeIDE() { shutdown(); }

bool ForgeIDE::init(int width, int height) {
    if (!ui_.init(width, height, "Forge IDE")) {
        std::cerr << "Failed to initialize SDL2 for IDE\n";
        return false;
    }
    running_ = true;
    currentDir_ = fs::current_path().string();
    buildFileTree(currentDir_, fileTree_);
    return true;
}

void ForgeIDE::shutdown() {
    ui_.shutdown();
    running_ = false;
}

// ============================================================
// File tree
// ============================================================

void ForgeIDE::buildFileTree(const std::string& dir, std::vector<IDEFileEntry>& entries, int depth) {
    entries.clear();
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    struct dirent* ent;
    std::vector<std::string> dirs, files;
    while ((ent = readdir(d)) != nullptr) {
        std::string name = ent->d_name;
        if (name == "." || name == "..") continue;
        std::string full = dir + "/" + name;
        struct stat st;
        if (stat(full.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (name[0] == '.' || name == "build" || name == "build-debug" ||
                name == "build-release" || name == "build-sdl" || name == ".git") continue;
            dirs.push_back(name);
        } else {
            files.push_back(name);
        }
    }
    closedir(d);
    std::sort(dirs.begin(), dirs.end());
    std::sort(files.begin(), files.end());
    for (auto& dn : dirs) {
        IDEFileEntry e;
        e.name = dn;
        e.fullPath = dir + "/" + dn;
        e.isDir = true;
        e.depth = depth;
        entries.push_back(e);
    }
    for (auto& fn : files) {
        IDEFileEntry e;
        e.name = fn;
        e.fullPath = dir + "/" + fn;
        e.isDir = false;
        e.depth = depth;
        entries.push_back(e);
    }
}

// ============================================================
// Main run loop
// ============================================================

void ForgeIDE::run() {
    while (running_) {
        // Poll events
        SDLKeyEvent key;
        SDLMouseEvent mouse;
        SDLWindowEvent win;
        if (!ui_.pumpEvents(&key, &mouse, &win)) {
            running_ = false;
            break;
        }
        if (win.closed) { running_ = false; break; }
        if (win.resized) { /* TODO: re-layout */ }

        mouseX_ = mouse.x;
        mouseY_ = mouse.y;
        mousePressed_ = mouse.pressed;
        mouseDown_ = mouse.pressed || mouse.released ? false : (mouseX_ > 0 || mouseY_ > 0);

        if (key.keycode != 0) {
            lastKey_ = key.keycode;
            lastChar_ = key.ch;
            keyShift_ = key.shift;
            keyCtrl_ = key.ctrl;
            keyAlt_ = key.alt;

            if (editorFocused_ && activeFileIndex_ >= 0) {
                handleEditorInput(key.keycode, key.ch, key.shift, key.ctrl);
            } else if (terminalFocused_) {
                handleTerminalInput(key.keycode, key.ch);
            }
        }

        // Layout
        int ww, wh;
        ui_.getSize(ww, wh);

        // Render
        ui_.clear(theme_.bgR, theme_.bgG, theme_.bgB);

        // Toolbar
        renderToolbar(0, 0, ww);

        // Tabs
        renderTabs(FILE_EXPLORER_W, TOOLBAR_H, ww - FILE_EXPLORER_W);

        // File explorer
        renderFileExplorer(0, TOOLBAR_H, FILE_EXPLORER_W, wh - TOOLBAR_H - STATUS_H - TERMINAL_H);

        // Editor
        renderEditor(FILE_EXPLORER_W, TOOLBAR_H + TAB_H, ww - FILE_EXPLORER_W, wh - TOOLBAR_H - TAB_H - STATUS_H - TERMINAL_H);

        // Terminal
        renderTerminal(0, wh - TERMINAL_H - STATUS_H, ww, TERMINAL_H);

        // Status bar
        renderStatusBar(0, wh - STATUS_H, ww);

        // Handle clicks
        if (mouse.pressed) {
            if (mouseY_ < TOOLBAR_H) {
                handleToolbarClick(mouseX_, mouseY_, 0, 0, ww);
            } else if (mouseY_ >= TOOLBAR_H && mouseY_ < TOOLBAR_H + TAB_H && mouseX_ >= FILE_EXPLORER_W) {
                handleTabClick(mouseX_, mouseY_, FILE_EXPLORER_W, TOOLBAR_H, ww - FILE_EXPLORER_W);
            } else if (mouseX_ < FILE_EXPLORER_W && mouseY_ >= TOOLBAR_H && mouseY_ < wh - TERMINAL_H - STATUS_H) {
                handleFileExplorerClick(mouseX_, mouseY_, 0, TOOLBAR_H, FILE_EXPLORER_W, wh - TOOLBAR_H - STATUS_H - TERMINAL_H);
            } else if (mouseX_ >= FILE_EXPLORER_W && mouseY_ >= TOOLBAR_H + TAB_H && mouseY_ < wh - TERMINAL_H - STATUS_H) {
                editorFocused_ = true;
                terminalFocused_ = false;
                explorerFocused_ = false;
                handleEditorClick(mouseX_, mouseY_, FILE_EXPLORER_W, TOOLBAR_H + TAB_H, ww - FILE_EXPLORER_W, wh - TOOLBAR_H - TAB_H - STATUS_H - TERMINAL_H);
            } else if (mouseY_ >= wh - TERMINAL_H - STATUS_H && mouseY_ < wh - STATUS_H) {
                terminalFocused_ = true;
                editorFocused_ = false;
                explorerFocused_ = false;
            }
        }

        ui_.present();
        ui_.delay(16);
    }
}

// ============================================================
// Toolbar
// ============================================================

void ForgeIDE::renderToolbar(int x, int y, int w) {
    ui_.drawRect(x, y, w, TOOLBAR_H, theme_.toolbarR, theme_.toolbarG, theme_.toolbarB, 255);

    // App name
    ui_.drawText(x + 10, y + 12, "Forge IDE", theme_.accentR, theme_.accentG, theme_.accentB, 255);

    // Buttons
    int bx = 120;
    auto drawBtn = [&](const std::string& label, int bw) {
        bool hover = mouseX_ >= bx && mouseX_ <= bx + bw && mouseY_ >= y && mouseY_ <= y + TOOLBAR_H;
        uint8_t r = hover ? theme_.accentR : theme_.fgR;
        uint8_t g = hover ? theme_.accentG : theme_.fgG;
        uint8_t b = hover ? theme_.accentB : theme_.fgB;
        if (hover) ui_.drawRect(bx, y + 4, bw, TOOLBAR_H - 8, 50, 50, 70, 255);
        ui_.drawText(bx + 8, y + 12, label, r, g, b, 255);
        bool clicked = hover && mousePressed_;
        bx += bw + 4;
        return clicked;
    };

    if (drawBtn("New", 50)) {
        // Create untitled file
        IDEFile f;
        f.name = "untitled.fge";
        f.path = "";
        f.content = "";
        f.lines = {""};
        f.cursorRow = 0;
        f.cursorCol = 0;
        openFiles_.push_back(f);
        activeFileIndex_ = (int)openFiles_.size() - 1;
        editorFocused_ = true;
        terminalFocused_ = false;
    }
    if (drawBtn("Open", 55)) {
        // Refresh file tree
        buildFileTree(currentDir_, fileTree_);
    }
    if (drawBtn("Save", 50) && activeFileIndex_ >= 0) {
        auto& f = openFiles_[activeFileIndex_];
        f.content = "";
        for (size_t i = 0; i < f.lines.size(); i++) {
            f.content += f.lines[i];
            if (i + 1 < f.lines.size()) f.content += "\n";
        }
        if (!f.path.empty()) {
            writeFileContent(f.path, f.content);
            f.modified = false;
        }
    }
    if (drawBtn("Run", 45) && activeFileIndex_ >= 0) {
        auto& f = openFiles_[activeFileIndex_];
        if (!f.path.empty()) {
            runCommand("forge " + f.path);
        } else {
            terminalOutput_ += "Error: Save the file first\n";
        }
    }
    if (drawBtn("Build", 55) && activeFileIndex_ >= 0) {
        runCommand("forgevm compile " + openFiles_[activeFileIndex_].path);
    }
}

void ForgeIDE::handleToolbarClick(int mx, int my, int x, int y, int w) {
    // Handled in renderToolbar via drawBtn
}

// ============================================================
// Tabs
// ============================================================

void ForgeIDE::renderTabs(int x, int y, int w) {
    ui_.drawRect(x, y, w, TAB_H, theme_.panelR, theme_.panelG, theme_.panelB, 255);
    int tx = x;
    for (int i = 0; i < (int)openFiles_.size(); i++) {
        int tw = (int)openFiles_[i].name.size() * 8 + 24;
        if (tx + tw > x + w) break;
        bool active = (i == activeFileIndex_);
        if (active) {
            ui_.drawRect(tx, y, tw, TAB_H, theme_.bgR, theme_.bgG, theme_.bgB, 255);
            ui_.drawRect(tx, y + TAB_H - 2, tw, 2, theme_.accentR, theme_.accentG, theme_.accentB, 255);
        }
        std::string label = openFiles_[i].name;
        if (openFiles_[i].modified) label += " *";
        ui_.drawText(tx + 8, y + 10, label,
                     active ? theme_.fgR : theme_.lineNumR,
                     active ? theme_.fgG : theme_.lineNumG,
                     active ? theme_.fgB : theme_.lineNumB, 255);
        tx += tw;
    }
}

void ForgeIDE::handleTabClick(int mx, int my, int x, int y, int w) {
    int tx = x;
    for (int i = 0; i < (int)openFiles_.size(); i++) {
        int tw = (int)openFiles_[i].name.size() * 8 + 24;
        if (tx + tw > x + w) break;
        if (mx >= tx && mx <= tx + tw) {
            activeFileIndex_ = i;
            editorFocused_ = true;
            terminalFocused_ = false;
            return;
        }
        tx += tw;
    }
}

// ============================================================
// File Explorer
// ============================================================

void ForgeIDE::renderFileExplorer(int x, int y, int w, int h) {
    ui_.drawRect(x, y, w, h, theme_.panelR, theme_.panelG, theme_.panelB, 255);
    ui_.drawRect(x, y, w, 22, theme_.toolbarR, theme_.toolbarG, theme_.toolbarB, 255);
    ui_.drawText(x + 8, y + 7, "Files", theme_.fgR, theme_.fgG, theme_.fgB, 255);

    int ey = y + 26;
    int lineH = 18;
    for (auto& entry : fileTree_) {
        if (ey > y + h) break;
        int indent = entry.depth * 12;
        bool hover = mouseX_ >= x && mouseX_ <= x + w && mouseY_ >= ey && mouseY_ <= ey + lineH;
        if (hover) ui_.drawRect(x, ey, w, lineH, theme_.selR, theme_.selG, theme_.selB, 255);

        if (entry.isDir) {
            std::string icon = entry.expanded ? "- " : "+ ";
            ui_.drawText(x + 8 + indent, ey + 3, icon + entry.name,
                         theme_.dirR, theme_.dirG, theme_.dirB, 255);
        } else {
            ui_.drawText(x + 8 + indent, ey + 3, entry.name,
                         theme_.fileR, theme_.fileG, theme_.fileB, 255);
        }
        ey += lineH;
    }
}

void ForgeIDE::handleFileExplorerClick(int mx, int my, int x, int y, int w, int h) {
    int ey = y + 26;
    int lineH = 18;
    for (auto& entry : fileTree_) {
        if (ey > y + h) break;
        if (mx >= x && mx <= x + w && my >= ey && my <= ey + lineH) {
            if (entry.isDir) {
                entry.expanded = !entry.expanded;
                if (entry.expanded) {
                    buildFileTree(entry.fullPath, entry.children, entry.depth + 1);
                    // Insert children after this entry
                    auto it = std::find_if(fileTree_.begin(), fileTree_.end(),
                        [&](const IDEFileEntry& e) { return e.fullPath == entry.fullPath; });
                    if (it != fileTree_.end()) {
                        auto pos = it - fileTree_.begin() + 1;
                        fileTree_.insert(fileTree_.begin() + pos, entry.children.begin(), entry.children.end());
                    }
                } else {
                    // Remove children
                    fileTree_.erase(
                        std::remove_if(fileTree_.begin(), fileTree_.end(),
                            [&](const IDEFileEntry& e) {
                                return e.depth > entry.depth &&
                                    e.fullPath.find(entry.fullPath) == 0;
                            }),
                        fileTree_.end());
                }
            } else {
                openFile(entry.fullPath);
            }
            return;
        }
        ey += lineH;
    }
}

// ============================================================
// Editor
// ============================================================

void ForgeIDE::renderEditor(int x, int y, int w, int h) {
    ui_.drawRect(x, y, w, h, theme_.bgR, theme_.bgG, theme_.bgB, 255);

    if (activeFileIndex_ < 0 || activeFileIndex_ >= (int)openFiles_.size()) {
        ui_.drawText(x + w / 2 - 80, y + h / 2 - 8, "Open a file from the explorer",
                     theme_.lineNumR, theme_.lineNumG, theme_.lineNumB, 255);
        return;
    }

    auto& file = openFiles_[activeFileIndex_];
    int lineH = 16;
    int gutterW = 50;
    int startY = y - file.scrollY * lineH;
    int firstLine = file.scrollY;
    int visibleLines = h / lineH + 1;

    // Draw gutter
    ui_.drawRect(x, y, gutterW, h, theme_.panelR, theme_.panelG, theme_.panelB, 255);

    for (int i = 0; i < visibleLines && firstLine + i < (int)file.lines.size(); i++) {
        int ly = startY + i * lineH;
        if (ly + lineH < y || ly > y + h) continue;
        int lineNum = firstLine + i + 1;

        // Line number
        std::string numStr = std::to_string(lineNum);
        while (numStr.size() < 4) numStr = " " + numStr;
        ui_.drawText(x + 4, ly + 2, numStr, theme_.lineNumR, theme_.lineNumG, theme_.lineNumB, 255);

        // Syntax-highlighted text
        auto colors = highlightLine(file.lines[firstLine + i]);
        int tx = x + gutterW + 4;
        for (size_t c = 0; c < file.lines[firstLine + i].size(); c++) {
            uint8_t cr = colors[c].r, cg = colors[c].g, cb = colors[c].b;
            // Selection highlight
            if (file.cursorRow == firstLine + i) {
                // cursor line highlight
            }
            std::string chStr(1, file.lines[firstLine + i][c]);
            ui_.drawText(tx, ly + 2, chStr, cr, cg, cb, 255);
            tx += 8;
        }

        // Cursor
        if (file.cursorRow == firstLine + i) {
            int cx = x + gutterW + 4 + file.cursorCol * 8;
            // Blink effect
            uint32_t t = ui_.ticks();
            if ((t / 500) % 2 == 0) {
                ui_.drawRect(cx, ly + 1, 2, lineH - 2, theme_.accentR, theme_.accentG, theme_.accentB, 255);
            }
        }
    }

    // Cursor line highlight
    if (activeFileIndex_ >= 0 && file.cursorRow >= firstLine && file.cursorRow < firstLine + visibleLines) {
        int ly = startY + (file.cursorRow - firstLine) * lineH;
        ui_.drawRect(x + gutterW, ly, w - gutterW, lineH, theme_.selR, theme_.selG, theme_.selB, 40);
    }
}

std::vector<ForgeIDE::SyntaxColor> ForgeIDE::highlightLine(const std::string& line) {
    std::vector<SyntaxColor> colors(line.size());
    size_t i = 0;
    while (i < line.size()) {
        // Comment
        if (line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/') {
            for (size_t j = i; j < line.size(); j++) {
                colors[j] = {theme_.commentR, theme_.commentG, theme_.commentB};
            }
            break;
        }
        // String
        if (line[i] == '"' || line[i] == '\'') {
            char q = line[i];
            colors[i] = {theme_.stringR, theme_.stringG, theme_.stringB};
            i++;
            while (i < line.size() && line[i] != q) {
                if (line[i] == '\\' && i + 1 < line.size()) {
                    colors[i] = {theme_.stringR, theme_.stringG, theme_.stringB};
                    i++;
                }
                colors[i] = {theme_.stringR, theme_.stringG, theme_.stringB};
                i++;
            }
            if (i < line.size()) { colors[i] = {theme_.stringR, theme_.stringG, theme_.stringB}; i++; }
            continue;
        }
        // Number
        if (std::isdigit(line[i]) || (line[i] == '.' && i + 1 < line.size() && std::isdigit(line[i + 1]))) {
            while (i < line.size() && (std::isdigit(line[i]) || line[i] == '.')) {
                colors[i] = {theme_.numberR, theme_.numberG, theme_.numberB};
                i++;
            }
            continue;
        }
        // Keywords
        if (std::isalpha(line[i]) || line[i] == '_') {
            size_t start = i;
            while (i < line.size() && (std::isalnum(line[i]) || line[i] == '_')) i++;
            std::string word = line.substr(start, i - start);
            static const char* keywords[] = {
                "fn", "var", "val", "if", "else", "while", "for", "return",
                "class", "struct", "impl", "trait", "import", "from", "as",
                "new", "this", "super", "null", "true", "false", "break",
                "continue", "switch", "case", "default", "match", "enum",
                "const", "static", "let", "pub", "mod", "use", "test"
            };
            bool isKw = false;
            for (auto& kw : keywords) {
                if (word == kw) { isKw = true; break; }
            }
            if (isKw) {
                for (size_t j = start; j < i; j++) {
                    colors[j] = {theme_.keywordR, theme_.keywordG, theme_.keywordB};
                }
            } else if (i < line.size() && line[i] == '(') {
                for (size_t j = start; j < i; j++) {
                    colors[j] = {theme_.accentR, theme_.accentG, theme_.accentB};
                }
            }
            continue;
        }
        // Default
        colors[i] = {theme_.fgR, theme_.fgG, theme_.fgB};
        i++;
    }
    return colors;
}

void ForgeIDE::handleEditorInput(int key, char ch, bool shift, bool ctrl) {
    if (activeFileIndex_ < 0) return;
    auto& file = openFiles_[activeFileIndex_];

    // Ctrl+S: save
    if (ctrl && (key == SDLK_s || key == 115)) {
        file.content = "";
        for (size_t i = 0; i < file.lines.size(); i++) {
            file.content += file.lines[i];
            if (i + 1 < file.lines.size()) file.content += "\n";
        }
        if (!file.path.empty()) {
            writeFileContent(file.path, file.content);
            file.modified = false;
        }
        return;
    }

    // Ctrl+C: copy
    if (ctrl && (key == SDLK_c || key == 99)) {
        if (!file.lines.empty() && file.cursorRow < (int)file.lines.size()) {
            std::string line = file.lines[file.cursorRow];
            SDL_SetClipboardText(line.c_str());
        }
        return;
    }

    // Ctrl+V: paste
    if (ctrl && (key == SDLK_v || key == 118)) {
        const char* clip = SDL_GetClipboardText();
        if (clip) {
            std::string text(clip);
            for (char c : text) {
                if (c == '\n') {
                    std::string rest = file.lines[file.cursorRow].substr(file.cursorCol);
                    file.lines[file.cursorRow] = file.lines[file.cursorRow].substr(0, file.cursorCol);
                    file.lines.insert(file.lines.begin() + file.cursorRow + 1, rest);
                    file.cursorRow++;
                    file.cursorCol = 0;
                } else {
                    file.lines[file.cursorRow].insert(file.lines[file.cursorRow].begin() + file.cursorCol, c);
                    file.cursorCol++;
                }
            }
            file.modified = true;
        }
        return;
    }

    // Ctrl+X: cut
    if (ctrl && (key == SDLK_x || key == 120)) {
        if (!file.lines.empty() && file.cursorRow < (int)file.lines.size()) {
            SDL_SetClipboardText(file.lines[file.cursorRow].c_str());
            file.lines[file.cursorRow].clear();
            file.cursorCol = 0;
            file.modified = true;
        }
        return;
    }

    // Ctrl+A: select all
    if (ctrl && (key == SDLK_a || key == 97)) {
        file.cursorRow = (int)file.lines.size() - 1;
        file.cursorCol = (int)file.lines[file.cursorRow].size();
        return;
    }

    // Navigation
    if (key == SDLK_LEFT || key == SDLK_KP_4) {
        if (file.cursorCol > 0) file.cursorCol--;
        else if (file.cursorRow > 0) {
            file.cursorRow--;
            file.cursorCol = (int)file.lines[file.cursorRow].size();
        }
        return;
    }
    if (key == SDLK_RIGHT || key == SDLK_KP_6) {
        if (file.cursorCol < (int)file.lines[file.cursorRow].size()) file.cursorCol++;
        else if (file.cursorRow < (int)file.lines.size() - 1) {
            file.cursorRow++;
            file.cursorCol = 0;
        }
        return;
    }
    if (key == SDLK_UP || key == SDLK_KP_8) {
        if (file.cursorRow > 0) {
            file.cursorRow--;
            file.cursorCol = std::min(file.cursorCol, (int)file.lines[file.cursorRow].size());
        }
        return;
    }
    if (key == SDLK_DOWN || key == SDLK_KP_2) {
        if (file.cursorRow < (int)file.lines.size() - 1) {
            file.cursorRow++;
            file.cursorCol = std::min(file.cursorCol, (int)file.lines[file.cursorRow].size());
        }
        return;
    }

    // Home/End
    if (key == SDLK_HOME) { file.cursorCol = 0; return; }
    if (key == SDLK_END) { file.cursorCol = (int)file.lines[file.cursorRow].size(); return; }

    // Enter
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        std::string rest = file.lines[file.cursorRow].substr(file.cursorCol);
        file.lines[file.cursorRow] = file.lines[file.cursorRow].substr(0, file.cursorCol);
        file.lines.insert(file.lines.begin() + file.cursorRow + 1, rest);
        file.cursorRow++;
        file.cursorCol = 0;
        file.modified = true;
        return;
    }

    // Tab
    if (key == SDLK_TAB) {
        file.lines[file.cursorRow].insert(file.cursorCol, "    ");
        file.cursorCol += 4;
        file.modified = true;
        return;
    }

    // Backspace
    if (key == SDLK_BACKSPACE) {
        if (file.cursorCol > 0) {
            file.lines[file.cursorRow].erase(file.cursorCol - 1, 1);
            file.cursorCol--;
        } else if (file.cursorRow > 0) {
            file.cursorCol = (int)file.lines[file.cursorRow - 1].size();
            file.lines[file.cursorRow - 1] += file.lines[file.cursorRow];
            file.lines.erase(file.lines.begin() + file.cursorRow);
            file.cursorRow--;
        }
        file.modified = true;
        return;
    }

    // Delete
    if (key == SDLK_DELETE) {
        if (file.cursorCol < (int)file.lines[file.cursorRow].size()) {
            file.lines[file.cursorRow].erase(file.cursorCol, 1);
        } else if (file.cursorRow < (int)file.lines.size() - 1) {
            file.lines[file.cursorRow] += file.lines[file.cursorRow + 1];
            file.lines.erase(file.lines.begin() + file.cursorRow + 1);
        }
        file.modified = true;
        return;
    }

    // Printable character
    if (ch >= 32 && ch < 127) {
        file.lines[file.cursorRow].insert(file.lines[file.cursorRow].begin() + file.cursorCol, ch);
        file.cursorCol++;
        file.modified = true;
        return;
    }
}

void ForgeIDE::handleEditorClick(int mx, int my, int x, int y, int w, int h) {
    if (activeFileIndex_ < 0) return;
    auto& file = openFiles_[activeFileIndex_];
    int lineH = 16;
    int gutterW = 50;

    int row = (my - y) / lineH + file.scrollY;
    int col = (mx - x - gutterW - 4) / 8;

    if (row >= 0 && row < (int)file.lines.size()) {
        file.cursorRow = row;
        file.cursorCol = std::max(0, std::min(col, (int)file.lines[row].size()));
    }
}

// ============================================================
// Terminal
// ============================================================

void ForgeIDE::renderTerminal(int x, int y, int w, int h) {
    ui_.drawRect(x, y, w, h, theme_.panelR, theme_.panelG, theme_.panelB, 255);
    ui_.drawRect(x, y, w, 22, theme_.toolbarR, theme_.toolbarG, theme_.toolbarB, 255);
    ui_.drawText(x + 8, y + 7, "Terminal", theme_.fgR, theme_.fgG, theme_.fgB, 255);

    // Output
    int ty = y + 26;
    int lineH = 14;
    std::istringstream stream(terminalOutput_);
    std::string line;
    std::vector<std::string> outputLines;
    while (std::getline(stream, line)) outputLines.push_back(line);
    int firstLine = std::max(0, (int)outputLines.size() - (h - 46) / lineH);
    for (int i = firstLine; i < (int)outputLines.size(); i++) {
        if (ty + lineH > y + h - 20) break;
        ui_.drawText(x + 8, ty, outputLines[i], theme_.fgR, theme_.fgG, theme_.fgB, 255);
        ty += lineH;
    }

    // Input prompt
    int iy = y + h - 18;
    ui_.drawRect(x, iy, w, 18, 20, 20, 30, 255);
    std::string prompt = "> " + terminalInput_;
    ui_.drawText(x + 8, iy + 2, prompt, theme_.accentR, theme_.accentG, theme_.accentB, 255);

    // Cursor blink
    if (terminalFocused_) {
        uint32_t t = ui_.ticks();
        if ((t / 500) % 2 == 0) {
            int cx = x + 8 + (int)prompt.size() * 8;
            ui_.drawRect(cx, iy + 2, 8, 14, theme_.accentR, theme_.accentG, theme_.accentB, 255);
        }
    }
}

void ForgeIDE::handleTerminalInput(int key, char ch) {
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        if (!terminalInput_.empty()) {
            terminalOutput_ += "$ " + terminalInput_ + "\n";
            runCommand(terminalInput_);
            terminalInput_.clear();
        }
        return;
    }
    if (key == SDLK_BACKSPACE) {
        if (!terminalInput_.empty()) terminalInput_.pop_back();
        return;
    }
    if (ch >= 32 && ch < 127) {
        terminalInput_ += ch;
    }
}

void ForgeIDE::runCommand(const std::string& cmd) {
    terminalOutput_ += "$ " + cmd + "\n";
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe) {
        terminalOutput_ += "Error: Failed to execute command\n";
        return;
    }
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) {
        terminalOutput_ += buf;
    }
    int status = pclose(pipe);
    if (status != 0) {
        terminalOutput_ += "[exit code: " + std::to_string(status) + "]\n";
    }
}

// ============================================================
// Status bar
// ============================================================

void ForgeIDE::renderStatusBar(int x, int y, int w) {
    ui_.drawRect(x, y, w, STATUS_H, theme_.statusR, theme_.statusG, theme_.statusB, 255);

    if (activeFileIndex_ >= 0 && activeFileIndex_ < (int)openFiles_.size()) {
        auto& f = openFiles_[activeFileIndex_];
        std::string info = f.name;
        if (f.modified) info += " [modified]";
        info += "  Line " + std::to_string(f.cursorRow + 1) + ", Col " + std::to_string(f.cursorCol + 1);
        info += "  " + std::to_string(f.lines.size()) + " lines";
        ui_.drawText(x + 8, y + 5, info, theme_.fgR, theme_.fgG, theme_.fgB, 255);
    } else {
        ui_.drawText(x + 8, y + 5, "Forge IDE", theme_.fgR, theme_.fgG, theme_.fgB, 255);
    }
}

// ============================================================
// File I/O
// ============================================================

std::string ForgeIDE::readFileContent(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void ForgeIDE::writeFileContent(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (f.is_open()) f << content;
}

void ForgeIDE::openFile(const std::string& path) {
    // Check if already open
    for (int i = 0; i < (int)openFiles_.size(); i++) {
        if (openFiles_[i].path == path) {
            activeFileIndex_ = i;
            editorFocused_ = true;
            terminalFocused_ = false;
            return;
        }
    }
    IDEFile f;
    f.path = path;
    f.name = fs::path(path).filename().string();
    f.content = readFileContent(path);
    f.lines.clear();
    std::istringstream stream(f.content);
    std::string line;
    while (std::getline(stream, line)) f.lines.push_back(line);
    if (f.lines.empty()) f.lines.push_back("");
    f.cursorRow = 0;
    f.cursorCol = 0;
    openFiles_.push_back(f);
    activeFileIndex_ = (int)openFiles_.size() - 1;
    editorFocused_ = true;
    terminalFocused_ = false;
}

void ForgeIDE::closeFile(int index) {
    if (index < 0 || index >= (int)openFiles_.size()) return;
    openFiles_.erase(openFiles_.begin() + index);
    if (activeFileIndex_ >= (int)openFiles_.size()) {
        activeFileIndex_ = (int)openFiles_.size() - 1;
    }
}

int ForgeIDE::charAtPosition(const std::string& line, int px, int textX) {
    int col = (px - textX) / 8;
    return std::max(0, std::min(col, (int)line.size()));
}

// ============================================================
// Entry point
// ============================================================

int runForgeIDE(int width, int height) {
    ForgeIDE ide;
    if (!ide.init(width, height)) return 1;
    ide.run();
    return 0;
}

} // namespace forge::fvm
