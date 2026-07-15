#pragma once

#include "runtime.hpp"
#include <string>
#include <vector>
#include <cstdint>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace forge::fvm {

// ============================================================
// SDL2UI - C++ SDL2 rendering + input backend for the FVM.
//
// WHY SDL2?
//   The FVM is a portable bytecode virtual machine, exactly
//   like the JVM: it loads and executes Forge bytecode
//   (.fclass) the same way the JVM loads and executes a .jar.
//   Just as a .jar runs unchanged on Windows, Linux/X11,
//   Wayland or macOS, a compiled Forge program must open a
//   window unchanged on every one of those platforms.
//
//   SDL2 is the FVM's cross-platform windowing/UI layer -- the
//   analogue of AWT/Swing for the JVM. It abstracts the native
//   windowing system, input and rendering, so a single C++ UI
//   implementation produces a working window on any OS / window
//   server without per-platform code. The FVM itself stays
//   platform-agnostic; SDL2 is the only portability boundary.
//
// This is the native (C++) UI layer for the Forge Virtual
// Machine. It owns an SDL2 window/renderer and exposes an
// immediate-mode style drawing API plus an event pump.
// It is registered into the FVM as the `sdl` native module
// so Forge programs can also drive it, but the implementation
// lives entirely in C++ (like the JVM's native AWT/UI layer).
// ============================================================

struct SDLKeyEvent {
    int keycode = 0;     // SDL_Keycode
    int scancode = 0;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    char ch = 0;         // printable character if any
};

struct SDLMouseEvent {
    int x = 0, y = 0;
    int button = 0;      // 1 left, 2 middle, 3 right
    int wheel = 0;       // wheel delta (signed)
    bool pressed = false;
    bool released = false;
    bool motion = false;
};

struct SDLWindowEvent {
    int width = 0, height = 0;
    bool resized = false;
    bool closed = false;
};

class SDL2UI {
public:
    SDL2UI();
    ~SDL2UI();

    // Window management
    bool init(int width, int height, const std::string& title);
    void shutdown();
    bool isOpen() const { return open_; }

    // Frame management
    void clear(uint8_t r, uint8_t g, uint8_t b);
    void present();
    void delay(uint32_t ms);
    uint32_t ticks() const;

    // 2D primitives ------------------------------------------------
    void drawRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void drawRectOutline(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void drawLine(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void drawCircle(int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void drawText(int x, int y, const std::string& text,
                  uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void drawTextBG(int x, int y, const std::string& text,
                    uint8_t r, uint8_t g, uint8_t b,
                    uint8_t br, uint8_t bg, uint8_t bb, uint8_t ba = 255);

    // Extended primitives for IDE-style UI -------------------------
    void drawRoundedRect(int x, int y, int w, int h, int radius,
                         uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void drawRoundedRectOutline(int x, int y, int w, int h, int radius,
                                uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void drawGradientV(int x, int y, int w, int h,
                       uint8_t r1, uint8_t g1, uint8_t b1,
                       uint8_t r2, uint8_t g2, uint8_t b2, uint8_t a = 255);
    void drawShadow(int x, int y, int w, int h, int radius, uint8_t a = 60);
    void drawTab(int x, int y, int w, int h, const std::string& label,
                 bool active, uint8_t r, uint8_t g, uint8_t b);

    // Sizing helpers
    int textWidth(const std::string& text) const { return (int)text.size() * CHAR_W; }
    int textHeight() const { return CHAR_H; }
    void getSize(int& w, int& h) const { w = width_; h = height_; }

    // Event pump (C++ side). Returns false when window closed.
    bool pumpEvents(SDLKeyEvent* key, SDLMouseEvent* mouse, SDLWindowEvent* win);

    // Immediate-mode button helper (pure C++, no Forge needed)
    bool button(int x, int y, int w, int h, const std::string& label,
                uint8_t r, uint8_t g, uint8_t b,
                const SDLMouseEvent& mouse, bool hover);

    // Access to raw SDL handles (for advanced native use)
    SDL_Renderer* renderer() { return renderer_; }

    // Capture the current frame to a BMP file (so the UI can be
    // shown/screenshotted without a live display attached).
    bool saveScreenshot(const std::string& path);

    static SDL2UI& instance();

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    int width_ = 800, height_ = 600;
    bool open_ = false;

    static constexpr int CHAR_W = 8;
    static constexpr int CHAR_H = 8;

    void drawChar(int x, int y, char c, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
};

// Register the `sdl` native module into the VM (call from defineModules()).
void defineSDL2Module(ForgeVM& vm);

// Launch the FVM's SDL2 windowed UI. Packaged together with the FVM
// (invoked via `forgevm --sdl` / `forge --sdl`) so the VM and its
// cross-platform UI ship as one deliverable, like a .jar with its JVM.
// If `screenshotPath` is set, render one frame and save it, then exit
// (used to show the running window headlessly).
int runSdlGui(const std::string& screenshotPath = "");

} // namespace forge::fvm
