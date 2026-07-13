#pragma once

#include <string>
#include <vector>
#include <functional>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

namespace ansi {

// Colors
const std::string RESET     = "\033[0m";
const std::string BOLD      = "\033[1m";
const std::string DIM       = "\033[2m";
const std::string ITALIC    = "\033[3m";
const std::string UNDERLINE = "\033[4m";
const std::string BLINK     = "\033[5m";
const std::string REVERSE   = "\033[7m";
const std::string HIDDEN    = "\033[8m";
const std::string STRIKETHROUGH = "\033[9m";

// Foreground 16-color
const std::string FG_BLACK   = "\033[30m";
const std::string FG_RED     = "\033[31m";
const std::string FG_GREEN   = "\033[32m";
const std::string FG_YELLOW  = "\033[33m";
const std::string FG_BLUE    = "\033[34m";
const std::string FG_MAGENTA = "\033[35m";
const std::string FG_CYAN    = "\033[36m";
const std::string FG_WHITE   = "\033[37m";
const std::string FG_GRAY    = "\033[90m";

// Bold foreground
const std::string FG_BOLD_BLACK   = "\033[1;30m";
const std::string FG_BOLD_RED     = "\033[1;31m";
const std::string FG_BOLD_GREEN   = "\033[1;32m";
const std::string FG_BOLD_YELLOW  = "\033[1;33m";
const std::string FG_BOLD_BLUE    = "\033[1;34m";
const std::string FG_BOLD_MAGENTA = "\033[1;35m";
const std::string FG_BOLD_CYAN    = "\033[1;36m";
const std::string FG_BOLD_WHITE   = "\033[1;37m";

// Background 16-color
const std::string BG_BLACK   = "\033[40m";
const std::string BG_RED     = "\033[41m";
const std::string BG_GREEN   = "\033[42m";
const std::string BG_YELLOW  = "\033[43m";
const std::string BG_BLUE    = "\033[44m";
const std::string BG_MAGENTA = "\033[45m";
const std::string BG_CYAN    = "\033[46m";
const std::string BG_WHITE   = "\033[47m";
const std::string BG_GRAY    = "\033[100m";

// 256-color helpers
inline std::string fg256(int n) { return "\033[38;5;" + std::to_string(n) + "m"; }
inline std::string bg256(int n) { return "\033[48;5;" + std::to_string(n) + "m"; }

// Truecolor (24-bit) helpers
inline std::string fgRGB(int r, int g, int b) { return "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m"; }
inline std::string bgRGB(int r, int g, int b) { return "\033[48;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m"; }

// Cursor movement
inline std::string move(int row, int col) {
    return "\033[" + std::to_string(row) + ";" + std::to_string(col) + "H";
}
inline std::string moveToRow(int row) { return "\033[" + std::to_string(row) + "H"; }
inline std::string moveUp(int n = 1) { return "\033[" + std::to_string(n) + "A"; }
inline std::string moveDown(int n = 1) { return "\033[" + std::to_string(n) + "B"; }
inline std::string moveRight(int n = 1) { return "\033[" + std::to_string(n) + "C"; }
inline std::string moveLeft(int n = 1) { return "\033[" + std::to_string(n) + "D"; }

// Screen control
inline std::string clearScreen() { return "\033[2J"; }
inline std::string clearLine() { return "\033[2K"; }
inline std::string clearToEnd() { return "\033[K"; }
inline std::string hideCursor() { return "\033[?25l"; }
inline std::string showCursor() { return "\033[?25h"; }
inline std::string saveCursor() { return "\033[s"; }
inline std::string restoreCursor() { return "\033[u"; }
inline std::string alternateScreen() { return "\033[?1049h"; }
inline std::string mainScreen() { return "\033[?1049l"; }
inline std::string enableMouse() { return "\033[?1003h\033[?1006h"; }
inline std::string disableMouse() { return "\033[?1003l\033[?1006l"; }

// Box drawing
namespace box {
    const std::string TL = "\u250C";  // ┌
    const std::string TR = "\u2510";  // ┐
    const std::string BL = "\u2514";  // └
    const std::string BR = "\u2518";  // ┘
    const std::string H  = "\u2500";  // ─
    const std::string V  = "\u2502";  // │
    const std::string T  = "\u252C";  // ┬
    const std::string B  = "\u2534";  // ┴
    const std::string L  = "\u251C";  // ├
    const std::string R  = "\u2524";  // ┤
    const std::string X  = "\u253C";  // ┼
    const std::string LR = "\u2500";  // ─ (horizontal)
    const std::string TB = "\u2502";  // │ (vertical)
}

} // namespace ansi

// Key codes
enum Key {
    KEY_NONE = 0,
    KEY_ENTER = 10,
    KEY_TAB = 9,
    KEY_BACKSPACE = 127,
    KEY_ESCAPE = 27,
    KEY_UP = 1000,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_HOME,
    KEY_END,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_DELETE,
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_MOUSE,
    KEY_CTRL_A = 1,
    KEY_CTRL_B = 2,
    KEY_CTRL_C = 3,
    KEY_CTRL_D = 4,
    KEY_CTRL_E = 5,
    KEY_CTRL_F = 6,
    KEY_CTRL_G = 7,
    KEY_CTRL_H = 8,
    KEY_CTRL_I = 9,
    KEY_CTRL_J = 10,
    KEY_CTRL_K = 11,
    KEY_CTRL_L = 12,
    KEY_CTRL_M = 13,
    KEY_CTRL_N = 14,
    KEY_CTRL_O = 15,
    KEY_CTRL_P = 16,
    KEY_CTRL_Q = 17,
    KEY_CTRL_R = 18,
    KEY_CTRL_S = 19,
    KEY_CTRL_T = 20,
    KEY_CTRL_U = 21,
    KEY_CTRL_V = 22,
    KEY_CTRL_W = 23,
    KEY_CTRL_X = 24,
    KEY_CTRL_Y = 25,
    KEY_CTRL_Z = 26,
};

struct TerminalSize {
    int cols;
    int rows;
};

class Terminal {
public:
    Terminal();
    ~Terminal();

    void enableRawMode();
    void disableRawMode();
    void enterAlternateScreen();
    void exitAlternateScreen();
    void enableMouse();
    void disableMouse();

    int readKey();
    std::string readSequence();
    TerminalSize getSize();

    void write(const std::string& s);
    void write(const char* s);
    void flush();

    static Terminal& instance();

    struct WinSize { int cols; int rows; };
    static volatile sig_atomic_t resizeFlag;
    static void handleResize(int sig);

private:
    struct termios origTermios_;
    bool rawMode_ = false;
    int stdinFd_ = 0;
    int stdoutFd_ = 1;
};
