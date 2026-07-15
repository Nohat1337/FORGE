#include "sdl2_ui.hpp"
#include <SDL2/SDL.h>
#include <cstring>
#include <vector>
#include <iostream>

namespace forge::fvm {

// ============================================================
// Built-in 8x8 bitmap font (public-domain "font8x8_basic",
// codes 32..126). One byte per row, MSB = left-most pixel.
// ============================================================
static const uint8_t FONT8x8[95][8] = {
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 32 space
  {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // 33 !
  {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // 34 "
  {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, // 35 #
  {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, // 36 $
  {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, // 37 %
  {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, // 38 &
  {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, // 39 '
  {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, // 40 (
  {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, // 41 )
  {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // 42 *
  {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, // 43 +
  {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // 44 ,
  {0x00,0x00,0x0C,0x0C,0x0C,0x0C,0x0C,0x00}, // 45 -
  {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // 46 .
  {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, // 47 /
  {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, // 48 0
  {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, // 49 1
  {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, // 50 2
  {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, // 51 3
  {0x38,0x3C,0x36,0x33,0x7F,0x30,0x30,0x00}, // 52 4
  {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, // 53 5
  {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, // 54 6
  {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, // 55 7
  {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, // 56 8
  {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, // 57 9
  {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, // 58 :
  {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30}, // 59 ;
  {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // 60 <
  {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, // 61 =
  {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, // 62 >
  {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, // 63 ?
  {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, // 64 @
  {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, // 65 A
  {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, // 66 B
  {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, // 67 C
  {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, // 68 D
  {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, // 69 E
  {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, // 70 F
  {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, // 71 G
  {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, // 72 H
  {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 73 I
  {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, // 74 J
  {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, // 75 K
  {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, // 76 L
  {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, // 77 M
  {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, // 78 N
  {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, // 79 O
  {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, // 80 P
  {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, // 81 Q
  {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, // 82 R
  {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, // 83 S
  {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 84 T
  {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, // 85 U
  {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, // 86 V
  {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // 87 W
  {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, // 88 X
  {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, // 89 Y
  {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, // 90 Z
  {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00}, // 91 [
  {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, // 92 backslash
  {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, // 93 ]
  {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, // 94 ^
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // 95 _
  {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, // 96 `
  {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, // 97 a
  {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, // 98 b
  {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, // 99 c
  {0x38,0x30,0x30,0x3e,0x33,0x33,0x6E,0x00}, // 100 d
  {0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00}, // 101 e
  {0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00}, // 102 f
  {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, // 103 g
  {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, // 104 h
  {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, // 105 i
  {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, // 106 j
  {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, // 107 k
  {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 108 l
  {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, // 109 m
  {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, // 110 n
  {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, // 111 o
  {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, // 112 p
  {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, // 113 q
  {0x00,0x00,0x3B,0x6E,0x06,0x06,0x0F,0x00}, // 114 r
  {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, // 115 s
  {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, // 116 t
  {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, // 117 u
  {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, // 118 v
  {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, // 119 w
  {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, // 120 x
  {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, // 121 y
  {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, // 122 z
  {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, // 123 {
  {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // 124 |
  {0x07,0x30,0x30,0x38,0x30,0x30,0x07,0x00}, // 125 }
  {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, // 126 ~
};

// ============================================================
// SDL2UI implementation
// ============================================================

SDL2UI::SDL2UI() = default;

SDL2UI::~SDL2UI() { shutdown(); }

SDL2UI& SDL2UI::instance() {
    static SDL2UI ui;
    return ui;
}

bool SDL2UI::init(int width, int height, const std::string& title) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }
    width_ = width;
    height_ = height;
    window_ = SDL_CreateWindow(title.c_str(),
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window_) { shutdown(); return false; }

    // Load window icon from assets/icon.bmp
    SDL_Surface* icon = SDL_LoadBMP("assets/icon.bmp");
    if (!icon) icon = SDL_LoadBMP("../assets/icon.bmp");
    if (!icon) icon = SDL_LoadBMP("/usr/local/share/forge/icon.bmp");
    if (icon) {
        SDL_SetWindowIcon(window_, icon);
        SDL_FreeSurface(icon);
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer_) {
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer_) { shutdown(); return false; }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    open_ = true;
    return true;
}

void SDL2UI::shutdown() {
    if (renderer_) { SDL_DestroyRenderer(renderer_); renderer_ = nullptr; }
    if (window_) { SDL_DestroyWindow(window_); window_ = nullptr; }
    if (open_) { SDL_Quit(); open_ = false; }
}

void SDL2UI::clear(uint8_t r, uint8_t g, uint8_t b) {
    if (!renderer_) return;
    SDL_SetRenderDrawColor(renderer_, r, g, b, 255);
    SDL_RenderClear(renderer_);
}

void SDL2UI::present() {
    if (renderer_) SDL_RenderPresent(renderer_);
}

void SDL2UI::delay(uint32_t ms) { SDL_Delay(ms); }
uint32_t SDL2UI::ticks() const { return SDL_GetTicks(); }

void SDL2UI::drawRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!renderer_) return;
    SDL_SetRenderDrawColor(renderer_, r, g, b, a);
    SDL_Rect rect{ x, y, w, h };
    SDL_RenderFillRect(renderer_, &rect);
}

void SDL2UI::drawRectOutline(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!renderer_) return;
    SDL_SetRenderDrawColor(renderer_, r, g, b, a);
    SDL_Rect rect{ x, y, w, h };
    SDL_RenderDrawRect(renderer_, &rect);
}

void SDL2UI::drawLine(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!renderer_) return;
    SDL_SetRenderDrawColor(renderer_, r, g, b, a);
    SDL_RenderDrawLine(renderer_, x0, y0, x1, y1);
}

void SDL2UI::drawCircle(int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!renderer_) return;
    SDL_SetRenderDrawColor(renderer_, r, g, b, a);
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= radius * radius)
                SDL_RenderDrawPoint(renderer_, cx + dx, cy + dy);
        }
    }
}

void SDL2UI::drawChar(int x, int y, char c, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!renderer_) return;
    int idx = (int)c - 32;
    if (idx < 0 || idx >= 95) return;
    const uint8_t* glyph = FONT8x8[idx];
    SDL_SetRenderDrawColor(renderer_, r, g, b, a);
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            // bits are LSB-first (bit 0 = leftmost pixel)
            if (bits & (1 << col))
                SDL_RenderDrawPoint(renderer_, x + col, y + row);
        }
    }
}

void SDL2UI::drawText(int x, int y, const std::string& text,
                      uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int cx = x;
    for (char c : text) {
        if (c == '\n') { cx = x; y += CHAR_H; continue; }
        drawChar(cx, y, c, r, g, b, a);
        cx += CHAR_W;
    }
}

void SDL2UI::drawTextBG(int x, int y, const std::string& text,
                        uint8_t r, uint8_t g, uint8_t b,
                        uint8_t br, uint8_t bg, uint8_t bb, uint8_t ba) {
    int w = (int)text.size() * CHAR_W;
    drawRect(x, y, w, CHAR_H, br, bg, bb, ba);
    drawText(x, y, text, r, g, b, 255);
}

// ============================================================
// Extended primitives for IDE-style UI
// ============================================================

void SDL2UI::drawRoundedRect(int x, int y, int w, int h, int radius,
                             uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!renderer_) return;
    SDL_SetRenderDrawColor(renderer_, r, g, b, a);
    // Fill center rect
    SDL_Rect center{ x + radius, y, w - 2 * radius, h };
    SDL_RenderFillRect(renderer_, &center);
    // Fill left rect
    SDL_Rect left{ x, y + radius, radius, h - 2 * radius };
    SDL_RenderFillRect(renderer_, &left);
    // Fill right rect
    SDL_Rect right{ x + w - radius, y + radius, radius, h - 2 * radius };
    SDL_RenderFillRect(renderer_, &right);
    // Draw four corner circles
    auto fillCircle = [&](int cx, int cy) {
        for (int dy = -radius; dy <= 0; dy++) {
            for (int dx = -radius; dx <= 0; dx++) {
                if (dx * dx + dy * dy <= radius * radius)
                    SDL_RenderDrawPoint(renderer_, cx + dx, cy + dy);
            }
        }
    };
    fillCircle(x + radius, y + radius);
    fillCircle(x + w - radius - 1, y + radius);
    fillCircle(x + radius, y + h - radius - 1);
    fillCircle(x + w - radius - 1, y + h - radius - 1);
}

void SDL2UI::drawRoundedRectOutline(int x, int y, int w, int h, int radius,
                                    uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!renderer_) return;
    SDL_SetRenderDrawColor(renderer_, r, g, b, a);
    // Top and bottom lines
    SDL_RenderDrawLine(renderer_, x + radius, y, x + w - radius - 1, y);
    SDL_RenderDrawLine(renderer_, x + radius, y + h - 1, x + w - radius - 1, y + h - 1);
    // Left and right lines
    SDL_RenderDrawLine(renderer_, x, y + radius, x, y + h - radius - 1);
    SDL_RenderDrawLine(renderer_, x + w - 1, y + radius, x + w - 1, y + h - radius - 1);
    // Four corner arcs
    auto arc = [&](int cx, int cy) {
        for (int dy = -radius; dy <= 0; dy++) {
            for (int dx = -radius; dx <= 0; dx++) {
                int d2 = dx * dx + dy * dy;
                if (d2 <= radius * radius && d2 >= (radius - 1) * (radius - 1))
                    SDL_RenderDrawPoint(renderer_, cx + dx, cy + dy);
            }
        }
    };
    arc(x + radius, y + radius);
    arc(x + w - radius - 1, y + radius);
    arc(x + radius, y + h - radius - 1);
    arc(x + w - radius - 1, y + h - radius - 1);
}

void SDL2UI::drawGradientV(int x, int y, int w, int h,
                           uint8_t r1, uint8_t g1, uint8_t b1,
                           uint8_t r2, uint8_t g2, uint8_t b2, uint8_t a) {
    if (!renderer_ || h <= 0) return;
    for (int row = 0; row < h; row++) {
        float t = (float)row / (float)(h - 1);
        uint8_t r = (uint8_t)(r1 + (r2 - r1) * t);
        uint8_t g = (uint8_t)(g1 + (g2 - g1) * t);
        uint8_t b = (uint8_t)(b1 + (b2 - b1) * t);
        SDL_SetRenderDrawColor(renderer_, r, g, b, a);
        SDL_RenderDrawLine(renderer_, x, y + row, x + w - 1, y + row);
    }
}

void SDL2UI::drawShadow(int x, int y, int w, int h, int radius, uint8_t a) {
    if (!renderer_) return;
    for (int i = 0; i < radius; i++) {
        uint8_t sa = (uint8_t)(a * (radius - i) / radius);
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, sa);
        SDL_Rect r{ x - radius + i, y + i, w + radius * 2 - i * 2, h + radius - i };
        SDL_RenderDrawRect(renderer_, &r);
    }
}

void SDL2UI::drawTab(int x, int y, int w, int h, const std::string& label,
                     bool active, uint8_t r, uint8_t g, uint8_t b) {
    if (!renderer_) return;
    if (active) {
        drawRect(x, y, w, h, r, g, b, 255);
        drawRect(x, y + h - 2, w, 2, 98, 160, 234, 255);
        drawText(x + 8, y + (h - CHAR_H) / 2, label, 255, 255, 255, 255);
    } else {
        drawRect(x, y, w, h, 36, 36, 46, 255);
        drawText(x + 8, y + (h - CHAR_H) / 2, label, 150, 150, 160, 255);
    }
}

bool SDL2UI::pumpEvents(SDLKeyEvent* key, SDLMouseEvent* mouse, SDLWindowEvent* win) {
    if (key) *key = SDLKeyEvent{};
    if (mouse) *mouse = SDLMouseEvent{};
    if (win) *win = SDLWindowEvent{};

    SDL_Event e;
    bool gotKey = false, gotMouse = false, gotWin = false;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                if (win) { win->closed = true; gotWin = true; }
                open_ = false;
                return false;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_RESIZED && win) {
                    width_ = e.window.data1;
                    height_ = e.window.data2;
                    win->width = width_;
                    win->height = height_;
                    win->resized = true;
                    gotWin = true;
                }
                break;
            case SDL_KEYDOWN:
                if (key && !gotKey) {
                    key->keycode = e.key.keysym.sym;
                    key->scancode = e.key.keysym.scancode;
                    key->shift = (e.key.keysym.mod & KMOD_SHIFT) != 0;
                    key->ctrl = (e.key.keysym.mod & KMOD_CTRL) != 0;
                    key->alt = (e.key.keysym.mod & KMOD_ALT) != 0;
                    if (e.key.keysym.sym >= 32 && e.key.keysym.sym < 127)
                        key->ch = (char)e.key.keysym.sym;
                    gotKey = true;
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (mouse && !gotMouse) {
                    mouse->x = e.button.x;
                    mouse->y = e.button.y;
                    mouse->button = e.button.button;
                    mouse->pressed = true;
                    gotMouse = true;
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (mouse && !gotMouse) {
                    mouse->x = e.button.x;
                    mouse->y = e.button.y;
                    mouse->button = e.button.button;
                    mouse->released = true;
                    gotMouse = true;
                }
                break;
            case SDL_MOUSEMOTION:
                if (mouse && !gotMouse) {
                    mouse->x = e.motion.x;
                    mouse->y = e.motion.y;
                    mouse->motion = true;
                    gotMouse = true;
                }
                break;
            case SDL_MOUSEWHEEL:
                if (mouse && !gotMouse) {
                    mouse->wheel = e.wheel.y;
                    gotMouse = true;
                }
                break;
        }
    }
    return true;
}

bool SDL2UI::button(int x, int y, int w, int h, const std::string& label,
                    uint8_t r, uint8_t g, uint8_t b,
                    const SDLMouseEvent& mouse, bool hover) {
    uint8_t br = hover ? 80 : 50, bg = hover ? 90 : 60, bb = hover ? 110 : 80;
    drawRect(x, y, w, h, br, bg, bb, 255);
    drawRectOutline(x, y, w, h, r, g, b, 255);
    int tw = (int)label.size() * CHAR_W;
    drawText(x + (w - tw) / 2, y + (h - CHAR_H) / 2, label, r, g, b, 255);
    bool inside = mouse.x >= x && mouse.x <= x + w && mouse.y >= y && mouse.y <= y + h;
    return inside && mouse.pressed && mouse.button == 1;
}

bool SDL2UI::saveScreenshot(const std::string& path) {
    if (!renderer_) return false;
    int w = 0, h = 0;
    SDL_GetRendererOutputSize(renderer_, &w, &h);
    if (w <= 0 || h <= 0) return false;
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surf) return false;
    int r = SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_RGBA32,
                                  surf->pixels, surf->pitch);
    bool ok = (r == 0);
    if (ok) ok = (SDL_SaveBMP(surf, path.c_str()) == 0);
    SDL_FreeSurface(surf);
    return ok;
}

// ============================================================
// Native `sdl` module exposed to Forge programs
// ============================================================

void defineSDL2Module(ForgeVM& vm) {
    auto* mod = new GCMap();
    SDL2UI& ui = SDL2UI::instance();

    mod->entries["init"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>& args) -> FValue {
            int w = args.size() > 0 ? (int)args[0].asInteger() : 800;
            int h = args.size() > 1 ? (int)args[1].asInteger() : 600;
            std::string title = args.size() > 2 && args[2].isString() ? args[2].asString()->value : "Forge FVM";
            return FValue::boolean(ui.init(w, h, title));
        }, "sdl.init"));

    mod->entries["quit"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>&) -> FValue { ui.shutdown(); return FValue::nil(); },
        "sdl.quit"));

    mod->entries["clear"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>& args) -> FValue {
            uint8_t r = args.size() > 0 ? (uint8_t)args[0].asInteger() : 0;
            uint8_t g = args.size() > 1 ? (uint8_t)args[1].asInteger() : 0;
            uint8_t b = args.size() > 2 ? (uint8_t)args[2].asInteger() : 0;
            ui.clear(r, g, b);
            return FValue::nil();
        }, "sdl.clear"));

    mod->entries["present"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>&) -> FValue { ui.present(); return FValue::nil(); },
        "sdl.present"));

    mod->entries["delay"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>& args) -> FValue {
            ui.delay(args.size() > 0 ? (uint32_t)args[0].asInteger() : 16);
            return FValue::nil();
        }, "sdl.delay"));

    mod->entries["ticks"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>&) -> FValue { return FValue::integer((long long)ui.ticks()); },
        "sdl.ticks"));

    mod->entries["rect"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>& args) -> FValue {
            int x = (int)args[0].asInteger(), y = (int)args[1].asInteger();
            int w = (int)args[2].asInteger(), h = (int)args[3].asInteger();
            uint8_t r = (uint8_t)args[4].asInteger(), g = (uint8_t)args[5].asInteger(), b = (uint8_t)args[6].asInteger();
            uint8_t a = args.size() > 7 ? (uint8_t)args[7].asInteger() : 255;
            ui.drawRect(x, y, w, h, r, g, b, a);
            return FValue::nil();
        }, "sdl.rect"));

    mod->entries["rect_outline"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>& args) -> FValue {
            int x = (int)args[0].asInteger(), y = (int)args[1].asInteger();
            int w = (int)args[2].asInteger(), h = (int)args[3].asInteger();
            uint8_t r = (uint8_t)args[4].asInteger(), g = (uint8_t)args[5].asInteger(), b = (uint8_t)args[6].asInteger();
            ui.drawRectOutline(x, y, w, h, r, g, b, 255);
            return FValue::nil();
        }, "sdl.rect_outline"));

    mod->entries["line"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>& args) -> FValue {
            int x0 = (int)args[0].asInteger(), y0 = (int)args[1].asInteger();
            int x1 = (int)args[2].asInteger(), y1 = (int)args[3].asInteger();
            uint8_t r = (uint8_t)args[4].asInteger(), g = (uint8_t)args[5].asInteger(), b = (uint8_t)args[6].asInteger();
            ui.drawLine(x0, y0, x1, y1, r, g, b, 255);
            return FValue::nil();
        }, "sdl.line"));

    mod->entries["circle"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>& args) -> FValue {
            int cx = (int)args[0].asInteger(), cy = (int)args[1].asInteger();
            int rad = (int)args[2].asInteger();
            uint8_t r = (uint8_t)args[3].asInteger(), g = (uint8_t)args[4].asInteger(), b = (uint8_t)args[5].asInteger();
            ui.drawCircle(cx, cy, rad, r, g, b, 255);
            return FValue::nil();
        }, "sdl.circle"));

    mod->entries["text"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>& args) -> FValue {
            int x = (int)args[0].asInteger(), y = (int)args[1].asInteger();
            std::string t = args[2].toString();
            uint8_t r = (uint8_t)args[3].asInteger(), g = (uint8_t)args[4].asInteger(), b = (uint8_t)args[5].asInteger();
            ui.drawText(x, y, t, r, g, b, 255);
            return FValue::nil();
        }, "sdl.text"));

    mod->entries["text_bg"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>& args) -> FValue {
            int x = (int)args[0].asInteger(), y = (int)args[1].asInteger();
            std::string t = args[2].toString();
            uint8_t r = (uint8_t)args[3].asInteger(), g = (uint8_t)args[4].asInteger(), b = (uint8_t)args[5].asInteger();
            uint8_t br = (uint8_t)args[6].asInteger(), bg = (uint8_t)args[7].asInteger(), bb = (uint8_t)args[8].asInteger();
            ui.drawTextBG(x, y, t, r, g, b, br, bg, bb, 255);
            return FValue::nil();
        }, "sdl.text_bg"));

    mod->entries["text_width"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>& args) -> FValue {
            return FValue::integer((long long)ui.textWidth(args[0].toString()));
        }, "sdl.text_width"));

    mod->entries["size"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>&) -> FValue {
            int w, h; ui.getSize(w, h);
            auto* arr = new GCArray();
            arr->elements.push_back(FValue::integer(w));
            arr->elements.push_back(FValue::integer(h));
            return FValue::obj(arr);
        }, "sdl.size"));

    // Event pump. Returns a map: {type, ...} or nil if none.
    mod->entries["poll"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>&) -> FValue {
            SDLKeyEvent key; SDLMouseEvent mouse; SDLWindowEvent win;
            bool alive = ui.pumpEvents(&key, &mouse, &win);
            if (win.closed) {
                auto* m = new GCMap();
                m->entries["type"] = FValue::obj(new GCString("quit"));
                return FValue::obj(m);
            }
            if (key.keycode != 0) {
                auto* m = new GCMap();
                m->entries["type"] = FValue::obj(new GCString("key"));
                m->entries["key"] = FValue::integer(key.keycode);
                m->entries["char"] = FValue::obj(new GCString(std::string(1, key.ch)));
                m->entries["shift"] = FValue::boolean(key.shift);
                m->entries["ctrl"] = FValue::boolean(key.ctrl);
                m->entries["alt"] = FValue::boolean(key.alt);
                return FValue::obj(m);
            }
            if (mouse.pressed || mouse.released || mouse.motion || mouse.wheel != 0) {
                auto* m = new GCMap();
                m->entries["type"] = FValue::obj(new GCString(mouse.pressed || mouse.released ? "click" : "motion"));
                m->entries["x"] = FValue::integer(mouse.x);
                m->entries["y"] = FValue::integer(mouse.y);
                m->entries["button"] = FValue::integer(mouse.button);
                m->entries["wheel"] = FValue::integer(mouse.wheel);
                m->entries["pressed"] = FValue::boolean(mouse.pressed);
                m->entries["released"] = FValue::boolean(mouse.released);
                return FValue::obj(m);
            }
            if (win.resized) {
                auto* m = new GCMap();
                m->entries["type"] = FValue::obj(new GCString("resize"));
                m->entries["w"] = FValue::integer(win.width);
                m->entries["h"] = FValue::integer(win.height);
                return FValue::obj(m);
            }
            (void)alive;
            auto* m = new GCMap();
            m->entries["type"] = FValue::obj(new GCString("none"));
            return FValue::obj(m);
        }, "sdl.poll"));

    mod->entries["screenshot"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>& args) -> FValue {
            std::string path = args.size() > 0 ? args[0].toString() : "screenshot.bmp";
            return FValue::boolean(ui.saveScreenshot(path));
        }, "sdl.screenshot"));

    // ============================================================
    // Extended UI primitives (buttons, links, clipboard, etc.)
    // ============================================================

    // sdl.button(x, y, w, h, label, r, g, b, mx, my, mpressed) -> bool
    mod->entries["button"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>& args) -> FValue {
            int x = (int)args[0].asInteger(), y = (int)args[1].asInteger();
            int w = (int)args[2].asInteger(), h = (int)args[3].asInteger();
            std::string label = args[4].toString();
            uint8_t r = (uint8_t)args[5].asInteger(), g = (uint8_t)args[6].asInteger(), b = (uint8_t)args[7].asInteger();
            int mx = args.size() > 8 ? (int)args[8].asInteger() : 0;
            int my = args.size() > 9 ? (int)args[9].asInteger() : 0;
            bool mpressed = args.size() > 10 ? args[10].isTruthy() : false;
            SDLMouseEvent mouse;
            mouse.x = mx; mouse.y = my; mouse.pressed = mpressed; mouse.button = 1;
            bool hover = mx >= x && mx <= x + w && my >= y && my <= y + h;
            return FValue::boolean(ui.button(x, y, w, h, label, r, g, b, mouse, hover));
        }, "sdl.button"));

    // sdl.link(x, y, text, url, r, g, b, mx, my, mpressed) -> bool
    mod->entries["link"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>& args) -> FValue {
            int x = (int)args[0].asInteger(), y = (int)args[1].asInteger();
            std::string text = args[2].toString();
            std::string url = args[3].toString();
            uint8_t r = (uint8_t)args[4].asInteger(), g = (uint8_t)args[5].asInteger(), b = (uint8_t)args[6].asInteger();
            int mx = args.size() > 7 ? (int)args[7].asInteger() : 0;
            int my = args.size() > 8 ? (int)args[8].asInteger() : 0;
            bool mpressed = args.size() > 9 ? args[9].isTruthy() : false;
            int tw = ui.textWidth(text);
            bool hover = mx >= x && mx <= x + tw && my >= y && my <= y + ui.textHeight();
            // Draw underlined text
            uint8_t dr = hover ? 100 : r, dg = hover ? 160 : g, db = hover ? 255 : b;
            ui.drawText(x, y, text, dr, dg, db, 255);
            ui.drawLine(x, y + ui.textHeight(), x + tw, y + ui.textHeight(), dr, dg, db, 255);
            // On click, open URL
            if (hover && mpressed) {
#ifdef __APPLE__
                std::string cmd = "open \"" + url + "\" &";
#elif _WIN32
                std::string cmd = "start \"" + url + "\"";
#else
                std::string cmd = "xdg-open \"" + url + "\" &";
#endif
                system(cmd.c_str());
            }
            return FValue::boolean(hover && mpressed);
        }, "sdl.link"));

    // sdl.clipboard_get() -> string
    mod->entries["clipboard_get"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>&) -> FValue {
            const char* text = SDL_GetClipboardText();
            if (!text) return FValue::obj(new GCString(""));
            return FValue::obj(new GCString(std::string(text)));
        }, "sdl.clipboard_get"));

    // sdl.clipboard_set(text)
    mod->entries["clipboard_set"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>& args) -> FValue {
            std::string text = args.size() > 0 ? args[0].toString() : "";
            SDL_SetClipboardText(text.c_str());
            return FValue::nil();
        }, "sdl.clipboard_set"));

    // sdl.text_selection(x, y, w, h, text, sel_start, sel_end, fg_r, fg_g, fg_b, bg_r, bg_g, bg_b, sel_r, sel_g, sel_b)
    mod->entries["text_selection"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>& args) -> FValue {
            int x = (int)args[0].asInteger(), y = (int)args[1].asInteger();
            int w = (int)args[2].asInteger(), h = (int)args[3].asInteger();
            std::string text = args[4].toString();
            int selStart = args.size() > 5 ? (int)args[5].asInteger() : 0;
            int selEnd = args.size() > 6 ? (int)args[6].asInteger() : 0;
            uint8_t fgR = args.size() > 7 ? (uint8_t)args[7].asInteger() : 255;
            uint8_t fgG = args.size() > 8 ? (uint8_t)args[8].asInteger() : 255;
            uint8_t fgB = args.size() > 9 ? (uint8_t)args[9].asInteger() : 255;
            uint8_t bgR = args.size() > 10 ? (uint8_t)args[10].asInteger() : 30;
            uint8_t bgG = args.size() > 11 ? (uint8_t)args[11].asInteger() : 30;
            uint8_t bgB = args.size() > 12 ? (uint8_t)args[12].asInteger() : 40;
            uint8_t selR = args.size() > 13 ? (uint8_t)args[13].asInteger() : 70;
            uint8_t selG = args.size() > 14 ? (uint8_t)args[14].asInteger() : 130;
            uint8_t selB = args.size() > 15 ? (uint8_t)args[15].asInteger() : 230;

            // Draw background
            ui.drawRect(x, y, w, h, bgR, bgG, bgB, 255);

            // Draw selection highlight
            if (selStart != selEnd && selStart >= 0 && selEnd > 0) {
                int s = std::min(selStart, selEnd);
                int e = std::max(selStart, selEnd);
                s = std::max(0, s);
                e = std::min((int)text.size(), e);
                int sx = x + s * 8;
                int sw = (e - s) * 8;
                ui.drawRect(sx, y, sw, h, selR, selG, selB, 255);
            }

            // Draw text
            ui.drawText(x, y, text, fgR, fgG, fgB, 255);
            return FValue::nil();
        }, "sdl.text_selection"));

    // sdl.rounded_rect(x, y, w, h, r, g, b, radius)
    mod->entries["rounded_rect"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>& args) -> FValue {
            int x = (int)args[0].asInteger(), y = (int)args[1].asInteger();
            int w = (int)args[2].asInteger(), h = (int)args[3].asInteger();
            uint8_t r = (uint8_t)args[4].asInteger(), g = (uint8_t)args[5].asInteger(), b = (uint8_t)args[6].asInteger();
            int radius = args.size() > 7 ? (int)args[7].asInteger() : 4;
            ui.drawRoundedRect(x, y, w, h, radius, r, g, b, 255);
            return FValue::nil();
        }, "sdl.rounded_rect"));

    // sdl.rounded_rect_outline(x, y, w, h, r, g, b, radius)
    mod->entries["rounded_rect_outline"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>& args) -> FValue {
            int x = (int)args[0].asInteger(), y = (int)args[1].asInteger();
            int w = (int)args[2].asInteger(), h = (int)args[3].asInteger();
            uint8_t r = (uint8_t)args[4].asInteger(), g = (uint8_t)args[5].asInteger(), b = (uint8_t)args[6].asInteger();
            int radius = args.size() > 7 ? (int)args[7].asInteger() : 4;
            ui.drawRoundedRectOutline(x, y, w, h, radius, r, g, b, 255);
            return FValue::nil();
        }, "sdl.rounded_rect_outline"));

    // sdl.gradient(x, y, w, h, r1, g1, b1, r2, g2, b2)
    mod->entries["gradient"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>& args) -> FValue {
            int x = (int)args[0].asInteger(), y = (int)args[1].asInteger();
            int w = (int)args[2].asInteger(), h = (int)args[3].asInteger();
            uint8_t r1 = (uint8_t)args[4].asInteger(), g1 = (uint8_t)args[5].asInteger(), b1 = (uint8_t)args[6].asInteger();
            uint8_t r2 = (uint8_t)args[7].asInteger(), g2 = (uint8_t)args[8].asInteger(), b2 = (uint8_t)args[9].asInteger();
            ui.drawGradientV(x, y, w, h, r1, g1, b1, r2, g2, b2, 255);
            return FValue::nil();
        }, "sdl.gradient"));

    // sdl.shadow(x, y, w, h, radius, alpha)
    mod->entries["shadow"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>& args) -> FValue {
            int x = (int)args[0].asInteger(), y = (int)args[1].asInteger();
            int w = (int)args[2].asInteger(), h = (int)args[3].asInteger();
            int radius = args.size() > 4 ? (int)args[4].asInteger() : 8;
            uint8_t a = args.size() > 5 ? (uint8_t)args[5].asInteger() : 60;
            ui.drawShadow(x, y, w, h, radius, a);
            return FValue::nil();
        }, "sdl.shadow"));

    // sdl.text_height() -> int
    mod->entries["text_height"] = FValue::obj(new GCNative(
        [&ui](const std::vector<FValue>&) -> FValue {
            return FValue::integer(ui.textHeight());
        }, "sdl.text_height"));

    vm.defineModule("sdl", mod);
}

} // namespace forge::fvm
