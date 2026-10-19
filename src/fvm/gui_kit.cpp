#include "gui_kit.hpp"
#include "sdl2_ui.hpp"
#include <SDL2/SDL_keycode.h>
#include <algorithm>
#include <deque>
#include <cstring>

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
};

GuiState& S() {
    static GuiState st;
    return st;
}

SDL2UI& ui() {
    if (!S().ui) S().ui = &SDL2UI::instance();
    return *S().ui;
}

constexpr uint8_t WIN_BG_R = 24, WIN_BG_G = 26, WIN_BG_B = 32;
constexpr uint8_t ACCENT_R = 0, ACCENT_G = 122, ACCENT_B = 204;
constexpr uint8_t WIDGET_R = 55, WIDGET_G = 58, WIDGET_B = 66;

void queueEvent(const GuiEvent& e) { S().events.push_back(e); }

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
// Rendering
// ============================================================

static void drawButtonLike(SDL2UI& u, GuiWidget& w, bool accent) {
    if (w.pressed)
        u.drawRoundedRect(w.x + 2, w.y + 2, w.w - 4, w.h - 4, 8,
                          ACCENT_R * 0.7f, ACCENT_G * 0.7f, ACCENT_B * 0.7f);
    else if (accent || w.hovered)
        u.drawRoundedRect(w.x, w.y, w.w, w.h, 8, ACCENT_R, ACCENT_G, ACCENT_B);
    else
        u.drawRoundedRect(w.x, w.y, w.w, w.h, 8, WIDGET_R, WIDGET_G, WIDGET_B);

    if (!w.pressed && !accent)
        u.drawRoundedRectOutline(w.x, w.y, w.w, w.h, 8, 90, 94, 105);

    int tx = w.x + (w.w - (int)w.text.size() * 8) / 2;
    int ty = w.y + (w.h - 8) / 2;
    u.drawText(tx, ty, w.text, 235, 236, 240);
}

void GuiKit::renderWidget(GuiWindow& win, GuiWidget& w) {
    SDL2UI& u = ui();

    if (w.kind == "button") {
        drawButtonLike(u, w, false);
    } else if (w.kind == "label") {
        u.drawText(w.x, w.y, w.text, 225, 227, 233);
    } else if (w.kind == "textfield") {
        bool focused = win.focusedField == w.id;
        u.drawRoundedRect(w.x, w.y, w.w, w.h, 6, focused ? 34 : 44, focused ? 36 : 46, focused ? 42 : 54);
        u.drawRoundedRectOutline(w.x, w.y, w.w, w.h, 6,
                                 focused ? ACCENT_R : 90,
                                 focused ? ACCENT_G : 94,
                                 focused ? ACCENT_B : 105);
        u.drawText(w.x + 8, w.y + (w.h - 8) / 2, win.textFieldBuffer, 230, 232, 238);
        // caret blink on the focused field
        if (focused && (u.ticks() / 500) % 2 == 0) {
            int cw = (int)win.textFieldBuffer.size() * 8;
            u.drawRect(w.x + 9 + cw, w.y + 4, 2, w.h - 8, 220, 222, 230);
        }
    } else if (w.kind == "checkbox") {
        u.drawRoundedRect(w.x, w.y, 18, 18, 4, WIDGET_R, WIDGET_G, WIDGET_B);
        u.drawRoundedRectOutline(w.x, w.y, 18, 18, 4, 110, 114, 124);
        if (w.checked) {
            u.drawRect(w.x + 4, w.y + 8, 4, 4, ACCENT_R, ACCENT_G, ACCENT_B);
            u.drawRect(w.x + 7, w.y + 10, 4, 4, ACCENT_R, ACCENT_G, ACCENT_B);
            u.drawRect(w.x + 10, w.y + 5, 4, 9, ACCENT_R, ACCENT_G, ACCENT_B);
        }
        u.drawText(w.x + 26, w.y + 5, w.text, 225, 227, 233);
    } else if (w.kind == "progress") {
        u.drawRoundedRect(w.x, w.y, w.w, w.h, 6, 40, 42, 50);
        int fillW = (int)(w.w * std::max(0.0, std::min(1.0, w.value)));
        if (fillW > 0)
            u.drawGradientV(w.x, w.y, fillW, w.h, 40, 110, 190, ACCENT_R, ACCENT_G, ACCENT_B);
        u.drawRoundedRectOutline(w.x, w.y, w.w, w.h, 6, 90, 94, 105);
        std::string pct = std::to_string((int)(w.value * 100)) + "%";
        u.drawText(w.x + w.w / 2 - (int)pct.size() * 4, w.y + (w.h - 8) / 2, pct, 235, 236, 240);
    } else if (w.kind == "panel") {
        u.drawRoundedRect(w.x, w.y, w.w, w.h, 8, w.bgR, w.bgG, w.bgB);
        for (auto& c : w.children) renderWidget(win, c);
    }
}

void GuiKit::renderWindow(GuiWindow& win) {
    SDL2UI& u = ui();
    u.clear(WIN_BG_R, WIN_BG_G, WIN_BG_B);

    // title bar with subtle gradient
    u.drawGradientV(0, 0, win.width, 30, 38, 41, 50, 28, 30, 37);
    u.drawText(12, 11, win.title, 200, 203, 212);
    // fake window controls (traffic lights)
    u.drawCircle(win.width - 60, 15, 6, 255, 95, 86);
    u.drawCircle(win.width - 40, 15, 6, 255, 189, 46);
    u.drawCircle(win.width - 20, 15, 6, 39, 201, 63);
    u.drawLine(0, 30, win.width, 30, 48, 51, 60);

    for (auto& w : win.children) renderWidget(win, w);
}

// ============================================================
// Event pump / frame tick
// ============================================================

bool GuiKit::tick() {
    auto& st = S();
    bool anyOpen = false;
    for (auto& [id, win] : st.windows) if (win.open) anyOpen = true;
    if (!anyOpen) return false;

    SDLKeyEvent key;
    SDLMouseEvent mouse;
    SDLWindowEvent winev;
    bool alive = ui().pumpEvents(&key, &mouse, &winev);
    // pumpEvents returns false only when the OS window was closed (SDL_QUIT)
    if (!alive || winev.closed) {
        bool queued = false;
        for (auto& [id, win] : st.windows) {
            if (win.open) {
                win.open = false;
                queueEvent({id, 0, "close", ""});
                queued = true;
            }
        }
        (void)queued;
        return false;
    }

    // v1 renders a single OS window: the most recently created Forge window.
    // Input is routed only there — otherwise every window with a focused
    // textfield would receive the same keystrokes.
    GuiWindow* top = nullptr;
    for (auto& [id, win] : st.windows)
        if (win.open && (!top || id > top->id)) top = &win;
    if (!top) return false;

    if (key.keycode != 0 && key.ch >= 32 && key.ch < 127) {
        if (top->focusedField >= 0) {
            top->textFieldBuffer += key.ch;
            queueEvent({top->id, top->focusedField, "text", top->textFieldBuffer});
        }
    }
    if (key.keycode == SDLK_BACKSPACE) {
        if (top->focusedField >= 0 && !top->textFieldBuffer.empty()) {
            top->textFieldBuffer.pop_back();
            queueEvent({top->id, top->focusedField, "text", top->textFieldBuffer});
        }
    }
    if (mouse.pressed || mouse.released || mouse.motion) {
        for (auto& w : top->children) {
            bool inside = mouse.x >= w.x && mouse.x < w.x + w.w &&
                          mouse.y >= w.y && mouse.y < w.y + w.h;
            bool hitExtra = w.kind == "checkbox" &&
                mouse.x >= w.x && mouse.x < w.x + 26 + (int)w.text.size() * 8 &&
                mouse.y >= w.y && mouse.y < w.y + std::max(20, w.h);
            inside = inside || hitExtra;
            if (w.kind == "button" || w.kind == "checkbox") w.hovered = inside;

            if (inside && mouse.pressed && mouse.button == 1) {
                w.pressed = true;
            } else if (w.pressed && mouse.released && mouse.button == 1) {
                w.pressed = false;
                if (inside) {
                    if (w.kind == "button")
                        queueEvent({top->id, w.id, "click", ""});
                    else if (w.kind == "checkbox") {
                        w.checked = !w.checked;
                        queueEvent({top->id, w.id, "check", w.checked ? "true" : "false"});
                    }
                }
            } else if (!mouse.pressed) {
                w.pressed = false;
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
    }

    // render the top-most open window
    renderWindow(*top);
    ui().present();
    ui().delay(16);   // cap ~60fps so we don't starve the CPU

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

    // label is flexible: (win,x,y,text) or (win,x,y,w,h,text)
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
        if (w) w->checked = a[4].isTruthy();
        return FValue::integer(id);
    }));

    m->entries["progress"] = FValue::obj(native("gui.progress", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 6) throw std::runtime_error("gui.progress() expects window,x,y,w,h,value");
        long long id = GuiKit::instance().addWidget(a[0].asInteger(), "progress",
            (int)a[1].asInteger(), (int)a[2].asInteger(),
            (int)a[3].asInteger(), (int)a[4].asInteger(), "");
        double v = a[5].asNumber();
        GuiWidget* w = GuiKit::instance().findWidget(a[0].asInteger(), id);
        if (w) w->value = std::max(0.0, std::min(1.0, v));
        return FValue::integer(id);
    }));

    m->entries["tick"] = FValue::obj(native("gui.tick", [](const std::vector<FValue>&) -> FValue {
        return FValue::boolean(GuiKit::instance().tick());
    }));

    m->entries["poll"] = FValue::obj(native("gui.poll", [](const std::vector<FValue>&) -> FValue {
        GCMap* ev = GuiKit::instance().poll();
        return ev ? FValue::obj(ev) : FValue::nil();
    }));

    m->entries["set_text"] = FValue::obj(native("gui.set_text", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 2) throw std::runtime_error("gui.set_text() expects id, text");
        // search all windows for the widget
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
        for (auto wid : GuiKit::instance().windowIds()) {
            GuiWidget* w = GuiKit::instance().findWidget(wid, a[0].asInteger());
            if (w) { w->checked = a[1].isTruthy(); break; }
        }
        return FValue::nil();
    }));

    m->entries["set_progress"] = FValue::obj(native("gui.set_progress", [](const std::vector<FValue>& a) -> FValue {
        if (a.size() < 2) throw std::runtime_error("gui.set_progress() expects id, value");
        double v = a[1].asNumber();
        for (auto wid : GuiKit::instance().windowIds()) {
            GuiWidget* w = GuiKit::instance().findWidget(wid, a[0].asInteger());
            if (w) { w->value = std::max(0.0, std::min(1.0, v)); break; }
        }
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
