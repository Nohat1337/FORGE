#include "terminal.hpp"
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>

volatile sig_atomic_t Terminal::resizeFlag = 0;

void Terminal::handleResize(int sig) {
    (void)sig;
    resizeFlag = 1;
}

Terminal& Terminal::instance() {
    static Terminal t;
    return t;
}

Terminal::Terminal() {
    stdinFd_ = STDIN_FILENO;
    stdoutFd_ = STDOUT_FILENO;
    signal(SIGWINCH, handleResize);
}

Terminal::~Terminal() {
    if (rawMode_) disableRawMode();
}

void Terminal::enableRawMode() {
    if (rawMode_) return;
    tcgetattr(stdinFd_, &origTermios_);
    struct termios raw = origTermios_;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(stdinFd_, TCSAFLUSH, &raw);
    rawMode_ = true;
}

void Terminal::disableRawMode() {
    if (!rawMode_) return;
    tcsetattr(stdinFd_, TCSAFLUSH, &origTermios_);
    rawMode_ = false;
}

void Terminal::enterAlternateScreen() {
    write(ansi::alternateScreen());
    write(ansi::hideCursor());
    flush();
}

void Terminal::exitAlternateScreen() {
    write(ansi::showCursor());
    write(ansi::mainScreen());
    flush();
}

void Terminal::enableMouse() {
    write(ansi::enableMouse());
    flush();
}

void Terminal::disableMouse() {
    write(ansi::disableMouse());
    flush();
}

int Terminal::readKey() {
    char c;
    int nread = read(stdinFd_, &c, 1);
    if (nread <= 0) return KEY_NONE;

    if (c == '\x1b') {
        char seq[6];
        nread = read(stdinFd_, &seq[0], 1);
        if (nread <= 0) return KEY_ESCAPE;
        nread = read(stdinFd_, &seq[1], 1);
        if (nread <= 0) return KEY_ESCAPE;

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                nread = read(stdinFd_, &seq[2], 1);
                if (nread <= 0) return KEY_ESCAPE;
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return KEY_HOME;
                        case '3': return KEY_DELETE;
                        case '4': return KEY_END;
                        case '5': return KEY_PAGE_UP;
                        case '6': return KEY_PAGE_DOWN;
                        case '7': return KEY_HOME;
                        case '8': return KEY_END;
                    }
                }
                if (seq[2] == ';') {
                    // CSI u ; ~ format (e.g. modified keys)
                    read(stdinFd_, &seq[3], 1);
                    read(stdinFd_, &seq[4], 1);
                    if (seq[4] == '~') {
                        switch (seq[3]) {
                            case '1': return KEY_F1;
                            case '2': return KEY_F2;
                            case '3': return KEY_F3;
                            case '4': return KEY_F4;
                            case '5': return KEY_F5;
                            case '6': return KEY_F6;
                            case '7': return KEY_F7;
                            case '8': return KEY_F8;
                            case '9': return KEY_F9;
                        }
                    }
                }
                // SGR mouse: \x1b[<row;col;M or m
                if (seq[1] == '<') {
                    // consume until M or m
                    char buf[32];
                    int i = 0;
                    while (i < 31) {
                        char ch;
                        if (read(stdinFd_, &ch, 1) != 1) break;
                        buf[i++] = ch;
                        if (ch == 'M' || ch == 'm') break;
                    }
                    buf[i] = '\0';
                    return KEY_MOUSE;
                }
            } else if (seq[1] == 'O') {
                // SS3 sequence (F1-F4)
                nread = read(stdinFd_, &seq[2], 1);
                switch (seq[2]) {
                    case 'P': return KEY_F1;
                    case 'Q': return KEY_F2;
                    case 'R': return KEY_F3;
                    case 'S': return KEY_F4;
                }
            } else {
                switch (seq[1]) {
                    case 'A': return KEY_UP;
                    case 'B': return KEY_DOWN;
                    case 'C': return KEY_RIGHT;
                    case 'D': return KEY_LEFT;
                    case 'H': return KEY_HOME;
                    case 'F': return KEY_END;
                    case 'Z': return KEY_NONE; // shift-tab placeholder
                }
            }
        } else if (seq[0] == 'O') {
            nread = read(stdinFd_, &seq[1], 1);
            switch (seq[1]) {
                case 'P': return KEY_F1;
                case 'Q': return KEY_F2;
                case 'R': return KEY_F3;
                case 'S': return KEY_F4;
            }
        }
        return KEY_ESCAPE;
    }
    return c;
}

TerminalSize Terminal::getSize() {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        return {80, 24};
    }
    return {ws.ws_col, ws.ws_row};
}

void Terminal::write(const std::string& s) {
    ::write(stdoutFd_, s.data(), s.size());
}

void Terminal::write(const char* s) {
    ::write(stdoutFd_, s, strlen(s));
}

void Terminal::flush() {
    fsync(stdoutFd_);
}
