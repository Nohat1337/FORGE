#include "forge_ide.hpp"
#include <SDL2/SDL.h>
#include <cstring>
#include <climits>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>
#include <filesystem>
#include <iostream>

namespace forge::fvm {

namespace fs = std::filesystem;

// ============================================================
// Themes
// ============================================================

IDETheme ForgeIDE::themeVscodeDark() {
    IDETheme t;
    // Surfaces (VS Code Dark+)
    t.bgR = 30;  t.bgG = 30;  t.bgB = 30;      // editor #1E1E1E
    t.panelR = 37; t.panelG = 37; t.panelB = 38;   // sidebar #252526
    t.activityR = 51; t.activityG = 51; t.activityB = 51; // #333333
    t.titleR = 55; t.titleG = 55; t.titleB = 56;   // title bar
    t.statusR = 0; t.statusG = 122; t.statusB = 204; // #007ACC
    t.statusFgR = 255; t.statusFgG = 255; t.statusFgB = 255;
    // Text
    t.fgR = 212; t.fgG = 212; t.fgB = 212;     // #D4D4D4
    t.accentR = 0; t.accentG = 122; t.accentB = 204;
    t.lineNumR = 133; t.lineNumG = 133; t.lineNumB = 133;
    t.lineNumActiveR = 198; t.lineNumActiveG = 198; t.lineNumActiveB = 198;
    t.selR = 38; t.selG = 79; t.selB = 120;    // #264F78
    t.curLineR = 255; t.curLineG = 255; t.curLineB = 255; t.curLineA = 12;
    // Tabs
    t.tabBarR = 37; t.tabBarG = 37; t.tabBarB = 38;
    t.tabActiveR = 30; t.tabActiveG = 30; t.tabActiveB = 30;
    t.tabInactiveR = 43; t.tabInactiveG = 43; t.tabInactiveB = 43;
    // Tree
    t.dirR = 231; t.dirG = 231; t.dirB = 231;
    t.fileR = 204; t.fileG = 204; t.fileB = 204;
    // Syntax (Dark+)
    t.keywordR = 86; t.keywordG = 156; t.keywordB = 214;   // #569CD6
    t.controlR = 197; t.controlG = 134; t.controlB = 192;  // #C586C0
    t.typeR = 78; t.typeG = 201; t.typeB = 176;            // #4EC9B0
    t.funcR = 220; t.funcG = 220; t.funcB = 170;           // #DCDCAA
    t.stringR = 206; t.stringG = 145; t.stringB = 120;     // #CE9178
    t.numberR = 181; t.numberG = 206; t.numberB = 168;     // #B5CEA8
    t.commentR = 106; t.commentG = 153; t.commentB = 85;   // #6A9955
    // Extras
    t.minimapR = 130; t.minimapG = 130; t.minimapB = 140;
    t.borderR = 48; t.borderG = 48; t.borderB = 50;
    return t;
}

IDETheme ForgeIDE::themeForgeNight() {
    IDETheme t = themeVscodeDark();
    t.bgR = 30;  t.bgG = 30;  t.bgB = 46;
    t.panelR = 24; t.panelG = 24; t.panelB = 37;
    t.activityR = 17; t.activityG = 17; t.activityB = 27;
    t.titleR = 17; t.titleG = 17; t.titleB = 27;
    t.statusR = 137; t.statusG = 180; t.statusB = 250; t.statusFgR = 17; t.statusFgG = 17; t.statusFgB = 27;
    t.fgR = 205; t.fgG = 214; t.fgB = 244;
    t.accentR = 137; t.accentG = 180; t.accentB = 250;
    t.lineNumR = 88; t.lineNumG = 91; t.lineNumB = 112;
    t.lineNumActiveR = 205; t.lineNumActiveG = 214; t.lineNumActiveB = 244;
    t.selR = 69; t.selG = 71; t.selB = 90;
    t.keywordR = 137; t.keywordG = 180; t.keywordB = 250;
    t.controlR = 203; t.controlG = 166; t.controlB = 247;
    t.typeR = 137; t.typeG = 220; t.typeB = 235;
    t.funcR = 250; t.funcG = 179; t.funcB = 135;
    t.stringR = 166; t.stringG = 227; t.stringB = 161;
    t.numberR = 250; t.numberG = 179; t.numberB = 135;
    t.commentR = 108; t.commentG = 112; t.commentB = 134;
    t.tabBarR = 24; t.tabBarG = 24; t.tabBarB = 37;
    t.tabActiveR = 30; t.tabActiveG = 30; t.tabActiveB = 46;
    t.tabInactiveR = 31; t.tabInactiveG = 32; t.tabInactiveB = 48;
    t.minimapR = 110; t.minimapG = 115; t.minimapB = 141;
    t.borderR = 40; t.borderG = 40; t.borderB = 60;
    return t;
}

IDETheme ForgeIDE::themeLight() {
    IDETheme t;
    t.bgR = 255; t.bgG = 255; t.bgB = 255;
    t.panelR = 243; t.panelG = 243; t.panelB = 243;
    t.activityR = 44; t.activityG = 44; t.activityB = 44;
    t.titleR = 221; t.titleG = 221; t.titleB = 221;
    t.statusR = 0; t.statusG = 120; t.statusB = 215;
    t.statusFgR = 255; t.statusFgG = 255; t.statusFgB = 255;
    t.fgR = 51; t.fgG = 51; t.fgB = 51;
    t.accentR = 0; t.accentG = 120; t.accentB = 215;
    t.lineNumR = 153; t.lineNumG = 153; t.lineNumB = 153;
    t.lineNumActiveR = 23; t.lineNumActiveG = 23; t.lineNumActiveB = 23;
    t.selR = 173; t.selG = 214; t.selB = 255;
    t.curLineR = 0; t.curLineG = 0; t.curLineB = 0; t.curLineA = 8;
    t.tabBarR = 236; t.tabBarG = 236; t.tabBarB = 236;
    t.tabActiveR = 255; t.tabActiveG = 255; t.tabActiveB = 255;
    t.tabInactiveR = 243; t.tabInactiveG = 243; t.tabInactiveB = 243;
    t.dirR = 43; t.dirG = 43; t.dirB = 43;
    t.fileR = 70; t.fileG = 70; t.fileB = 70;
    t.keywordR = 0; t.keywordG = 0; t.keywordB = 255;
    t.controlR = 175; t.controlG = 0; t.controlB = 219;
    t.typeR = 38; t.typeG = 127; t.typeB = 153;
    t.funcR = 121; t.funcG = 94; t.funcB = 38;
    t.stringR = 163; t.stringG = 21; t.stringB = 21;
    t.numberR = 9; t.numberG = 134; t.numberB = 49;
    t.commentR = 0; t.commentG = 128; t.commentB = 0;
    t.minimapR = 170; t.minimapG = 170; t.minimapB = 175;
    t.borderR = 216; t.borderG = 216; t.borderB = 216;
    return t;
}

void ForgeIDE::applyTheme(int index) {
    selectedTheme_ = index % 3;
    switch (selectedTheme_) {
        case 0: theme_ = themeVscodeDark(); break;
        case 1: theme_ = themeForgeNight(); break;
        case 2: theme_ = themeLight(); break;
    }
}

// ============================================================
// Lifecycle
// ============================================================

ForgeIDE::ForgeIDE() : ui_(SDL2UI::instance()), theme_(themeVscodeDark()) {}

ForgeIDE::~ForgeIDE() { shutdown(); }

bool ForgeIDE::init(int width, int height) {
    if (!ui_.init(width, height, "Forge IDE")) {
        std::cerr << "Failed to initialize SDL2 for IDE\n";
        return false;
    }
    running_ = true;
    currentDir_ = fs::current_path().string();
    buildFileTree(currentDir_, fileTree_);
    scanProjectFiles(currentDir_);
    return true;
}

void ForgeIDE::shutdown() {
    ui_.shutdown();
    running_ = false;
}

// ============================================================
// Project scan / file tree
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
            if (name[0] == '.' || name == "build" || name == "node_modules") continue;
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

static void scanRecursive(const std::string& dir, std::vector<std::string>& out, int depth) {
    if (depth > 8) return;
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        std::string name = ent->d_name;
        if (name == "." || name == ".." || name[0] == '.') continue;
        if (name == "build" || name == "node_modules") continue;
        std::string full = dir + "/" + name;
        struct stat st;
        if (stat(full.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) scanRecursive(full, out, depth + 1);
        else out.push_back(full);
    }
    closedir(d);
}

void ForgeIDE::scanProjectFiles(const std::string& dir) {
    allFiles_.clear();
    scanRecursive(dir, allFiles_, 0);
    std::sort(allFiles_.begin(), allFiles_.end());
}

// ============================================================
// Fuzzy subsequence match for quick-open (case-insensitive).
// Higher score = better: consecutive hits and word starts win.
// ============================================================

bool ForgeIDE::subsequenceMatch(const std::string& pattern, const std::string& text, int* scoreOut) {
    if (pattern.empty()) { if (scoreOut) *scoreOut = 0; return true; }
    auto lower = [](char c) { return (char)std::tolower((unsigned char)c); };
    int score = 0;
    size_t ti = 0;
    int lastHit = -2;
    for (size_t pi = 0; pi < pattern.size(); pi++) {
        char pc = lower(pattern[pi]);
        bool found = false;
        while (ti < text.size()) {
            char tc = lower(text[ti]);
            if (tc == pc) {
                if ((int)ti == lastHit + 1) score += 4;      // consecutive
                else score += 1;
                if (ti == 0 || text[ti - 1] == '/' || text[ti - 1] == '_' ||
                    text[ti - 1] == '-' || text[ti - 1] == '.')
                    score += 3;                               // word start
                lastHit = (int)ti;
                ti++;
                found = true;
                break;
            }
            ti++;
        }
        if (!found) return false;
    }
    if (scoreOut) *scoreOut = score - (int)(text.size() / 16); // prefer shorter paths
    return true;
}

// ============================================================
// Main run loop
// ============================================================

void ForgeIDE::run() {
    int frame = 0;
    const char* shotPath = std::getenv("FORGE_IDE_SCREENSHOT");
    while (running_) {
        SDLKeyEvent key;
        SDLMouseEvent mouse;
        SDLWindowEvent win;
        if (!ui_.pumpEvents(&key, &mouse, &win)) { running_ = false; break; }
        if (win.closed) { running_ = false; break; }

        mouseX_ = mouse.x;
        mouseY_ = mouse.y;
        mousePressed_ = mouse.pressed && mouse.button == 1;

        if (key.keycode != 0) {
            lastKey_ = key.keycode;
            lastChar_ = key.ch;
            keyShift_ = key.shift;
            keyCtrl_ = key.ctrl;
            keyAlt_ = key.alt;

            if (paletteOpen_) {
                handlePaletteKey(key.keycode, key.ch);
            } else {
                // Global shortcuts
                if (key.ctrl && key.keycode == SDLK_p) {
                    paletteOpen_ = true;
                    paletteInput_.clear();
                    paletteSelected_ = 0;
                    updatePalette();
                } else if (key.ctrl && key.keycode == SDLK_n) {
                    IDEFile f;
                    f.name = "untitled-" + std::to_string(openFiles_.size() + 1) + ".fge";
                    f.path = "";
                    f.lines = {""};
                    openFiles_.push_back(f);
                    activeFileIndex_ = (int)openFiles_.size() - 1;
                    editorFocused_ = true; terminalFocused_ = false;
                } else if (key.ctrl && key.keycode == SDLK_s) {
                    saveActiveFile();
                } else if (key.ctrl && key.keycode == SDLK_w) {
                    closeFile(activeFileIndex_);
                } else if (key.keycode == SDLK_ESCAPE) {
                    paletteOpen_ = false;
                } else if (key.keycode == SDLK_F5 && activeFileIndex_ >= 0 &&
                           !openFiles_[activeFileIndex_].path.empty()) {
                    saveActiveFile();
                    runCommand("forge " + openFiles_[activeFileIndex_].path);
                } else if (terminalFocused_) {
                    handleTerminalInput(key.keycode, key.ch);
                } else if (editorFocused_ && activeFileIndex_ >= 0) {
                    handleEditorInput(key.keycode, key.ch, key.shift, key.ctrl);
                } else if (filterFocused_) {
                    if (key.keycode == SDLK_BACKSPACE) {
                        if (!filterInput_.empty()) filterInput_.pop_back();
                        explorerScroll_ = 0;
                    } else if (key.ch >= 32 && key.ch < 127) {
                        filterInput_ += key.ch;
                        explorerScroll_ = 0;
                    }
                }
            }
        }

        // Mouse wheel scrolling per region
        if (mouse.wheel != 0) {
            int ww, wh; ui_.getSize(ww, wh);
            int termY = wh - TERMINAL_H - STATUS_H;
            if (paletteOpen_) {
                // no-op
            } else if (mouseY_ >= termY) {
                terminalScroll_ -= mouse.wheel * 3;
                if (terminalScroll_ < 0) terminalScroll_ = 0;
            } else if (mouseX_ >= ACTIVITY_W && mouseX_ < ACTIVITY_W + SIDEBAR_W &&
                       mouseY_ > TOOLBAR_H) {
                explorerScroll_ -= mouse.wheel * 3;
                if (explorerScroll_ < 0) explorerScroll_ = 0;
            } else if (activeFileIndex_ >= 0) {
                openFiles_[activeFileIndex_].scrollY -= mouse.wheel * 3;
                if (openFiles_[activeFileIndex_].scrollY < 0)
                    openFiles_[activeFileIndex_].scrollY = 0;
            }
        }

        // Layout
        int ww, wh;
        ui_.getSize(ww, wh);

        // Render
        ui_.clear(theme_.bgR, theme_.bgG, theme_.bgB);

        renderActivityBar(0, 0, ACTIVITY_W, wh - STATUS_H);
        renderToolbar(ACTIVITY_W, 0, ww - ACTIVITY_W);

        if (sideView_ == SideView::Explorer || sideView_ == SideView::Filter) {
            renderSidebar(ACTIVITY_W, TOOLBAR_H, SIDEBAR_W, wh - TOOLBAR_H - STATUS_H - TERMINAL_H);
        } else {
            renderSidebar(ACTIVITY_W, TOOLBAR_H, SIDEBAR_W, wh - TOOLBAR_H - STATUS_H - TERMINAL_H);
        }

        renderTabs(ACTIVITY_W + SIDEBAR_W, TOOLBAR_H, ww - ACTIVITY_W - SIDEBAR_W);

        int edX = ACTIVITY_W + SIDEBAR_W;
        int edY = TOOLBAR_H + TAB_H;
        int edW = ww - edX;
        int edH = wh - edY - STATUS_H - TERMINAL_H;

        if (openFiles_.empty()) renderWelcome(edX, TOOLBAR_H, edW, wh - TOOLBAR_H - STATUS_H - TERMINAL_H);
        else renderEditor(edX, edY, edW, edH);

        renderTerminal(0, wh - TERMINAL_H - STATUS_H, ww, TERMINAL_H);
        renderStatusBar(0, wh - STATUS_H, ww);

        if (paletteOpen_) renderPalette();

        // Click routing
        if (mousePressed_) {
            if (!paletteOpen_) {
                if (mouseX_ < ACTIVITY_W) handleActivityClick(mouseX_, mouseY_);
                else if (mouseY_ < TOOLBAR_H) { /* toolbar handled in render */ }
                else if (mouseX_ < ACTIVITY_W + SIDEBAR_W && mouseY_ < wh - TERMINAL_H - STATUS_H) {
                    handleSidebarClick(mouseX_, mouseY_, ACTIVITY_W, TOOLBAR_H, SIDEBAR_W,
                                       wh - TOOLBAR_H - STATUS_H - TERMINAL_H);
                } else if (mouseX_ >= ACTIVITY_W + SIDEBAR_W && mouseY_ >= TOOLBAR_H &&
                           mouseY_ < TOOLBAR_H + TAB_H) {
                    handleTabClick(mouseX_, mouseY_, ACTIVITY_W + SIDEBAR_W, TOOLBAR_H,
                                   ww - ACTIVITY_W - SIDEBAR_W);
                } else if (mouseX_ >= ACTIVITY_W + SIDEBAR_W && mouseY_ >= edY &&
                           mouseY_ < edY + edH) {
                    if (activeFileIndex_ >= 0) {
                        editorFocused_ = true; terminalFocused_ = false; filterFocused_ = false;
                        handleEditorClick(mouseX_, mouseY_, edX, edY, edW, edH);
                    }
                } else if (mouseY_ >= wh - TERMINAL_H - STATUS_H && mouseY_ < wh - STATUS_H) {
                    handleTerminalClick(mouseX_, mouseY_, 0, wh - TERMINAL_H - STATUS_H);
                }
            }
        }

        ui_.present();
        ui_.delay(16);
        frame++;
        if (shotPath && frame == 12) {
            ui_.saveScreenshot(shotPath);
            running_ = false;
        }
    }
}

// ============================================================
// Activity bar (left icon rail, VS Code style)
// ============================================================

static void drawIconFiles(SDL2UI& ui, int cx, int cy, uint8_t r, uint8_t g, uint8_t b) {
    // document sheet with folded corner
    ui.drawRect(cx - 7, cy - 9, 14, 18, r, g, b, 255);
    ui.drawRect(cx + 3, cy - 9, 4, 4, 40, 40, 40, 255);
    ui.drawRect(cx - 4, cy - 2, 8, 2, 40, 40, 40, 255);
    ui.drawRect(cx - 4, cy + 2, 8, 2, 40, 40, 40, 255);
}

static void drawIconSearch(SDL2UI& ui, int cx, int cy, uint8_t r, uint8_t g, uint8_t b) {
    ui.drawCircle(cx - 1, cy - 1, 6, r, g, b, 255);
    ui.drawCircle(cx - 1, cy - 1, 4, 51, 51, 51, 255);
    for (int i = 0; i <= 6; i++) ui.drawRect(cx + 2 + i, cy + 2 + i, 2, 2, r, g, b, 255);
}

static void drawIconGear(SDL2UI& ui, int cx, int cy, uint8_t r, uint8_t g, uint8_t b) {
    ui.drawCircle(cx, cy, 7, r, g, b, 255);
    ui.drawCircle(cx, cy, 3, 51, 51, 51, 255);
    ui.drawRect(cx - 2, cy - 11, 4, 3, r, g, b, 255);
    ui.drawRect(cx - 2, cy + 8, 4, 3, r, g, b, 255);
    ui.drawRect(cx - 11, cy - 2, 3, 4, r, g, b, 255);
    ui.drawRect(cx + 8, cy - 2, 3, 4, r, g, b, 255);
}

void ForgeIDE::renderActivityBar(int x, int y, int w, int h) {
    ui_.drawRect(x, y, w, h, theme_.activityR, theme_.activityG, theme_.activityB, 255);

    auto iconBtn = [&](int iy, void (*icon)(SDL2UI&, int, int, uint8_t, uint8_t, uint8_t),
                       bool active) -> bool {
        bool hover = mouseX_ >= x && mouseX_ < x + w && mouseY_ >= iy - 20 && mouseY_ < iy + 20;
        if (active) {
            ui_.drawRect(x, iy - 20, w, 40, theme_.selR, theme_.selG, theme_.selB, 90);
            ui_.drawRect(x, iy - 20, 3, 40, theme_.accentR, theme_.accentG, theme_.accentB, 255);
        } else if (hover) {
            ui_.drawRect(x, iy - 20, w, 40, 255, 255, 255, 14);
        }
        uint8_t ic = active ? 255 : 150;
        icon(ui_, x + w / 2, iy, ic, ic, ic);
        return hover && mousePressed_;
    };

    int cy = y + 34;
    if (iconBtn(cy, drawIconFiles, sideView_ == SideView::Explorer)) {
        sideView_ = SideView::Explorer;
        explorerScroll_ = 0;
    }
    cy += 52;
    if (iconBtn(cy, drawIconSearch, false)) {
        // Search jumps to explorer with filter focused
        sideView_ = SideView::Explorer;
        filterFocused_ = true;
        editorFocused_ = false;
        terminalFocused_ = false;
    }

    // Settings pinned to bottom
    if (iconBtn(y + h - 34, drawIconGear, sideView_ == SideView::Settings)) {
        sideView_ = SideView::Settings;
    }
}

void ForgeIDE::handleActivityClick(int mx, int my) {
    (void)mx; (void)my; // handled in renderActivityBar (immediate mode)
}

// ============================================================
// Toolbar (title bar)
// ============================================================

void ForgeIDE::renderToolbar(int x, int y, int w) {
    ui_.drawRect(x, y, w, TOOLBAR_H, theme_.titleR, theme_.titleG, theme_.titleB, 255);
    ui_.drawRect(x, y + TOOLBAR_H - 1, w, 1, theme_.borderR, theme_.borderG, theme_.borderB, 255);

    // Window dots (decorative)
    ui_.drawCircle(x + 18, y + TOOLBAR_H / 2, 5, 255, 95, 86, 255);
    ui_.drawCircle(x + 38, y + TOOLBAR_H / 2, 5, 255, 189, 46, 255);
    ui_.drawCircle(x + 58, y + TOOLBAR_H / 2, 5, 39, 201, 63, 255);

    // Title
    std::string title = "Forge IDE";
    if (activeFileIndex_ >= 0) {
        title += " - " + openFiles_[activeFileIndex_].name +
                 (openFiles_[activeFileIndex_].modified ? " [modified]" : "");
    }
    ui_.drawText(x + 80, y + (TOOLBAR_H - CH_H) / 2, title,
                 theme_.fgR, theme_.fgG, theme_.fgB, 255);

    // Action buttons right-aligned (immediate-mode: draw + hit-test together)
    struct BtnDef { const char* label; int id; };
    static const BtnDef defs[] = {
        {"Build", 4}, {"Run", 3}, {"Save", 2}, {"New", 1},
    };
    int bx = x + w - 10;
    for (auto& d : defs) {
        int bw = (int)strlen(d.label) * CH_W + 20;
        bx -= bw;
        int by = y + 6, bh = TOOLBAR_H - 13;
        bool hover = mouseX_ >= bx && mouseX_ < bx + bw && mouseY_ >= by && mouseY_ < by + bh;

        bool primary = d.id == 3; // Run
        if (hover) {
            ui_.drawRoundedRect(bx, by, bw, bh, 4, 70, 70, 80, 255);
        } else {
            ui_.drawRoundedRect(bx, by, bw, bh, 4, 58, 58, 66, 255);
        }
        if (primary) {
            ui_.drawRectOutline(bx, by, bw, bh, 76, 175, 80, 200);
        }
        uint8_t fr = primary ? (hover ? (uint8_t)140 : (uint8_t)120) : theme_.fgR;
        uint8_t fg2 = primary ? (hover ? (uint8_t)240 : (uint8_t)230) : theme_.fgG;
        uint8_t fb = primary ? (hover ? (uint8_t)140 : (uint8_t)120) : theme_.fgB;
        if (!primary) { fr = theme_.fgR; fg2 = theme_.fgG; fb = theme_.fgB; }
        ui_.drawText(bx + 10, by + (bh - CH_H) / 2, d.label, fr, fg2, fb, 255);

        if (hover && mousePressed_) {
            if (d.id == 1) { // New
                IDEFile f;
                f.name = "untitled-" + std::to_string(openFiles_.size() + 1) + ".fge";
                f.lines = {""};
                openFiles_.push_back(f);
                activeFileIndex_ = (int)openFiles_.size() - 1;
                editorFocused_ = true; terminalFocused_ = false;
            } else if (d.id == 2) { // Save
                saveActiveFile();
            } else if (d.id == 3 && activeFileIndex_ >= 0) { // Run
                saveActiveFile();
                if (!openFiles_[activeFileIndex_].path.empty())
                    runCommand("forge " + openFiles_[activeFileIndex_].path);
                else
                    appendOutput("[error] save the file first (Ctrl+Shift+S to save-as via terminal)\n");
            } else if (d.id == 4 && activeFileIndex_ >= 0) { // Build
                runCommand("forgevm compile " + openFiles_[activeFileIndex_].path);
            }
            mousePressed_ = false; // consume click so it doesn't fall through
        }
    }
}

// ============================================================
// Tabs
// ============================================================

void ForgeIDE::renderTabs(int x, int y, int w) {
    ui_.drawRect(x, y, w, TAB_H, theme_.tabBarR, theme_.tabBarG, theme_.tabBarB, 255);
    tabGeom_.clear();
    termTabGeom_.clear();
    int tx = x;
    for (int i = 0; i < (int)openFiles_.size(); i++) {
        int tw = (int)openFiles_[i].name.size() * CH_W + 40;
        if (tx + tw > x + w) break;
        bool active = (i == activeFileIndex_);

        if (active) {
            ui_.drawRect(tx, y + 1, tw, TAB_H - 1, theme_.tabActiveR, theme_.tabActiveG, theme_.tabActiveB, 255);
            ui_.drawRect(tx, y + 1, tw, 2, theme_.accentR, theme_.accentG, theme_.accentB, 255);
        } else {
            bool hov = mouseX_ >= tx && mouseX_ < tx + tw && mouseY_ >= y && mouseY_ < y + TAB_H;
            ui_.drawRect(tx, y + 1, tw, TAB_H - 1,
                         hov ? (uint8_t)(theme_.tabInactiveR + 8) : theme_.tabInactiveR,
                         hov ? (uint8_t)(theme_.tabInactiveG + 8) : theme_.tabInactiveG,
                         hov ? (uint8_t)(theme_.tabInactiveB + 8) : theme_.tabInactiveB, 255);
        }

        std::string label = openFiles_[i].name;
        ui_.drawText(tx + 8, y + (TAB_H - CH_H) / 2, label,
                     active ? theme_.fgR : theme_.lineNumR,
                     active ? theme_.fgG : theme_.lineNumG,
                     active ? theme_.fgB : theme_.lineNumB, 255);

        // Close affordance: x for hover/active tab, dot when modified
        int cxr = tx + tw - 16;
        int cyr = y + (TAB_H - CH_H) / 2;
        bool xHover = mouseX_ >= cxr - 2 && mouseX_ < cxr + 10 && mouseY_ >= y + 4 && mouseY_ < y + TAB_H - 4;
        if (openFiles_[i].modified && !(active || xHover)) {
            ui_.drawCircle(tx + tw - 11, y + TAB_H / 2, 3, theme_.fgR, theme_.fgG, theme_.fgB, 255);
        } else {
            uint8_t xc = xHover ? 255 : theme_.lineNumR;
            uint8_t xg = xHover ? 255 : theme_.lineNumG;
            uint8_t xb = xHover ? 255 : theme_.lineNumB;
            ui_.drawText(cxr, cyr, "x", xc, xg, xb, 255);
        }

        // store geometry for click handling
        tabGeom_.push_back({tx, tw});
        tx += tw;
    }
}

void ForgeIDE::handleTabClick(int mx, int my, int x, int y, int w) {
    (void)my; (void)w;
    int tx = x;
    size_t gi = 0;
    for (int i = 0; i < (int)openFiles_.size(); i++) {
        if (gi >= tabGeom_.size()) break;
        int tw = tabGeom_[gi].second;
        if (mx >= tx && mx <= tx + tw) {
            // close hitbox?
            int cxr = tx + tw - 18;
            if (mx >= cxr) {
                closeFile(i);
            } else {
                activeFileIndex_ = i;
                editorFocused_ = true; terminalFocused_ = false; filterFocused_ = false;
            }
            return;
        }
        tx += tw;
        gi++;
    }
}

// ============================================================
// Sidebar (explorer / settings)
// ============================================================

static void fileIconColor(const std::string& name,
                          uint8_t& r, uint8_t& g, uint8_t& b) {
    auto endsWith = [&](const char* s) {
        size_t n = strlen(s), l = name.size();
        return l >= n && name.compare(l - n, n, s) == 0;
    };
    if (endsWith(".fge") || endsWith(".forge")) { r = 250; g = 179; b = 135; }
    else if (endsWith(".md")) { r = 137; g = 180; b = 250; }
    else if (endsWith(".json")) { r = 220; g = 220; b = 170; }
    else if (endsWith(".fclass")) { r = 203; g = 166; b = 247; }
    else if (endsWith(".hpp") || endsWith(".h") || endsWith(".cpp") || endsWith(".c")) { r = 137; g = 202; b = 244; }
    else { r = 140; g = 140; b = 145; }
}

void ForgeIDE::renderSidebar(int x, int y, int w, int h) {
    ui_.drawRect(x, y, w, h, theme_.panelR, theme_.panelG, theme_.panelB, 255);
    ui_.drawRect(x + w - 1, y, 1, h, theme_.borderR, theme_.borderG, theme_.borderB, 255);

    if (sideView_ == SideView::Settings) {
        ui_.drawText(x + 10, y + 8, "SETTINGS", theme_.lineNumR, theme_.lineNumG, theme_.lineNumB, 255);
        ui_.drawRect(x + 10, y + 22, w - 20, 1, theme_.borderR, theme_.borderG, theme_.borderB, 255);
        const char* names[3] = {"VS Code Dark+", "Forge Night", "Light+"};
        for (int i = 0; i < 3; i++) {
            int ry = y + 36 + i * 26;
            bool sel = selectedTheme_ == i;
            bool hov = mouseX_ >= x + 6 && mouseX_ < x + w - 6 && mouseY_ >= ry - 3 && mouseY_ < ry + 15;
            if (hov || sel)
                ui_.drawRect(x + 6, ry - 3, w - 12, 18, theme_.selR, theme_.selG, theme_.selB,
                             hov ? (uint8_t)160 : (uint8_t)70);
            // swatch
            IDETheme tmp;
            if (i == 0) tmp = themeVscodeDark();
            else if (i == 1) tmp = themeForgeNight();
            else tmp = themeLight();
            ui_.drawRect(x + 12, ry, 12, 10, tmp.bgR, tmp.bgG, tmp.bgB, 255);
            ui_.drawRectOutline(x + 12, ry, 12, 10, tmp.accentR, tmp.accentG, tmp.accentB, 255);
            ui_.drawText(x + 32, ry, names[i],
                         sel ? theme_.fgR : theme_.fileR,
                         sel ? theme_.fgG : theme_.fileG,
                         sel ? theme_.fgB : theme_.fileB, 255);
            if (hov && mousePressed_) {
                applyTheme(i);
                mousePressed_ = false;
            }
        }
        return;
    }

    // EXPLORER header
    ui_.drawText(x + 10, y + 6, "EXPLORER", theme_.lineNumR, theme_.lineNumG, theme_.lineNumB, 255);

    // Filter input box
    int fx = x + 8, fy = y + 20, fw = w - 16, fh = 20;
    bool filterHover = mouseX_ >= fx && mouseX_ < fx + fw && mouseY_ >= fy && mouseY_ < fy + fh;
    ui_.drawRoundedRect(fx, fy, fw, fh, 4,
                        filterFocused_ ? (uint8_t)(theme_.panelR + 14) : (uint8_t)(theme_.panelR + 8),
                        filterFocused_ ? (uint8_t)(theme_.panelG + 14) : (uint8_t)(theme_.panelG + 8),
                        filterFocused_ ? (uint8_t)(theme_.panelB + 14) : (uint8_t)(theme_.panelB + 8), 255);
    if (filterFocused_)
        ui_.drawRectOutline(fx, fy, fw, fh, theme_.accentR, theme_.accentG, theme_.accentB, 255);
    std::string shown = filterInput_.empty() && !filterFocused_ ? "filter files..." : filterInput_;
    uint8_t pr = filterInput_.empty() && !filterFocused_ ? theme_.lineNumR : theme_.fgR;
    uint8_t pg = filterInput_.empty() && !filterFocused_ ? theme_.lineNumG : theme_.fgG;
    uint8_t pb = filterInput_.empty() && !filterFocused_ ? theme_.lineNumB : theme_.fgB;
    ui_.drawText(fx + 6, fy + 6, shown, pr, pg, pb, 255);
    if (filterFocused_) {
        uint32_t t = ui_.ticks();
        if ((t / 500) % 2 == 0)
            ui_.drawRect(fx + 6 + (int)filterInput_.size() * CH_W, fy + 5, 1, 11,
                         theme_.fgR, theme_.fgG, theme_.fgB, 255);
    }
    if (filterHover && mousePressed_) {
        filterFocused_ = true; editorFocused_ = false; terminalFocused_ = false;
        mousePressed_ = false;
    }

    // Tree (with scroll + filter)
    int ey = fy + fh + 6;
    int lineH = 18;
    int maxVisible = (y + h - ey) / lineH;
    int visibleCount = 0;
    int skipped = 0;
    std::string needle = filterInput_;

    // Flatten visible entries (respecting expansion + filter)
    std::function<void(const std::vector<IDEFileEntry>&)> walk =
        [&](const std::vector<IDEFileEntry>& entries) {
            for (auto& entry : entries) {
                if (visibleCount >= maxVisible) return;
                if (!needle.empty() && !entry.isDir) {
                    // filtered: show only matching files (and their ancestor dirs are skipped too)
                    if (!subsequenceMatch(needle, entry.name)) continue;
                }
                if (skipped < explorerScroll_) { skipped++; continue; }
                visibleCount++;

                int indent = entry.depth * 12;
                bool rowHover = mouseX_ >= x && mouseX_ < x + w && mouseY_ >= ey && mouseY_ < ey + lineH;
                if (rowHover)
                    ui_.drawRect(x, ey, w, lineH, theme_.selR, theme_.selG, theme_.selB, 70);

                // chevron for dirs
                int ix = x + 8 + indent;
                if (entry.isDir) {
                    // triangle
                    if (entry.expanded) {
                        for (int k = 0; k < 5; k++)
                            ui_.drawRect(ix + k, ey + 6 + k - 0, 2, 1, theme_.dirR, theme_.dirG, theme_.dirB, 255);
                        for (int k = 0; k < 5; k++)
                            ui_.drawRect(ix + k, ey + 10 - k, 2, 1, theme_.dirR, theme_.dirG, theme_.dirB, 255);
                    } else {
                        for (int k = 0; k < 5; k++)
                            ui_.drawRect(ix + k, ey + 5 + k, 1, 2, theme_.dirR, theme_.dirG, theme_.dirB, 255);
                    }
                    ui_.drawText(ix + 10, ey + 4, entry.name,
                                 theme_.dirR, theme_.dirG, theme_.dirB, 255);
                } else {
                    uint8_t ir, ig, ib;
                    fileIconColor(entry.name, ir, ig, ib);
                    ui_.drawRect(ix, ey + 5, 5, 9, ir, ig, ib, 255);
                    ui_.drawText(ix + 10, ey + 4, entry.name,
                                 theme_.fileR, theme_.fileG, theme_.fileB, 255);
                }
                entryRowMap_.push_back({ey, entry.fullPath, entry.isDir});
                ey += lineH;
                if (entry.isDir && entry.expanded && needle.empty())
                    walk(entry.children);
            }
        };
    entryRowMap_.clear();
    walk(fileTree_);

    // Scroll indicator
    if (explorerScroll_ > 0) {
        int trackH = y + h - (fy + fh + 6);
        float frac = std::min(1.0f, (float)maxVisible / (float)(maxVisible + explorerScroll_));
        ui_.drawRect(x + w - 3, fy + fh + 6, 2, std::max(20, (int)(trackH * frac)),
                     theme_.lineNumR, theme_.lineNumG, theme_.lineNumB, 100);
    }
}

void ForgeIDE::handleSidebarClick(int mx, int my, int x, int y, int w, int h) {
    (void)y; (void)h;
    // Settings view rows handled in render; here only tree rows:
    if (sideView_ != SideView::Explorer) return;
    // Don't steal clicks from the filter box (handled in render)
    if (my < TOOLBAR_H + 46) return;

    for (auto& row : entryRowMap_) {
        if (my >= row.y && my < row.y + 18) {
            if (row.isDir) {
                // toggle in tree
                std::function<void(std::vector<IDEFileEntry>&)> toggle =
                    [&](std::vector<IDEFileEntry>& entries) -> void {
                    for (auto& e : entries) {
                        if (e.fullPath == row.path) {
                            e.expanded = !e.expanded;
                            if (e.expanded) buildFileTree(e.fullPath, e.children, e.depth + 1);
                            return;
                        }
                        if (e.expanded) toggle(e.children);
                    }
                };
                toggle(fileTree_);
            } else {
                openFile(row.path);
            }
            return;
        }
    }
    (void)mx;
}

// ============================================================
// Editor
// ============================================================

void ForgeIDE::renderEditor(int x, int y, int w, int h) {
    ui_.drawRect(x, y, w, h, theme_.bgR, theme_.bgG, theme_.bgB, 255);
    if (openFiles_.empty() || activeFileIndex_ < 0) return;
    auto& file = openFiles_[activeFileIndex_];

    int minimapX = x + w - MINIMAP_W - 8;
    int textW = w - GUTTER_W - MINIMAP_W - 12;

    // Current-line band (full editor width behind text)
    int curLineY = y + (file.cursorRow - file.scrollY) * LINE_H;
    if (file.cursorRow >= file.scrollY && file.cursorRow < file.scrollY + h / LINE_H + 1) {
        ui_.drawRect(x + GUTTER_W, curLineY, textW + 4, LINE_H,
                     theme_.curLineR, theme_.curLineG, theme_.curLineB, theme_.curLineA);
    }

    // Gutter
    ui_.drawRect(x, y, GUTTER_W, h, theme_.panelR, theme_.panelG, theme_.panelB, 255);
    ui_.drawRect(x + GUTTER_W - 1, y, 1, h, theme_.borderR, theme_.borderG, theme_.borderB, 255);

    // Bracket match (computed once per frame)
    int bmRow = -1, bmCol = -1;
    if (editorFocused_) findMatchingBracket(bmRow, bmCol);

    int visibleLines = h / LINE_H + 1;
    for (int i = 0; i < visibleLines; i++) {
        int lineIdx = file.scrollY + i;
        if (lineIdx >= (int)file.lines.size()) break;
        int ly = y + i * LINE_H;
        const std::string& line = file.lines[lineIdx];

        // Line number: active line is bright
        bool isActiveLine = lineIdx == file.cursorRow;
        std::string numStr = std::to_string(lineIdx + 1);
        while (numStr.size() < 4) numStr = " " + numStr;
        ui_.drawText(x + 10, ly + 3, numStr,
                     isActiveLine ? theme_.lineNumActiveR : theme_.lineNumR,
                     isActiveLine ? theme_.lineNumActiveG : theme_.lineNumG,
                     isActiveLine ? theme_.lineNumActiveB : theme_.lineNumB, 255);

        // Bracket highlight background
        if (bmRow == lineIdx) {
            ui_.drawRect(x + GUTTER_W + 2 + bmCol * CH_W, ly, CH_W, LINE_H,
                         theme_.accentR, theme_.accentG, theme_.accentB, 60);
            ui_.drawRectOutline(x + GUTTER_W + 2 + bmCol * CH_W, ly, CH_W, LINE_H,
                                theme_.accentR, theme_.accentG, theme_.accentB, 160);
        }

        // Syntax-highlighted text (clipped to text area)
        auto colors = highlightLine(line);
        int maxCols = textW / CH_W;
        int tx = x + GUTTER_W + 2;
        int drawCols = std::min((int)line.size(), maxCols);
        for (int c = 0; c < drawCols; c++) {
            std::string chStr(1, line[c]);
            ui_.drawText(tx, ly + 3, chStr, colors[c].r, colors[c].g, colors[c].b, 255);
            tx += CH_W;
        }
    }

    // Cursor (blinking bar)
    if (editorFocused_ && file.cursorRow >= file.scrollY &&
        file.cursorRow < file.scrollY + visibleLines) {
        uint32_t t = ui_.ticks();
        if ((t / 500) % 2 == 0) {
            int cx = x + GUTTER_W + 2 + file.cursorCol * CH_W;
            int cy2 = y + (file.cursorRow - file.scrollY) * LINE_H;
            ui_.drawRect(cx, cy2 + 1, 2, LINE_H - 2, theme_.fgR, theme_.fgG, theme_.fgB, 220);
            // subtle accent underline at cursor position
            ui_.drawRect(cx - 1, cy2 + LINE_H - 2, 4, 1, theme_.accentR, theme_.accentG, theme_.accentB, 255);
        }
    }

    renderMinimap(minimapX, y, MINIMAP_W, h, file);

    // Editor scrollbar
    int totalLines = (int)file.lines.size();
    if (totalLines > visibleLines - 1) {
        float frac = std::min(1.0f, (float)visibleLines / (float)totalLines);
        int trackH = h;
        int thumbH = std::max(24, (int)(trackH * frac));
        int thumbY = y + (int)((float)(trackH - thumbH) *
                               ((float)file.scrollY / (float)(totalLines - visibleLines + 1)));
        ui_.drawRect(x + w - 4, thumbY, 3, thumbH, theme_.lineNumR, theme_.lineNumG, theme_.lineNumB, 90);
    }
}

void ForgeIDE::renderMinimap(int x, int y, int w, int h, IDEFile& file) {
    if (!openFiles_.empty()) {
        ui_.drawRect(x - 4, y, 1, h, theme_.borderR, theme_.borderG, theme_.borderB, 255);
        int pitch = 3;   // pixels per line
        int barMaxW = w - 6;
        int totalLines = (int)file.lines.size();
        int visibleLines = h / pitch;
        float scrollFrac = totalLines > visibleLines
            ? (float)file.scrollY / (float)(totalLines - visibleLines + 1) : 0.0f;
        int startLine = (int)(scrollFrac * std::max(0, totalLines - visibleLines));

        for (int i = 0; i < visibleLines && startLine + i < totalLines; i++) {
            const std::string& line = file.lines[startLine + i];
            size_t first = line.find_first_not_of(" \t");
            if (first == std::string::npos) continue;
            int bw = std::min(barMaxW, (int)(line.size() - first));
            if (bw <= 0) continue;
            int indentPx = std::min(20, (int)first);
            ui_.drawRect(x + 4 + indentPx, y + i * pitch, bw, 2,
                         theme_.minimapR, theme_.minimapG, theme_.minimapB, 130);
        }

        // Viewport indicator
        int visLines = (h - (y ? 0 : 0)) ;
        (void)visLines;
        int editorVisible = std::max(1, h / LINE_H);
        int vpH = std::max(10, (int)((float)editorVisible / std::max(1, visibleLines) * h));
        vpH = std::min(vpH, h);
        int vpY = y + (int)(scrollFrac * (h - vpH));
        ui_.drawRect(x + 2, vpY, w - 2, vpH, theme_.accentR, theme_.accentG, theme_.accentB, 26);

        // Click-to-jump
        if (mousePressed_ && mouseX_ >= x && mouseX_ < x + w && mouseY_ >= y && mouseY_ < y + h) {
            float rel = (float)(mouseY_ - y) / (float)h;
            int target = (int)(rel * totalLines - editorVisible / 2.0f);
            file.scrollY = std::max(0, std::min(target, std::max(0, totalLines - editorVisible)));
            mousePressed_ = false;
        }
    }
}

std::vector<ForgeIDE::SyntaxColor> ForgeIDE::highlightLine(const std::string& line) {
    std::vector<SyntaxColor> colors(line.size());
    size_t i = 0;
    auto setRange = [&](size_t from, size_t to, uint8_t r, uint8_t g, uint8_t b) {
        for (size_t j = from; j < to && j < colors.size(); j++) colors[j] = {r, g, b};
    };
    static const char* controlKws[] = {
        "if", "else", "while", "for", "return", "break", "continue",
        "try", "catch", "finally", "throw", "yield", "in", "match", "case"
    };
    static const char* declKws[] = {
        "fn", "let", "const", "var", "val", "class", "struct", "impl",
        "trait", "enum", "import", "from", "as", "new", "this", "super",
        "static", "pub", "mod", "use", "gen", "extern", "test"
    };
    auto inList = [](const char** list, size_t n, const std::string& w) {
        for (size_t k = 0; k < n; k++) if (w == list[k]) return true;
        return false;
    };

    while (i < line.size()) {
        if (line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/') {
            setRange(i, line.size(), theme_.commentR, theme_.commentG, theme_.commentB);
            break;
        }
        if (line[i] == '"' || line[i] == '\'') {
            char q = line[i];
            size_t start = i;
            i++;
            while (i < line.size() && line[i] != q) {
                if (line[i] == '\\' && i + 1 < line.size()) i++;
                i++;
            }
            if (i < line.size()) i++;
            setRange(start, i, theme_.stringR, theme_.stringG, theme_.stringB);
            continue;
        }
        if (std::isdigit((unsigned char)line[i]) ||
            (line[i] == '.' && i + 1 < line.size() && std::isdigit((unsigned char)line[i + 1]))) {
            size_t start = i;
            while (i < line.size() && (std::isdigit((unsigned char)line[i]) || line[i] == '.' ||
                   toupper((unsigned char)line[i]) == 'X' || isxdigit((unsigned char)line[i])))
                i++;
            setRange(start, i, theme_.numberR, theme_.numberG, theme_.numberB);
            continue;
        }
        if (std::isalpha((unsigned char)line[i]) || line[i] == '_') {
            size_t start = i;
            while (i < line.size() && (std::isalnum((unsigned char)line[i]) || line[i] == '_')) i++;
            std::string word = line.substr(start, i - start);
            if (inList(controlKws, sizeof(controlKws) / sizeof(char*), word))
                setRange(start, i, theme_.controlR, theme_.controlG, theme_.controlB);
            else if (inList(declKws, sizeof(declKws) / sizeof(char*), word))
                setRange(start, i, theme_.keywordR, theme_.keywordG, theme_.keywordB);
            else if (i < line.size() && line[i] == '(')
                setRange(start, i, theme_.funcR, theme_.funcG, theme_.funcB);
            else if (!word.empty() && (word[0] >= 'A' && word[0] <= 'Z'))
                setRange(start, i, theme_.typeR, theme_.typeG, typeBlueGuard());
            else
                setRange(start, i, theme_.fgR, theme_.fgG, theme_.fgB);
            continue;
        }
        colors[i] = {theme_.fgR, theme_.fgG, theme_.fgB};
        i++;
    }
    return colors;
}

uint8_t ForgeIDE::typeBlueGuard() { return theme_.typeB; }

bool ForgeIDE::findMatchingBracket(int& outRow, int& outCol) {
    if (activeFileIndex_ < 0) return false;
    auto& file = openFiles_[activeFileIndex_];
    auto isBracket = [](char c) {
        return c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}';
    };
    // Check char under cursor, then char before it
    int checkRow = file.cursorRow, checkCol = file.cursorCol;
    char c = 0;
    if (checkRow < (int)file.lines.size()) {
        const std::string& l = file.lines[checkRow];
        if (checkCol < (int)l.size()) c = l[checkCol];
        if (!isBracket(c) && checkCol > 0) { c = l[checkCol - 1]; checkCol--; }
        if (!isBracket(c)) return false;
    } else return false;

    char open, close;
    int dir;
    switch (c) {
        case '(': open = '('; close = ')'; dir = 1; break;
        case '[': open = '['; close = ']'; dir = 1; break;
        case '{': open = '{'; close = '}'; dir = 1; break;
        case ')': open = '('; close = ')'; dir = -1; break;
        case ']': open = '['; close = ']'; dir = -1; break;
        case '}': open = '{'; close = '}'; dir = -1; break;
        default: return false;
    }

    int depth = 0;
    int row = checkRow;
    int col = checkCol;
    while (row >= 0 && row < (int)file.lines.size()) {
        const std::string& l = file.lines[row];
        while (col >= 0 && col < (int)l.size()) {
            char lc = l[col];
            if (lc == open) depth += (dir == 1 ? 1 : -1);
            else if (lc == close) depth += (dir == 1 ? -1 : 1);
            if (depth == 0 && (lc == open || lc == close) &&
                !(row == checkRow && col == checkCol)) {
                outRow = row; outCol = col;
                return true;
            }
            col += dir;
            if (col < 0) break;
        }
        row += dir;
        col = dir == 1 ? 0 : INT_MAX;
        if (row < (int)file.lines.size() && col == INT_MAX)
            col = (int)file.lines[row].size() - 1;
        if (dir == -1 && col < 0) { row++; break; }
    }
    return false;
}

void ForgeIDE::ensureCursorVisible() { scrollToCursor(); }

void ForgeIDE::scrollToCursor() {
    if (activeFileIndex_ < 0) return;
    auto& file = openFiles_[activeFileIndex_];
    int ww, wh; ui_.getSize(ww, wh);
    int edH = wh - TOOLBAR_H - TAB_H - STATUS_H - TERMINAL_H;
    int visible = edH / LINE_H;
    if (visible <= 2) return;
    if (file.cursorRow < file.scrollY + 1)
        file.scrollY = std::max(0, file.cursorRow - 1);
    else if (file.cursorRow > file.scrollY + visible - 2)
        file.scrollY = file.cursorRow - visible + 2;
}

// ============================================================
// Editor input
// ============================================================

static bool isOpenBracket(char c) { return c=='('||c=='['||c=='{'; }
static bool isCloseBracket(char c) { return c==')'||c==']'||c=='}'; }
static char matchingClose(char c) {
    switch(c){case '(':return ')';case '[':return ']';case '{':return '}';}
    return c;
}

void ForgeIDE::handleEditorInput(int key, char ch, bool shift, bool ctrl) {
    if (activeFileIndex_ < 0) return;
    auto& file = openFiles_[activeFileIndex_];
    std::string& line = file.lines[file.cursorRow];

    auto cursorMoved = [&]() { scrollToCursor(); };

    // Ctrl+S handled globally; other shortcuts:
    if (ctrl && key == SDLK_c) {
        if (file.cursorRow < (int)file.lines.size())
            SDL_SetClipboardText(line.c_str());
        return;
    }
    if (ctrl && key == SDLK_v) {
        const char* clip = SDL_GetClipboardText();
        if (clip) {
            std::string text(clip);
            if (!text.empty() && text.back() == '\n') text.pop_back();
            for (char pc : text) {
                if (pc == '\n') {
                    std::string rest = line.substr(file.cursorCol);
                    line = line.substr(0, file.cursorCol);
                    file.lines.insert(file.lines.begin() + file.cursorRow + 1, rest);
                    file.cursorRow++;
                    file.cursorCol = 0;
                    line = file.lines[file.cursorRow];
                } else {
                    line.insert(file.cursorCol, 1, pc);
                    file.cursorCol++;
                }
            }
            file.modified = true;
            cursorMoved();
        }
        return;
    }
    if (ctrl && key == SDLK_x) {
        SDL_SetClipboardText(line.c_str());
        line.clear();
        file.cursorCol = 0;
        file.modified = true;
        return;
    }
    if (ctrl) return; // let other ctrl combos fall through untouched

    // Navigation
    if (key == SDLK_LEFT || key == SDLK_KP_4) {
        if (file.cursorCol > 0) file.cursorCol--;
        else if (file.cursorRow > 0) {
            file.cursorRow--;
            file.cursorCol = (int)file.lines[file.cursorRow].size();
        }
        file.desiredCol = file.cursorCol;
        cursorMoved(); return;
    }
    if (key == SDLK_RIGHT || key == SDLK_KP_6) {
        if (file.cursorCol < (int)line.size()) file.cursorCol++;
        else if (file.cursorRow < (int)file.lines.size() - 1) {
            file.cursorRow++;
            file.cursorCol = 0;
        }
        file.desiredCol = file.cursorCol;
        cursorMoved(); return;
    }
    if (key == SDLK_UP || key == SDLK_KP_8) {
        if (file.cursorRow > 0) {
            file.cursorRow--;
            int want = file.desiredCol >= 0 ? file.desiredCol : file.cursorCol;
            file.cursorCol = std::min(want, (int)file.lines[file.cursorRow].size());
        }
        cursorMoved(); return;
    }
    if (key == SDLK_DOWN || key == SDLK_KP_2) {
        if (file.cursorRow < (int)file.lines.size() - 1) {
            file.cursorRow++;
            int want = file.desiredCol >= 0 ? file.desiredCol : file.cursorCol;
            file.cursorCol = std::min(want, (int)file.lines[file.cursorRow].size());
        }
        cursorMoved(); return;
    }
    if (key == SDLK_PAGEUP) {
        int ww, wh; ui_.getSize(ww, wh);
        int page = std::max(1, (wh - TOOLBAR_H - TAB_H - STATUS_H - TERMINAL_H) / LINE_H - 2);
        file.cursorRow = std::max(0, file.cursorRow - page);
        file.cursorCol = std::min(file.cursorCol, (int)file.lines[file.cursorRow].size());
        cursorMoved(); return;
    }
    if (key == SDLK_PAGEDOWN) {
        int ww, wh; ui_.getSize(ww, wh);
        int page = std::max(1, (wh - TOOLBAR_H - TAB_H - STATUS_H - TERMINAL_H) / LINE_H - 2);
        file.cursorRow = std::min((int)file.lines.size() - 1, file.cursorRow + page);
        file.cursorCol = std::min(file.cursorCol, (int)file.lines[file.cursorRow].size());
        cursorMoved(); return;
    }
    if (key == SDLK_HOME) {
        // smart home: toggle between column 0 and first non-space
        size_t firstNonSpace = line.find_first_not_of(" \t");
        file.cursorCol = (firstNonSpace != std::string::npos && file.cursorCol != (int)firstNonSpace)
            ? (int)firstNonSpace : 0;
        file.desiredCol = file.cursorCol;
        cursorMoved(); return;
    }
    if (key == SDLK_END) {
        file.cursorCol = (int)line.size();
        file.desiredCol = file.cursorCol;
        cursorMoved(); return;
    }

    // Enter with auto-indent (+ brace expansion)
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        std::string beforeCursor = line.substr(0, file.cursorCol);
        std::string rest = line.substr(file.cursorCol);

        // leading whitespace of current line
        size_t firstNonSpace = beforeCursor.find_first_not_of(" \t");
        std::string indent = firstNonSpace == std::string::npos
            ? beforeCursor : beforeCursor.substr(0, firstNonSpace);

        bool opensBlock = !beforeCursor.empty() && beforeCursor.back() == '{';
        bool closesNext = !rest.empty() && rest[0] == '}';

        if (opensBlock && closesNext) {
            // expand { | } into three lines
            file.lines[file.cursorRow] = beforeCursor;
            file.lines.insert(file.lines.begin() + file.cursorRow + 1, indent + "    ");
            file.lines.insert(file.lines.begin() + file.cursorRow + 2, indent + rest);
            file.cursorRow++;
            file.cursorCol = (int)(indent.size() + 4);
        } else {
            if (opensBlock) indent += "    ";
            file.lines[file.cursorRow] = beforeCursor;
            file.lines.insert(file.lines.begin() + file.cursorRow + 1, indent + rest);
            file.cursorRow++;
            file.cursorCol = (int)indent.size();
        }
        file.modified = true;
        cursorMoved(); return;
    }

    // Tab inserts spaces
    if (key == SDLK_TAB) {
        line.insert(file.cursorCol, "    ");
        file.cursorCol += 4;
        file.modified = true; return;
    }

    // Backspace with pair deletion
    if (key == SDLK_BACKSPACE) {
        if (file.cursorCol > 0) {
            char prev = line[file.cursorCol - 1];
            char next = file.cursorCol < (int)line.size() ? line[file.cursorCol] : 0;
            bool pair = (prev == '(' && next == ')') || (prev == '[' && next == ']') ||
                        (prev == '{' && next == '}') || (prev == '"' && next == '"') ||
                        (prev == '\'' && next == '\'');
            if (pair) line.erase(file.cursorCol, 1);   // remove the partner too
            line.erase(file.cursorCol - 1, 1);
            file.cursorCol--;
        } else if (file.cursorRow > 0) {
            // join lines, preserving indentation of this line
            std::string trimmed = line;
            size_t fn = trimmed.find_first_not_of(" \t");
            std::string indentPart = fn == std::string::npos ? "" : trimmed.substr(0, fn);
            std::string content = fn == std::string::npos ? "" : trimmed.substr(fn);
            file.cursorCol = (int)(file.lines[file.cursorRow - 1].size() + indentPart.size());
            file.lines[file.cursorRow - 1] += indentPart + content;
            file.lines.erase(file.lines.begin() + file.cursorRow);
            file.cursorRow--;
        }
        file.modified = true;
        cursorMoved(); return;
    }

    if (key == SDLK_DELETE) {
        if (file.cursorCol < (int)line.size()) {
            line.erase(file.cursorCol, 1);
        } else if (file.cursorRow < (int)file.lines.size() - 1) {
            line += file.lines[file.cursorRow + 1];
            file.lines.erase(file.lines.begin() + file.cursorRow + 1);
        }
        file.modified = true; return;
    }

    // Printable characters
    if (ch >= 32 && ch < 127) {
        // Skip over a matching closer if we just typed the opener
        if ((ch == '(' || ch == '[' || ch == '{')) {
            line.insert(file.cursorCol, 1, ch);
            line.insert(file.cursorCol + 1, 1, matchingClose(ch));
            file.modified = true; return;   // cursor stays between the pair
        }
        if ((ch == ')' || ch == ']' || ch == '}') &&
            file.cursorCol < (int)line.size() && line[file.cursorCol] == ch) {
            file.cursorCol++;               // step over existing closer
            return;
        }
        if (ch == '"' || ch == '\'') {
            if (file.cursorCol < (int)line.size() && line[file.cursorCol] == ch) {
                file.cursorCol++;           // step over existing quote
                return;
            }
            line.insert(file.cursorCol, 1, ch);
            line.insert(file.cursorCol + 1, 1, ch);
            file.modified = true; return;   // cursor between quotes
        }
        line.insert(file.cursorCol, 1, ch);
        file.cursorCol++;
        file.modified = true; return;
    }
}

void ForgeIDE::handleEditorClick(int mx, int my, int x, int y, int w, int h) {
    (void)w; (void)h;
    if (activeFileIndex_ < 0) return;
    auto& file = openFiles_[activeFileIndex_];

    int row = (my - y) / LINE_H + file.scrollY;
    int col = (mx - x - GUTTER_W - 2) / CH_W;

    if (row >= 0 && row < (int)file.lines.size()) {
        file.cursorRow = row;
        file.cursorCol = std::max(0, std::min(col, (int)file.lines[row].size()));
        file.desiredCol = file.cursorCol;
    } else if (row >= (int)file.lines.size()) {
        // click below last line: move to end of document
        file.cursorRow = (int)file.lines.size() - 1;
        file.cursorCol = (int)file.lines[file.cursorRow].size();
    }
}

// ============================================================
// Command palette (Ctrl+P quick-open)
// ============================================================

void ForgeIDE::updatePalette() {
    paletteResults_.clear();
    for (auto& path : allFiles_) {
        PaletteItem item;
        item.path = path;
        item.label = fs::path(path).filename().string();
        fs::path parent = fs::path(path).parent_path();
        std::error_code ec;
        fs::path rel = fs::relative(parent, currentDir_, ec);
        item.detail = ec ? parent.string() : rel.string();
        int score = 0;
        if (subsequenceMatch(paletteInput_, item.label, &score) ||
            subsequenceMatch(paletteInput_, path, &score)) {
            item.score = score;
            paletteResults_.push_back(item);
        }
    }
    std::sort(paletteResults_.begin(), paletteResults_.end(),
              [](const PaletteItem& a, const PaletteItem& b) { return a.score > b.score; });
    if ((int)paletteResults_.size() > 12) paletteResults_.resize(12);
    if (paletteSelected_ >= (int)paletteResults_.size()) paletteSelected_ = std::max(0, (int)paletteResults_.size() - 1);
}

void ForgeIDE::handlePaletteKey(int key, char ch) {
    if (key == SDLK_ESCAPE) { paletteOpen_ = false; return; }
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        if (paletteSelected_ >= 0 && paletteSelected_ < (int)paletteResults_.size()) {
            openFile(paletteResults_[paletteSelected_].path);
            paletteOpen_ = false;
        }
        return;
    }
    if (key == SDLK_UP) {
        if (paletteSelected_ > 0) paletteSelected_--;
        return;
    }
    if (key == SDLK_DOWN) {
        if (paletteSelected_ < (int)paletteResults_.size() - 1) paletteSelected_++;
        return;
    }
    if (key == SDLK_BACKSPACE) {
        if (!paletteInput_.empty()) paletteInput_.pop_back();
        updatePalette();
        return;
    }
    if (ch >= 32 && ch < 127) {
        paletteInput_ += ch;
        paletteSelected_ = 0;
        updatePalette();
    }
}

void ForgeIDE::renderPalette() {
    int ww, wh; ui_.getSize(ww, wh);
    // dim overlay
    ui_.drawRect(0, 0, ww, wh, 0, 0, 0, 110);

    int pw = std::min(560, ww - 80);
    int px = (ww - pw) / 2;
    int py = 64;
    int rowH = 22;
    int listRows = std::min(10, (int)paletteResults_.size());
    int ph = 40 + listRows * rowH + 6;

    ui_.drawShadow(px, py, pw, ph, 14, 120);
    ui_.drawRoundedRect(px, py, pw, ph, 8, theme_.panelR, theme_.panelG, theme_.panelB, 252);
    ui_.drawRoundedRectOutline(px, py, pw, ph, 8, theme_.borderR, theme_.borderG, theme_.borderB, 255);

    // Input row
    ui_.drawText(px + 12, py + 10, "> ", theme_.accentR, theme_.accentG, theme_.accentB, 255);
    ui_.drawText(px + 30, py + 10,
                 paletteInput_.empty() ? "type a file name..." : paletteInput_,
                 paletteInput_.empty() ? theme_.lineNumR : theme_.fgR,
                 paletteInput_.empty() ? theme_.lineNumG : theme_.fgG,
                 paletteInput_.empty() ? theme_.lineNumB : theme_.fgB, 255);
    uint32_t t = ui_.ticks();
    if ((t / 500) % 2 == 0)
        ui_.drawRect(px + 30 + (int)paletteInput_.size() * CH_W, py + 9, 1, 11,
                     theme_.fgR, theme_.fgG, theme_.fgB, 255);
    ui_.drawRect(px + 8, py + 28, pw - 16, 1, theme_.borderR, theme_.borderG, theme_.borderB, 255);

    for (int i = 0; i < (int)paletteResults_.size(); i++) {
        int ry = py + 34 + i * rowH;
        bool sel = i == paletteSelected_;
        if (sel) {
            ui_.drawRoundedRect(px + 6, ry - 3, pw - 12, rowH - 2, 4,
                                theme_.selR, theme_.selG, theme_.selB, 200);
            ui_.drawRect(px + 6, ry - 3, 3, rowH - 2, theme_.accentR, theme_.accentG, theme_.accentB, 255);
        }
        uint8_t ir, ig, ib;
        fileIconColor(paletteResults_[i].label, ir, ig, ib);
        ui_.drawRect(px + 14, ry + 2, 5, 9, ir, ig, ib, 255);
        ui_.drawText(px + 26, ry, paletteResults_[i].label,
                     sel ? theme_.fgR : theme_.fileR,
                     sel ? theme_.fgG : theme_.fileG,
                     sel ? theme_.fgB : theme_.fileB, 255);
        std::string detail = paletteResults_[i].detail;
        if (detail == ".") detail = "";
        int dw = (int)detail.size() * CH_W;
        ui_.drawText(px + pw - dw - 12, ry, detail,
                     theme_.lineNumR, theme_.lineNumG, theme_.lineNumB, 255);
    }
}

// ============================================================
// Terminal
// ============================================================

void ForgeIDE::renderTerminal(int x, int y, int w, int h) {
    ui_.drawRect(x, y, w, h, theme_.panelR, theme_.panelG, theme_.panelB, 255);
    ui_.drawRect(x, y, w, 1, theme_.borderR, theme_.borderG, theme_.borderB, 255);

    // Panel tabs: TERMINAL | OUTPUT
    const char* tabNames[] = {"TERMINAL", "OUTPUT"};
    int tx = x + 10;
    for (int i = 0; i < 2; i++) {
        int tw = (int)strlen(tabNames[i]) * CH_W + 16;
        bool active = (i == (int)termTab_);
        if (active) {
            ui_.drawText(tx + 8, y + 7, tabNames[i], theme_.fgR, theme_.fgG, theme_.fgB, 255);
            ui_.drawRect(tx + 4, y + 19, tw - 8, 2, theme_.accentR, theme_.accentG, theme_.accentB, 255);
        } else {
            ui_.drawText(tx + 8, y + 7, tabNames[i],
                         theme_.lineNumR, theme_.lineNumG, theme_.lineNumB, 255);
        }
        termTabGeom_.push_back({tx, tw});
        tx += tw + 8;
    }

    int contentTop = y + 26;
    int inputH = 20;
    int contentH = h - 26 - inputH;
    int lineH = 13;

    const auto& lines = outputLines_;
    int visible = contentH / lineH;
    int total = (int)lines.size();
    int start = std::max(0, total - visible - terminalScroll_);

    for (int i = 0; i < visible && start + i < total; i++) {
        ui_.drawText(x + 10, contentTop + i * lineH, lines[start + i],
                     theme_.fgR, theme_.fgG, theme_.fgB, 255);
    }

    // Scrollbar hint
    if (terminalScroll_ > 0) {
        ui_.drawRect(x + w - 3, contentTop + 2, 2,
                     std::max(16, (int)((float)visible / std::max(1, total) * contentH)),
                     theme_.lineNumR, theme_.lineNumG, theme_.lineNumB, 100);
    }

    // Prompt row
    int iy = y + h - inputH;
    ui_.drawRect(x, iy, w, inputH, theme_.activityR, theme_.activityG, theme_.activityB, 255);
    std::string prompt = "> " + terminalInput_;
    ui_.drawText(x + 10, iy + 6, prompt, theme_.accentR, theme_.accentG, theme_.accentB, 255);
    if (terminalFocused_) {
        uint32_t t = ui_.ticks();
        if ((t / 500) % 2 == 0)
            ui_.drawRect(x + 10 + (int)prompt.size() * CH_W, iy + 5, 1, 11,
                         theme_.accentR, theme_.accentG, theme_.accentB, 255);
    }
}

void ForgeIDE::handleTerminalClick(int mx, int my, int x, int y) {
    (void)x; (void)y; (void)mx;
    // Tab switching via recorded geometry
    for (size_t i = 0; i < termTabGeom_.size(); i++) {
        if (mx >= termTabGeom_[i].first && mx <= termTabGeom_[i].first + termTabGeom_[i].second &&
            my >= y && my <= y + 22) {
            termTab_ = (TermTab)i;
            terminalFocused_ = true;
            editorFocused_ = false;
            filterFocused_ = false;
            return;
        }
    }
    terminalFocused_ = true;
    editorFocused_ = false;
    filterFocused_ = false;
}

void ForgeIDE::handleTerminalInput(int key, char ch) {
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        if (!terminalInput_.empty()) {
            runCommand(terminalInput_);
            terminalInput_.clear();
        }
        return;
    }
    if (key == SDLK_BACKSPACE) {
        if (!terminalInput_.empty()) terminalInput_.pop_back();
        return;
    }
    if (key == SDLK_UP || key == SDLK_DOWN) {
        // simple history navigation
        static std::vector<std::string> history;
        static int histPos = -1;
        if (key == SDLK_UP) {
            if (history.empty() || history.back() != terminalInput_)
                if (!terminalInput_.empty()) { history.push_back(terminalInput_); }
            if (!history.empty()) {
                if (histPos < 0) histPos = (int)history.size() - 1;
                else if (histPos > 0) histPos--;
                terminalInput_ = history[histPos];
            }
        } else {
            if (histPos >= 0 && histPos < (int)history.size() - 1) {
                histPos++;
                terminalInput_ = history[histPos];
            } else {
                histPos = -1;
                terminalInput_.clear();
            }
        }
        return;
    }
    if (ch >= 32 && ch < 127) {
        terminalInput_ += ch;
    }
}

void ForgeIDE::appendOutput(const std::string& text) {
    // strip ANSI escape sequences so the bitmap font never renders garbage
    std::string clean;
    clean.reserve(text.size());
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\033') {
            while (i < text.size() && text[i] != 'm' && text[i] != 'H' && text[i] != 'J') i++;
            continue;
        }
        clean += text[i];
    }
    terminalOutput_ += clean;
    std::istringstream stream(clean);
    std::string ln;
    while (std::getline(stream, ln)) outputLines_.push_back(ln);
    if (!clean.empty() && clean.back() != '\n' && !outputLines_.empty())
        outputLines_.back() += ""; // partial line stays merged visually
    terminalScroll_ = 0; // snap to bottom on new output
}

void ForgeIDE::runCommand(const std::string& cmd) {
    appendOutput("$ " + cmd + "\n");
    fflush(nullptr);
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe) {
        appendOutput("Error: failed to execute command\n");
        return;
    }
    char buf[512];
    size_t n;
    std::string out;
    while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0) out.append(buf, n);
    int status = pclose(pipe);
    appendOutput(out);
    if (!out.empty() && out.back() != '\n') appendOutput("\n");
    if (status != 0)
        appendOutput("[exit code: " + std::to_string(status) + "]\n");
}

// ============================================================
// Status bar
// ============================================================

void ForgeIDE::renderStatusBar(int x, int y, int w) {
    ui_.drawGradientV(x, y, w, STATUS_H, theme_.statusR, theme_.statusG, theme_.statusB,
                      (uint8_t)(theme_.statusR * 70 / 100),
                      (uint8_t)(theme_.statusG * 70 / 100),
                      (uint8_t)(theme_.statusB * 70 / 100), 255);

    // Left side: branch + no-issues badge
    std::string branch = "main";
    std::ifstream head(".git/HEAD");
    if (head.is_open()) {
        std::string ref;
        std::getline(head, ref);
        const std::string prefix = "ref: refs/heads/";
        if (ref.compare(0, prefix.size(), prefix) == 0) branch = ref.substr(prefix.size());
    }
    ui_.drawText(x + 8, y + 8, ("  " + branch).c_str(),
                 theme_.statusFgR, theme_.statusFgG, theme_.statusFgB, 255);
    ui_.drawCircle(x + 11, y + STATUS_H / 2, 3,
                   theme_.statusFgR, theme_.statusFgG, theme_.statusFgB, 255);
    ui_.drawText(x + 90, y + 8, "(0 errors, 0 warnings)",
                 theme_.statusFgR, theme_.statusFgG, theme_.statusFgB, 180);

    if (activeFileIndex_ >= 0) {
        auto& f = openFiles_[activeFileIndex_];
        std::string right =
            "Ln " + std::to_string(f.cursorRow + 1) +
            ", Col " + std::to_string(f.cursorCol + 1) +
            "    Spaces: 4    Forge    UTF-8";
        int rw = (int)right.size() * CH_W;
        ui_.drawText(x + w - rw - 10, y + 8, right,
                     theme_.statusFgR, theme_.statusFgG, theme_.statusFgB, 230);
        // language chip
        int chipW = 44;
        ui_.drawRect(x + w - rw - chipW - 22, y + 4, chipW, STATUS_H - 8,
                     255, 255, 255, 36);
        ui_.drawText(x + w - rw - chipW - 18, y + 8, "FORGE",
                     theme_.statusFgR, theme_.statusFgG, theme_.statusFgB, 255);
    }
}

// ============================================================
// Welcome screen
// ============================================================

void ForgeIDE::renderWelcome(int x, int y, int w, int h) {
    // Big FORGE wordmark (bold effect via multi-pass draw)
    const std::string logo = "FORGE";
    const int scale = 6;
    int logoW = (int)logo.size() * CH_W * scale;
    int lx = x + (w - logoW) / 2;
    int ly = y + h / 4;
    for (int dx = 0; dx < scale; dx++)
        for (int dy = 0; dy < scale; dy++)
            ui_.drawText(lx + dx, ly + dy, logo,
                         theme_.accentR, theme_.accentG, theme_.accentB, 255);

    std::string tagline = "the forge vm ide  --  write. compile. run.";
    int tagW = (int)tagline.size() * CH_W;
    ui_.drawText(x + (w - tagW) / 2, ly + CH_H * scale + 14, tagline,
                 theme_.lineNumR, theme_.lineNumG, theme_.lineNumB, 255);

    // Shortcuts card
    struct Hint { const char* keys; const char* desc; };
    static const Hint hints[] = {
        {"Ctrl+P", "quick open any file"},
        {"Ctrl+N", "new file"},
        {"Ctrl+S", "save"},
        {"Ctrl+W", "close tab"},
        {"F5",     "run active file"},
    };
    int hy = ly + CH_H * scale + 48;
    for (auto& hint : hints) {
        int kw = (int)strlen(hint.keys) * CH_W;
        int hx = x + w / 2 - 140;
        ui_.drawRoundedRect(hx, hy - 3, kw + 14, 15, 4,
                            theme_.panelR + 10 > 255 ? 255 : theme_.panelR + 10,
                            theme_.panelG + 10 > 255 ? 255 : theme_.panelG + 10,
                            theme_.panelB + 12 > 255 ? 255 : theme_.panelB + 12, 255);
        ui_.drawText(hx + 7, hy, hint.keys, theme_.fgR, theme_.fgG, theme_.fgB, 255);
        ui_.drawText(hx + kw + 34, hy, hint.desc,
                     theme_.lineNumR, theme_.lineNumG, theme_.lineNumB, 255);
        hy += 24;
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

void ForgeIDE::syncContent(IDEFile& f) {
    f.content.clear();
    for (size_t i = 0; i < f.lines.size(); i++) {
        f.content += f.lines[i];
        if (i + 1 < f.lines.size()) f.content += "\n";
    }
}

void ForgeIDE::saveActiveFile() {
    if (activeFileIndex_ < 0) return;
    auto& f = openFiles_[activeFileIndex_];
    syncContent(f);
    if (!f.path.empty()) {
        writeFileContent(f.path, f.content);
        f.modified = false;
        appendOutput("[saved] " + f.path + "\n");
    } else {
        appendOutput("[save] unsaved buffers need a path — use the terminal: not supported yet\n");
    }
}

void ForgeIDE::openFile(const std::string& path) {
    for (int i = 0; i < (int)openFiles_.size(); i++) {
        if (openFiles_[i].path == path) {
            activeFileIndex_ = i;
            editorFocused_ = true; terminalFocused_ = false; filterFocused_ = false;
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
    openFiles_.push_back(f);
    activeFileIndex_ = (int)openFiles_.size() - 1;
    editorFocused_ = true; terminalFocused_ = false; filterFocused_ = false;
}

void ForgeIDE::closeFile(int index) {
    if (index < 0 || index >= (int)openFiles_.size()) return;
    openFiles_.erase(openFiles_.begin() + index);
    if (activeFileIndex_ >= (int)openFiles_.size())
        activeFileIndex_ = (int)openFiles_.size() - 1;
    editorFocused_ = true;
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
