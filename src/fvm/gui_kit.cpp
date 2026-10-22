#include "gui_kit.hpp"
#include "sdl2_ui.hpp"
#include "liquid_glass.hpp"
#include <SDL2/SDL_keycode.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>

namespace forge {
namespace fvm {

// ============================================================
// State
// ============================================================

namespace {

struct GuiState {
    std::unordered_map<long long, GuiWindow> windows;
    std::deque<GuiEvent> events;
    long long nextId = 1;
    SDL2UI* ui = nullptr;
    bool sdlReady = false;
    GlassEngine glass;

    // wallpaper: rendered at 1/4 resolution and scaled up (smooth gradients)
    SDL_Surface* wpSurf = nullptr;
    SDL_Texture* wpTex = nullptr;
    int wpW = 0, wpH = 0;
    int wpScale = 4;
};

GuiState& S() {
    static GuiState st;
    return st;
}

SDL2UI& ui() {
    if (!S().ui) S().ui = &SDL2UI::instance();
    return *S().ui;
}

constexpr uint8_t WIN_BG_R = 18, WIN_BG_G = 20, WIN_BG_B = 30;
constexpr uint8_t ACCENT_R = 10, ACCENT_G = 132, ACCENT_B = 255;
constexpr uint8_t TEXT_R = 240, TEXT_G = 242, TEXT_B = 248;
constexpr uint8_t DIM_R = 168, DIM_G = 172, DIM_B = 186;

void queueEvent(const GuiEvent& e) { S().events.push_back(e); }

inline uint8_t clamp8(float v) {
    int i = (int)(v + 0.5f);
    if (i < 0) i = 0;
    if (i > 255) i = 255;
    return (uint8_t)i;
}

// deterministic hash -> speck positions (stable between frames)
inline float hash01(int i) {
    uint32_t x = (uint32_t)i * 2654435761u;
    x ^= x >> 15; x *= 2246822519u; x ^= x >> 13;
    return (float)(x & 0xFFFFFF) / (float)0xFFFFFF;
}

// ---------------------------------------------------------------
// wallpaper
// ---------------------------------------------------------------

void updateWallpaper(int W, int H, float t) {
    auto& st = S();
    int sw = std::max(1, W / st.wpScale), sh = std::max(1, H / st.wpScale);
    if (!st.wpSurf || st.wpSurf->w != sw || st.wpSurf->h != sh) {
        if (st.wpSurf) SDL_FreeSurface(st.wpSurf);
        st.wpSurf = SDL_CreateRGBSurfaceWithFormat(0, sw, sh, 32, SDL_PIXELFORMAT_RGBA32);
        st.wpTex = nullptr;
    }
    struct Orb { float cx, cy, r, cr, cg, cb; };
    Orb orbs[3] = {
        { 0.26f + 0.05f * std::sin(t * 0.42f), 0.32f + 0.04f * std::cos(t * 0.31f),
          0.52f, 0.20f, 0.34f, 0.62f },
        { 0.74f + 0.05f * std::cos(t * 0.27f), 0.70f + 0.05f * std::sin(t * 0.23f),
          0.48f, 0.44f, 0.24f, 0.58f },
        { 0.56f + 0.06f * std::sin(t * 0.19f), 0.12f + 0.04f * std::cos(t * 0.37f),
          0.38f, 0.58f, 0.38f, 0.24f },
    };
    uint8_t* px = (uint8_t*)st.wpSurf->pixels;
    const int pitch = st.wpSurf->pitch;
    for (int y = 0; y < sh; y++) {
        float fy = (float)y / (float)sh;
        for (int x = 0; x < sw; x++) {
            float fx = (float)x / (float)sw;
            float r = 0.045f + 0.055f * (1.0f - fy);
            float g = 0.055f + 0.045f * (1.0f - fy);
            float b = 0.090f + 0.090f * (1.0f - fy);
            for (const auto& o : orbs) {
                float dx = fx - o.cx, dy = fy - o.cy;
                float d = (dx * dx + dy * dy) / (o.r * o.r);
                float g2 = std::exp(-d * 2.2f);
                r += o.cr * g2 * 0.42f;
                g += o.cg * g2 * 0.42f;
                b += o.cb * g2 * 0.42f;
            }
            float vx = fx - 0.5f, vy = fy - 0.5f;
            float vig = 1.0f - 0.5f * (vx * vx + vy * vy) * 2.4f;
            uint8_t* p = px + (size_t)y * pitch + (size_t)x * 4;
            p[0] = clamp8(r * vig * 255.0f);
            p[1] = clamp8(g * vig * 255.0f);
            p[2] = clamp8(b * vig * 255.0f);
            p[3] = 255;
        }
    }
}

void drawWallpaper(SDL_Renderer* ren, int W, int H, float t) {
    auto& st = S();
    updateWallpaper(W, H, t);
    if (!st.wpTex) {
        st.wpTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32,
                                     SDL_TEXTUREACCESS_STREAMING, st.wpSurf->w, st.wpSurf->h);
        if (!st.wpTex) return;
        SDL_SetTextureBlendMode(st.wpTex, SDL_BLENDMODE_BLEND);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    }
    SDL_UpdateTexture(st.wpTex, nullptr, st.wpSurf->pixels, st.wpSurf->pitch);
    SDL_Rect full{ 0, 0, W, H };
    SDL_RenderCopy(ren, st.wpTex, nullptr, &full);

    // high frequency detail so the lens has something to refract
    for (int x = 0; x < W; x += 46)
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 12);
    for (int x = 0; x < W; x += 46) {
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 12);
        SDL_RenderDrawLine(ren, x, 0, x, H);
    }
    for (int y = 0; y < H; y += 46) {
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 10);
        SDL_RenderDrawLine(ren, 0, y, W, y);
    }
    for (int i = 0; i < 220; i++) {
        int sx = (int)(hash01(i * 3 + 1) * W);
        int sy = (int)(hash01(i * 7 + 5) * H);
        int a = 60 + (int)(hash01(i * 11 + 9) * 90);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, a);
        SDL_RenderDrawPoint(ren, sx, sy);
    }
}

// ---------------------------------------------------------------
// small drawing helpers
// ---------------------------------------------------------------

void drawTextShadow(SDL2UI& u, int x, int y, const std::string& t,
                    uint8_t r, uint8_t g, uint8_t b) {
    u.drawText(x + 1, y + 1, t, 0, 0, 0, 120);
    u.drawText(x, y, t, r, g, b, 255);
}

void drawTextCentered(SDL2UI& u, int cx, int cy, const std::string& t,
                      uint8_t r, uint8_t g, uint8_t b) {
    int tw = (int)t.size() * 8;
    drawTextShadow(u, cx - tw / 2, cy - 4, t, r, g, b);
}

// arc from a0 to a1 (degrees, clockwise in screen space)
void drawArc(SDL2UI& u, int cx, int cy, int radius, double a0, double a1,
             int thickness, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    const double PI = 3.14159265358979323846;
    double span = a1 - a0;
    int steps = std::max(8, (int)(std::fabs(span) / 180.0 * radius * 2.2));
    for (int i = 0; i <= steps; i++) {
        double ang = (a0 + span * (double)i / steps) * PI / 180.0;
        int px = cx + (int)std::lround(std::cos(ang) * radius);
        int py = cy + (int)std::lround(std::sin(ang) * radius);
        for (int tx = -thickness / 2; tx <= thickness / 2; tx++)
            for (int ty = -thickness / 2; ty <= thickness / 2; ty++)
                u.drawRect(px + tx, py + ty, 1, 1, r, g, b, a);
    }
}

// ---------------------------------------------------------------
// glass materials
// ---------------------------------------------------------------

GlassStyle materialFor(const std::string& kind, const GuiWidget& w) {
    GlassStyle st;
    st.cornerRadius = 12;
    st.tintR = 235; st.tintG = 240; st.tintB = 255;
    st.tintStrength = 0.07f;
    st.vibrancy = 0.28f;
    st.brightness = 1.03f;
    st.specular = 1.0f;
    st.shadow = 0.45f;
    st.refractionHeight = 150.0f;
    st.bevelWidth = 46;
    st.dispersion = 0.12f;

    if (kind == "button") {
        st.cornerRadius = w.h / 2;
        st.tintR = ACCENT_R; st.tintG = ACCENT_G; st.tintB = ACCENT_B;
        st.tintStrength = 0.11f;
        st.refractionHeight = 90.0f;
        st.bevelWidth = 26;
        st.shadow = 0.5f;
    } else if (kind == "panel") {
        st.cornerRadius = 18;
        st.tintStrength = 0.05f;
        st.shadow = 0.55f;
        st.refractionHeight = 190.0f;
        st.bevelWidth = 60;
    } else if (kind == "textfield") {
        st.cornerRadius = 10;
        st.tintR = 12; st.tintG = 14; st.tintB = 22;
        st.tintStrength = 0.30f;
        st.vibrancy = 0.12f;
        st.refractionHeight = 60.0f;
        st.bevelWidth = 22;
        st.shadow = 0.30f;
    } else if (kind == "checkbox") {
        st.cornerRadius = 8;
        st.refractionHeight = 44.0f;
        st.bevelWidth = 16;
    } else if (kind == "toggle" || kind == "slider" || kind == "tabs" ||
               kind == "combo" || kind == "list" || kind == "gauge") {
        st.cornerRadius = 14;
        st.tintStrength = 0.08f;
    } else if (kind == "radio") {
        st.cornerRadius = w.h / 2;
        st.refractionHeight = 44.0f;
        st.bevelWidth = 16;
    }
    if (w.hovered) {
        st.tintStrength = std::min(1.0f, st.tintStrength + 0.05f);
        st.brightness += 0.04f;
    }
    return st;
}

void drawGlass(SDL_Renderer* ren, const std::string& kind, const GuiWidget& w,
               int x, int y, int width, int height, float press = 0.0f) {
    if (width <= 0 || height <= 0) return;
    GlassStyle st = materialFor(kind, w);
    st.cornerRadius = std::min(st.cornerRadius, std::min(width, height) / 2);
    S().glass.drawLens(ren, x, y, width, height, st, press, (float)SDL_GetTicks() / 1000.0f);
}

// ---------------------------------------------------------------
// widget rendering
// ---------------------------------------------------------------

void renderWidget(GuiWindow& win, GuiWidget& w, SDL_Renderer* ren, float t);

void renderChildren(GuiWindow& win, GuiWidget& w, SDL_Renderer* ren, float t) {
    for (auto& c : w.children) renderWidget(win, c, ren, t);
}

void renderWidget(GuiWindow& win, GuiWidget& w, SDL_Renderer* ren, float t) {
    SDL2UI& u = ui();
    const std::string& k = w.kind;

    if (k == "label") {
        drawTextShadow(u, w.x, w.y, w.text, TEXT_R, TEXT_G, TEXT_B);
        return;
    }

    if (k == "button") {
        float press = w.pressAnim;
        int h = (int)(w.h * (1.0f - 0.04f * press));
        int y = w.y + (w.h - h) / 2;
        drawGlass(ren, "button", w, w.x, y, w.w, h, press);
        drawTextCentered(u, w.x + w.w / 2, y + h / 2, w.text, TEXT_R, TEXT_G, TEXT_B);
        return;
    }

    if (k == "textfield") {
        bool focused = win.focusedField == w.id;
        drawGlass(ren, "textfield", w, w.x, w.y, w.w, w.h);
        if (focused) {
            u.drawRoundedRectOutline(w.x, w.y, w.w, w.h, 10, ACCENT_R, ACCENT_G, ACCENT_B, 190);
        }
        std::string txt = win.textFieldBuffer.empty() ? w.text : win.textFieldBuffer;
        bool placeholder = win.textFieldBuffer.empty() && !w.text.empty();
        u.drawText(w.x + 9, w.y + (w.h - 8) / 2, txt,
                   placeholder ? DIM_R : TEXT_R, placeholder ? DIM_G : TEXT_G,
                   placeholder ? DIM_B : TEXT_B);
        if (focused && (SDL_GetTicks() / 500) % 2 == 0) {
            int cw = (int)win.textFieldBuffer.size() * 8;
            u.drawRect(w.x + 10 + cw, w.y + 5, 2, w.h - 10, TEXT_R, TEXT_G, TEXT_B);
        }
        return;
    }

    if (k == "checkbox") {
        int bs = 20;
        drawGlass(ren, "checkbox", w, w.x, w.y, bs, bs);
        if (w.animCheck > 0.02f) {
            // liquid check mark, scaled by the eased check state
            float s = w.animCheck;
            int cx = w.x + bs / 2, cy = w.y + bs / 2;
            int half = (int)(6 * s);
            for (int i = -half; i <= half; i++) {
                int t1 = (int)(-half * 0.5f + (float)i * 0.5f);
                int t2 = (int)(half - (float)i * 0.5f);
                u.drawRect(cx + t1, cy + (int)(std::fabs((float)i) * 0.5f) - 1, 2, 2,
                           ACCENT_R, ACCENT_G, ACCENT_B);
                u.drawRect(cx + t2, cy + (int)(std::fabs((float)i) * 0.5f) - 1, 2, 2,
                           ACCENT_R, ACCENT_G, ACCENT_B);
            }
        }
        drawTextShadow(u, w.x + bs + 8, w.y + (w.h - 8) / 2, w.text, TEXT_R, TEXT_G, TEXT_B);
        return;
    }

    if (k == "radio") {
        int bs = 20;
        drawGlass(ren, "radio", w, w.x, w.y, bs, bs);
        if (w.animCheck > 0.02f) {
            int r = (int)(7 * w.animCheck);
            u.drawCircle(w.x + bs / 2, w.y + bs / 2, r, ACCENT_R, ACCENT_G, ACCENT_B);
        }
        drawTextShadow(u, w.x + bs + 8, w.y + (w.h - 8) / 2, w.text, TEXT_R, TEXT_G, TEXT_B);
        return;
    }

    if (k == "toggle") {
        int trackW = 46, trackH = 26;
        drawGlass(ren, "toggle", w, w.x, w.y, trackW, trackH);
        if (w.animCheck > 0.02f) {
            u.drawRoundedRect(w.x + 2, w.y + 2,
                               (int)((trackW - 4) * w.animCheck), trackH - 4,
                               (trackH - 4) / 2, ACCENT_R, ACCENT_G, ACCENT_B, 210);
        }
        int knob = trackH - 6;
        int kx = w.x + 3 + (int)((trackW - knob - 6) * w.animCheck);
        drawGlass(ren, "radio", w, kx, w.y + 3, knob, knob);
        drawTextShadow(u, w.x + trackW + 10, w.y + (w.h - 8) / 2, w.text,
                       TEXT_R, TEXT_G, TEXT_B);
        return;
    }

    if (k == "slider") {
        int trackH = 10;
        int ty = w.y + (w.h - trackH) / 2;
        drawGlass(ren, "slider", w, w.x, ty, w.w, trackH);
        double v = std::max(0.0, std::min(1.0, w.anim));
        int fillW = (int)(w.w * v);
        if (fillW > 2)
            u.drawRoundedRect(w.x, ty, fillW, trackH, trackH / 2, ACCENT_R, ACCENT_G, ACCENT_B, 235);
        int knobR = (w.h < 26 ? 10 : 13);
        int kx = w.x + (int)((w.w - knobR) * v) ;
        int ky = w.y + (w.h - knobR) / 2;
        drawGlass(ren, "radio", w, kx, ky, knobR, knobR);
        return;
    }

    if (k == "gauge") {
        int size = std::min(w.w, w.h);
        int cx = w.x + size / 2, cy = w.y + size / 2;
        int r = size / 2 - 6;
        drawGlass(ren, "gauge", w, w.x, w.y, size, size);
        drawArc(u, cx, cy, r, 135, 405, 7, 255, 255, 255, 46);
        double v = std::max(0.0, std::min(1.0, w.anim));
        if (v > 0.001)
            drawArc(u, cx, cy, r, 135, 135 + 270 * v, 7, ACCENT_R, ACCENT_G, ACCENT_B, 255);
        const double PI = 3.14159265358979323846;
        double ang = (135 + 270 * v) * PI / 180.0;
        int nx = cx + (int)std::lround(std::cos(ang) * (r - 12));
        int ny = cy + (int)std::lround(std::sin(ang) * (r - 12));
        u.drawLine(cx, cy, nx, ny, 255, 255, 255, 230);
        u.drawCircle(cx, cy, 4, 255, 255, 255, 255);
        std::string pct = std::to_string((int)(v * 100)) + "%";
        drawTextCentered(u, cx, cy + r / 2, pct, TEXT_R, TEXT_G, TEXT_B);
        return;
    }

    if (k == "progress") {
        drawGlass(ren, "slider", w, w.x, w.y, w.w, w.h);
        double v = std::max(0.0, std::min(1.0, w.anim));
        int fillW = (int)((w.w - 4) * v);
        if (fillW > 2) {
            u.drawRoundedRect(w.x + 2, w.y + 2, fillW, w.h - 4, (w.h - 4) / 2,
                              ACCENT_R, ACCENT_G, ACCENT_B, 225);
        }
        std::string pct = std::to_string((int)(v * 100)) + "%";
        drawTextCentered(u, w.x + w.w / 2, w.y + w.h / 2, pct, TEXT_R, TEXT_G, TEXT_B);
        return;
    }

    if (k == "tabs") {
        drawGlass(ren, "tabs", w, w.x, w.y, w.w, w.h);
        int n = (int)w.items.size();
        if (n > 0) {
            int tabW = w.w / n;
            int indW = tabW - 6;
            int ix = w.x + 3 + tabW * w.selected + (tabW - indW) / 2;
            drawGlass(ren, "button", w, ix, w.y + 3, indW, w.h - 6);
            for (int i = 0; i < n; i++) {
                uint8_t rr = (i == w.selected) ? TEXT_R : DIM_R;
                uint8_t gg = (i == w.selected) ? TEXT_G : DIM_G;
                uint8_t bb = (i == w.selected) ? TEXT_B : DIM_B;
                drawTextCentered(u, w.x + tabW * i + tabW / 2, w.y + w.h / 2, w.items[i], rr, gg, bb);
            }
        }
        return;
    }

    if (k == "list") {
        drawGlass(ren, "list", w, w.x, w.y, w.w, w.h);
        int rowH = 26;
        int visible = std::max(1, (w.h - 8) / rowH);
        for (int i = 0; i < (int)w.items.size() && i < visible; i++) {
            if (i == w.selected) {
                drawGlass(ren, "button", w, w.x + 4, w.y + 4 + i * rowH, w.w - 8, rowH - 4);
            }
            uint8_t rr = (i == w.selected) ? TEXT_R : DIM_R;
            uint8_t gg = (i == w.selected) ? TEXT_G : DIM_G;
            uint8_t bb = (i == w.selected) ? TEXT_B : DIM_B;
            drawTextShadow(u, w.x + 12, w.y + 4 + i * rowH + (rowH - 8) / 2 - 2,
                           w.items[i], rr, gg, bb);
        }
        return;
    }

    if (k == "combo") {
        drawGlass(ren, "combo", w, w.x, w.y, w.w, w.h);
        std::string label = (w.selected >= 0 && w.selected < (int)w.items.size())
                            ? w.items[w.selected] : w.text;
        drawTextShadow(u, w.x + 10, w.y + (w.h - 8) / 2, label, TEXT_R, TEXT_G, TEXT_B);
        // chevron
        int ax = w.x + w.w - 18, ay = w.y + w.h / 2;
        for (int i = 0; i < 5; i++)
            u.drawRect(ax - 4 + i, ay - 2 + i, 2, 2, DIM_R, DIM_G, DIM_B);
        for (int i = 0; i < 5; i++)
            u.drawRect(ax + 4 - i, ay - 2 + i, 2, 2, DIM_R, DIM_G, DIM_B);
        if (w.open) {
            int rowH = 24;
            int dropH = rowH * (int)w.items.size() + 8;
            int dy = w.y + w.h + 4;
            for (size_t i = 0; i < w.items.size(); i++) {
                int ry = dy + 4 + (int)i * rowH;
                if ((int)i == w.selected)
                    drawGlass(ren, "button", w, w.x, ry, w.w, rowH);
                drawTextShadow(u, w.x + 10, ry + (rowH - 8) / 2, w.items[i], TEXT_R, TEXT_G, TEXT_B);
            }
            (void)dropH;
        }
        return;
    }

    if (k == "panel") {
        drawGlass(ren, "panel", w, w.x, w.y, w.w, w.h);
        renderChildren(win, w, ren, t);
        return;
    }
}

// ============================================================
// animation
// ============================================================

void animate(GuiWidget& w, double dt) {
    double k = std::min(1.0, dt * 12.0);
    w.anim += (w.value - w.anim) * k;
    w.animCheck += ((w.checked ? 1.0 : 0.0) - w.animCheck) * k;
    w.pressAnim += ((w.pressed ? 1.0 : 0.0) - w.pressAnim) * k;
    w.glow += ((w.hovered ? 1.0 : 0.0) - w.glow) * k;
    for (auto& c : w.children) animate(c, dt);
}

} // namespace

// ============================================================
// Widget tree management
// ============================================================

GuiKit& GuiKit::instance() {
    static GuiKit kit;
    return kit;
}

long long GuiKit::createWindow(const std::string& title, int w, int h) {
    auto& st = S();
    long long id = st.nextId++;
    GuiWindow win;
    win.id = id;
    win.title = title;
    win.width = w;
    win.height = h;
    st.windows[id] = win;

    if (!st.sdlReady) {
        ui().init(w > 0 ? w : 800, h > 0 ? h : 600, title);
        st.sdlReady = true;
    }
    return id;
}

long long GuiKit::addWidget(long long winId, const std::string& kind,
                            int x, int y, int w, int h, const std::string& text) {
    GuiWindow* win = findWindow(winId);
    if (!win) return -1;
    GuiWidget wd;
    wd.id = S().nextId++;
    wd.kind = kind;
    wd.x = x; wd.y = y; wd.w = w; wd.h = h;
    wd.text = text;
    if (kind == "toggle" || kind == "radio") wd.h = std::max(h, 26);
    win->children.push_back(wd);
    return wd.id;
}

GuiWindow* GuiKit::findWindow(long long winId) {
    auto it = S().windows.find(winId);
    return it == S().windows.end() ? nullptr : &it->second;
}

GuiWidget* GuiKit::findWidget(long long winId, long long widgetId) {
    GuiWindow* win = findWindow(winId);
    if (!win) return nullptr;
    for (auto& w : win->children)
        if (w.id == widgetId) return &w;
    return nullptr;
}

void GuiKit::closeWindow(long long winId) {
    auto it = S().windows.find(winId);
    if (it != S().windows.end()) it->second.open = false;
}

std::vector<long long> GuiKit::windowIds() const {
    std::vector<long long> ids;
    for (auto& [id, _] : S().windows) ids.push_back(id);
    return ids;
}

// ============================================================
// Event pump / frame tick
// ============================================================

namespace {

bool widgetHit(const GuiWidget& w, int mx, int my) {
    bool inside = mx >= w.x && mx < w.x + w.w && my >= w.y && my < w.y + w.h;
    if (w.kind == "checkbox" || w.kind == "radio" || w.kind == "toggle") {
        int labelW = (int)w.text.size() * 8;
        bool overLabel = mx >= w.x && mx < w.x + 20 + 8 + labelW &&
                         my >= w.y && my < w.y + std::max(20, w.h);
        inside = inside || overLabel;
    }
    if (w.kind == "tabs") {
        int n = (int)w.items.size();
        if (n > 0) {
            int tabW = w.w / n;
            int idx = (mx - w.x) / tabW;
            if (idx >= 0 && idx < n && my >= w.y && my < w.y + w.h) inside = true;
        }
    }
    if (w.kind == "list") {
        int rowH = 26;
        int visible = std::max(1, (w.h - 8) / rowH);
        if (my >= w.y + 4 && my < w.y + 4 + visible * rowH) {
            int idx = (my - w.y - 4) / rowH;
            if (idx < (int)w.items.size()) inside = true;
        }
    }
    return inside;
}

void handleWidgetClick(GuiWindow* win, GuiWidget& w, int mx, int my) {
    const std::string& k = w.kind;
    if (k == "button") {
        queueEvent({win->id, w.id, "click", ""});
    } else if (k == "checkbox" || k == "toggle") {
        w.checked = !w.checked;
        queueEvent({win->id, w.id, "check", w.checked ? "true" : "false"});
    } else if (k == "radio") {
        for (auto& sib : win->children) {
            if (sib.kind == "radio" && sib.group == w.group && !sib.group.empty())
                sib.checked = false;
        }
        w.checked = true;
        queueEvent({win->id, w.id, "check", "true"});
    } else if (k == "tabs") {
        int n = (int)w.items.size();
        int tabW = w.w / std::max(1, n);
        int idx = std::clamp((mx - w.x) / std::max(1, tabW), 0, n - 1);
        if (idx != w.selected) {
            w.selected = idx;
            queueEvent({win->id, w.id, "select", std::to_string(idx)});
        }
    } else if (k == "list") {
        int rowH = 26;
        int idx = (my - w.y - 4) / rowH;
        if (idx >= 0 && idx < (int)w.items.size()) {
            w.selected = idx;
            queueEvent({win->id, w.id, "select", std::to_string(idx)});
        }
    } else if (k == "combo") {
        if (!w.open) {
            w.open = true;
        } else {
            int rowH = 24;
            int dropY = w.y + w.h + 8;
            if (my >= dropY && my < dropY + rowH * (int)w.items.size()) {
                int idx = (my - dropY) / rowH;
                if (idx >= 0 && idx < (int)w.items.size()) {
                    w.selected = idx;
                    w.open = false;
                    queueEvent({win->id, w.id, "select", std::to_string(idx)});
                }
            } else {
                w.open = false;
            }
        }
    }
}

void updateSlider(GuiWindow* win, GuiWidget& w, int mx) {
    if (w.w <= 0) return;
    double v = (double)(mx - w.x) / (double)w.w;
    v = std::max(0.0, std::min(1.0, v));
    if (std::fabs(v - w.value) > 1e-6) {
        w.value = v;
        char buf[32];
        snprintf(buf, sizeof(buf), "%.4f", v);
        queueEvent({win->id, w.id, "change", buf});
    }
}

} // namespace

bool GuiKit::tick() {
    auto& st = S();
    bool anyOpen = false;
    for (auto& [id, win] : st.windows) if (win.open) anyOpen = true;
    if (!anyOpen) return false;

    SDLKeyEvent key;
    SDLMouseEvent mouse;
    SDLWindowEvent winev;
    bool alive = ui().pumpEvents(&key, &mouse, &winev);
    if (!alive || winev.closed) {
        for (auto& [id, win] : st.windows) {
            if (win.open) {
                win.open = false;
                queueEvent({id, 0, "close", ""});
            }
        }
        return false;
    }

    GuiWindow* top = nullptr;
    for (auto& [id, win] : st.windows)
        if (win.open && (!top || id > top->id)) top = &win;
    if (!top) return false;

    static uint32_t lastTicks = 0;
    uint32_t now = SDL_GetTicks();
    double dt = lastTicks ? (double)(now - lastTicks) / 1000.0 : 0.016;
    lastTicks = now;
    if (dt > 0.1) dt = 0.1;

    // keyboard
    if (key.keycode != 0) {
        if (top->focusedField >= 0) {
            if (key.keycode == SDLK_BACKSPACE) {
                if (!top->textFieldBuffer.empty()) {
                    top->textFieldBuffer.pop_back();
                    queueEvent({top->id, top->focusedField, "text", top->textFieldBuffer});
                }
            } else if (key.ch >= 32 && key.ch < 127) {
                top->textFieldBuffer += key.ch;
                queueEvent({top->id, top->focusedField, "text", top->textFieldBuffer});
            }
        }
    }

    // mouse
    for (auto& w : top->children) {
        bool inside = widgetHit(w, mouse.x, mouse.y);
        if (w.kind == "button" || w.kind == "checkbox" || w.kind == "toggle" ||
            w.kind == "radio" || w.kind == "slider" || w.kind == "tabs" ||
            w.kind == "list" || w.kind == "combo") {
            w.hovered = inside;
        }

        if (mouse.pressed && mouse.button == 1 && inside) {
            if (w.kind == "slider") {
                w.pressed = true;
                updateSlider(top, w, mouse.x);
            } else {
                w.pressed = true;
            }
        } else if (w.pressed && mouse.released && mouse.button == 1) {
            w.pressed = false;
            if (inside) handleWidgetClick(top, w, mouse.x, mouse.y);
        } else if (!mouse.pressed) {
            w.pressed = false;
        }

        if (w.kind == "slider" && w.pressed && mouse.motion) {
            updateSlider(top, w, mouse.x);
        }

        if (w.kind == "textfield") {
            bool wasFocused = top->focusedField == w.id;
            if (inside && mouse.pressed && mouse.button == 1 && !wasFocused) {
                top->focusedField = w.id;
            } else if (!inside && mouse.pressed && wasFocused) {
                top->focusedField = -1;
            }
        }
    }

    // animation
    for (auto& w : top->children) animate(w, dt);

    // render
    SDL_Renderer* ren = ui().renderer();
    int W = top->width, H = top->height;
    ui().clear(WIN_BG_R, WIN_BG_G, WIN_BG_B);
    float tsec = (float)SDL_GetTicks() / 1000.0f;
    drawWallpaper(ren, W, H, tsec);
    st.glass.beginFrame(ren, W, H);

    // window chrome: glass title bar
    {
        GuiWidget chrome;
        chrome.kind = "panel";
        chrome.x = 8; chrome.y = 8; chrome.w = W - 16; chrome.h = 34;
        drawGlass(ren, "panel", chrome, chrome.x, chrome.y, chrome.w, chrome.h);
        drawTextShadow(ui(), 20, 18, top->title, TEXT_R, TEXT_G, TEXT_B);
        ui().drawCircle(W - 46, 25, 6, 255, 95, 86);
        ui().drawCircle(W - 30, 25, 6, 255, 189, 46);
        ui().drawCircle(W - 14, 25, 6, 39, 201, 63);
    }

    for (auto& w : top->children) renderWidget(*top, w, ren, tsec);

    ui().present();
    ui().delay(16);

    return true;
}

GCMap* GuiKit::poll() {
    auto& st = S();
    if (st.events.empty()) return nullptr;
    GuiEvent ev = st.events.front();
    st.events.pop_front();

    GCMap* m = new GCMap();
    m->entries["window"] = FValue::integer(ev.windowId);
    m->entries["id"] = FValue::integer(ev.widgetId);
    m->entries["type"] = FValue::obj(new GCString(ev.type));
    m->entries["text"] = FValue::obj(new GCString(ev.text));
    return m;
}

// ============================================================
// Module registration
// ============================================================

namespace {

GCNative* native(const char* name, GCNative::NativeFn fn) {
    return new GCNative(std::move(fn), name);
}

std::string argStr(const std::vector<FValue>& a, size_t i) {
    return i < a.size() ? a[i].toString() : "";
}

std::vector<std::string> splitItems(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == '|') { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty() || !out.empty()) out.push_back(cur);
    return out;
}

GuiWidget* findAnywhere(long long id) {
    for (auto wid : GuiKit::instance().windowIds()) {
        if (GuiWidget* w = GuiKit::instance().findWidget(wid, id)) return w;
    }
    return nullptr;
}

} // namespace

void defineGuiModule(ForgeVM& vm) {
    auto* m = new GCMap();

    m->entries["window"] = FValue::obj(native("gui.window", [](const std::vector<FValue>& a) -> FValue {
        std::string title = argStr(a, 0);
        int w = a.size() > 1 ? (int)a[1].asInteger() : 800;
        int h = a.size() > 2 ? (int)a[2].asInteger() : 600;
        return FValue::integer(GuiKit::instance().createWindow(title, w, h));
    }));

    auto adder = [](const char* kind) {
        return GCNative::NativeFn([kind](const std::vector<FValue>& a) -> FValue {
            if (a.size() < 6) throw std::runtime_error(std::string("gui.") + kind + "() expects window,x,y,w,h,text");
            long long winId = a[0].asInteger();
            int x = (int)a[1].asInteger(), y = (int)a[2].asInteger();
            int w = (int)a[3].asInteger(), h = (int)a[4].asInteger();
            std::string text = a.size() > 5 ? a[5].toString() : "";
            return FValue::integer(
                GuiKit::instance().addWidget(winId, kind, x, y, w, h, text));
        });
    };
    m->entries["button"]   = FValue::obj(new GCNative(adder("button"), "gui.button"));
    m->entries["textfield"] = FValue::obj(new GCNative(adder("textfield"), "gui.textfield"));
    m->entries["panel"]    = FValue::obj(new GCNative(adder("panel"), "gui.panel"));

    m->entries["label"] = FValue::obj(native("gui.label", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() >= 6)
            return FValue::integer(GuiKit::instance().addWidget(a[0].asInteger(), "label",
                (int)a[1].asInteger(), (int)a[2].asInteger(),
                (int)a[3].asInteger(), (int)a[4].asInteger(), argStr(a, 5)));
        if (a.size() >= 4)
            return FValue::integer(GuiKit::instance().addWidget(a[0].asInteger(), "label",
                (int)a[1].asInteger(), (int)a[2].asInteger(), 10, 12, argStr(a, 3)));
        throw std::runtime_error("gui.label() expects window,x,y,text");
    }));

    m->entries["screenshot"] = FValue::obj(native("gui.screenshot", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 1) throw std::runtime_error("gui.screenshot() expects path");
        bool ok = ui().saveScreenshot(argStr(a, 0));
        return FValue::boolean(ok);
    }));

    m->entries["checkbox"] = FValue::obj(native("gui.checkbox", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 5) throw std::runtime_error("gui.checkbox() expects window,x,y,label,checked");
        long long id = GuiKit::instance().addWidget(a[0].asInteger(), "checkbox",
            (int)a[1].asInteger(), (int)a[2].asInteger(), 24, 24, argStr(a, 3));
        GuiWidget* w = GuiKit::instance().findWidget(a[0].asInteger(), id);
        if (w) { w->checked = a[4].isTruthy(); w->animCheck = w->checked ? 1.0 : 0.0; }
        return FValue::integer(id);
    }));

    m->entries["progress"] = FValue::obj(native("gui.progress", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 6) throw std::runtime_error("gui.progress() expects window,x,y,w,h,value");
        long long id = GuiKit::instance().addWidget(a[0].asInteger(), "progress",
            (int)a[1].asInteger(), (int)a[2].asInteger(),
            (int)a[3].asInteger(), (int)a[4].asInteger(), "");
        double v = a[5].asNumber();
        GuiWidget* w = GuiKit::instance().findWidget(a[0].asInteger(), id);
        if (w) { w->value = std::max(0.0, std::min(1.0, v)); w->anim = w->value; }
        return FValue::integer(id);
    }));

    // ---- liquid glass widget set -------------------------------

    m->entries["slider"] = FValue::obj(native("gui.slider", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 6) throw std::runtime_error("gui.slider() expects window,x,y,w,h,value");
        long long id = GuiKit::instance().addWidget(a[0].asInteger(), "slider",
            (int)a[1].asInteger(), (int)a[2].asInteger(),
            (int)a[3].asInteger(), (int)a[4].asInteger(), "");
        GuiWidget* w = GuiKit::instance().findWidget(a[0].asInteger(), id);
        if (w) { w->value = std::max(0.0, std::min(1.0, a[5].asNumber())); w->anim = w->value; }
        return FValue::integer(id);
    }));

    m->entries["gauge"] = FValue::obj(native("gui.gauge", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 6) throw std::runtime_error("gui.gauge() expects window,x,y,size,value,label");
        int size = (int)a[3].asInteger();
        long long id = GuiKit::instance().addWidget(a[0].asInteger(), "gauge",
            (int)a[1].asInteger(), (int)a[2].asInteger(), size, size, "");
        GuiWidget* w = GuiKit::instance().findWidget(a[0].asInteger(), id);
        if (w) { w->value = std::max(0.0, std::min(1.0, a[4].asNumber())); w->anim = w->value; }
        return FValue::integer(id);
    }));

    m->entries["toggle"] = FValue::obj(native("gui.toggle", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 5) throw std::runtime_error("gui.toggle() expects window,x,y,label,checked");
        long long id = GuiKit::instance().addWidget(a[0].asInteger(), "toggle",
            (int)a[1].asInteger(), (int)a[2].asInteger(), 46, 26, argStr(a, 3));
        GuiWidget* w = GuiKit::instance().findWidget(a[0].asInteger(), id);
        if (w) { w->checked = a[4].isTruthy(); w->animCheck = w->checked ? 1.0 : 0.0; }
        return FValue::integer(id);
    }));

    m->entries["radio"] = FValue::obj(native("gui.radio", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 6) throw std::runtime_error("gui.radio() expects window,x,y,label,group,checked");
        long long id = GuiKit::instance().addWidget(a[0].asInteger(), "radio",
            (int)a[1].asInteger(), (int)a[2].asInteger(), 20, 20, argStr(a, 3));
        GuiWidget* w = GuiKit::instance().findWidget(a[0].asInteger(), id);
        if (w) { w->group = argStr(a, 4); w->checked = a[5].isTruthy(); w->animCheck = w->checked ? 1.0 : 0.0; }
        return FValue::integer(id);
    }));

    auto itemsWidget = [](const char* kind) {
        return GCNative::NativeFn([kind](const std::vector<FValue>& a) -> FValue {
            if (a.size() < 6) throw std::runtime_error(std::string("gui.") + kind + "() expects window,x,y,w,h,items");
            long long id = GuiKit::instance().addWidget(a[0].asInteger(), kind,
                (int)a[1].asInteger(), (int)a[2].asInteger(),
                (int)a[3].asInteger(), (int)a[4].asInteger(), "");
            GuiWidget* w = GuiKit::instance().findWidget(a[0].asInteger(), id);
            if (w) {
                w->items = splitItems(argStr(a, 5));
                if (!w->items.empty() && w->selected < 0) w->selected = 0;
            }
            return FValue::integer(id);
        });
    };
    m->entries["tabs"]  = FValue::obj(new GCNative(itemsWidget("tabs"), "gui.tabs"));
    m->entries["list"]  = FValue::obj(new GCNative(itemsWidget("list"), "gui.list"));
    m->entries["combo"] = FValue::obj(new GCNative(itemsWidget("combo"), "gui.combo"));

    m->entries["set_value"] = FValue::obj(native("gui.set_value", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 2) throw std::runtime_error("gui.set_value() expects id, value");
        if (GuiWidget* w = findAnywhere(a[0].asInteger()))
            w->value = std::max(0.0, std::min(1.0, a[1].asNumber()));
        return FValue::nil();
    }));

    m->entries["get_value"] = FValue::obj(native("gui.get_value", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 1) throw std::runtime_error("gui.get_value() expects id");
        if (GuiWidget* w = findAnywhere(a[0].asInteger())) return FValue::floating(w->value);
        return FValue::nil();
    }));

    m->entries["set_items"] = FValue::obj(native("gui.set_items", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 2) throw std::runtime_error("gui.set_items() expects id, items");
        if (GuiWidget* w = findAnywhere(a[0].asInteger())) {
            w->items = splitItems(argStr(a, 1));
            if (w->selected >= (int)w->items.size()) w->selected = (int)w->items.size() - 1;
        }
        return FValue::nil();
    }));

    m->entries["select"] = FValue::obj(native("gui.select", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 2) throw std::runtime_error("gui.select() expects id, index");
        if (GuiWidget* w = findAnywhere(a[0].asInteger())) {
            w->selected = (int)a[1].asInteger();
            if (w->kind == "radio") w->checked = true;
        }
        return FValue::nil();
    }));

    m->entries["get_selected"] = FValue::obj(native("gui.get_selected", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 1) throw std::runtime_error("gui.get_selected() expects id");
        if (GuiWidget* w = findAnywhere(a[0].asInteger())) return FValue::integer(w->selected);
        return FValue::nil();
    }));

    // ---- event loop ------------------------------------------------

    m->entries["tick"] = FValue::obj(native("gui.tick", [](const std::vector<FValue>&) -> FValue {
        return FValue::boolean(GuiKit::instance().tick());
    }));

    m->entries["poll"] = FValue::obj(native("gui.poll", [](const std::vector<FValue>&) -> FValue {
        GCMap* ev = GuiKit::instance().poll();
        return ev ? FValue::obj(ev) : FValue::nil();
    }));

    m->entries["set_text"] = FValue::obj(native("gui.set_text", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 2) throw std::runtime_error("gui.set_text() expects id, text");
        for (auto wid : GuiKit::instance().windowIds()) {
            GuiWidget* w = GuiKit::instance().findWidget(wid, a[0].asInteger());
            if (w) { w->text = argStr(a, 1); break; }
        }
        return FValue::nil();
    }));

    m->entries["get_text"] = FValue::obj(native("gui.get_text", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 1) throw std::runtime_error("gui.get_text() expects id");
        for (auto wid : GuiKit::instance().windowIds()) {
            GuiWindow* win = GuiKit::instance().findWindow(wid);
            GuiWidget* w = win ? GuiKit::instance().findWidget(wid, a[0].asInteger()) : nullptr;
            if (w) {
                if (w->kind == "textfield" && win) return FValue::obj(new GCString(win->textFieldBuffer));
                return FValue::obj(new GCString(w->text));
            }
        }
        return FValue::nil();
    }));

    m->entries["set_checked"] = FValue::obj(native("gui.set_checked", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 2) throw std::runtime_error("gui.set_checked() expects id, bool");
        if (GuiWidget* w = findAnywhere(a[0].asInteger())) w->checked = a[1].isTruthy();
        return FValue::nil();
    }));

    m->entries["set_progress"] = FValue::obj(native("gui.set_progress", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 2) throw std::runtime_error("gui.set_progress() expects id, value");
        if (GuiWidget* w = findAnywhere(a[0].asInteger()))
            w->value = std::max(0.0, std::min(1.0, a[1].asNumber()));
        return FValue::nil();
    }));

    m->entries["close"] = FValue::obj(native("gui.close", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 1) throw std::runtime_error("gui.close() expects window id");
        GuiKit::instance().closeWindow(a[0].asInteger());
        return FValue::nil();
    }));

    vm.defineModule("gui", m);
}

} // namespace fvm
} // namespace forge
