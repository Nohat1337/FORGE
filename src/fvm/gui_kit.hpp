#pragma once

// ============================================================
// Forge GUI Kit - the FVM's widget toolkit (the analogue of
// Swing/AWT on the JVM).
//
// Retained widget tree rendered with SDL2 through SDL2UI.
// Every widget is drawn with the liquid glass lens renderer
// (see liquid_glass.hpp): the widget is a real SDF lens that
// refracts the backdrop, disperses the rim into RGB fringes and
// carries a specular highlight.
//
// Because the interpreter is not re-entrant, callbacks are
// delivered as an event queue consumed from Forge:
//
//   import "gui"
//   let win = gui.window("Demo", 640, 480)
//   let btn = gui.button(win, 20, 20, 180, 48, "Click me")
//   while (gui.tick()) {
//       let ev = gui.poll()
//       while (ev != nil) {
//           if (ev.id == btn && ev.type == "click") { ... }
//           ev = gui.poll()
//       }
//   }
// ============================================================

#include "runtime.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace forge::fvm {

struct GuiEvent {
    long long windowId = 0;
    long long widgetId = 0;
    std::string type;      // "click", "check", "text", "close", "change", "select"
    std::string text;
};

struct GuiWidget {
    long long id = 0;
    std::string kind;      // button label textfield checkbox progress panel
                             // slider gauge toggle radio tabs list combo
    int x = 0, y = 0, w = 0, h = 0;
    std::string text;
    bool checked = false;
    bool hovered = false;
    bool pressed = false;
    double value = 0;          // slider / gauge / progress: 0..1
    double minV = 0, maxV = 1;
    uint8_t bgR = 60, bgG = 62, bgB = 70;

    // liquid glass animation state
    double anim = 0;           // eased value (0..1)
    double animCheck = 0;      // eased checked state
    double pressAnim = 0;      // eased press amount
    double glow = 0;           // eased hover glow

    // selection widgets
    int selected = -1;
    bool open = false;         // combo dropdown open
    std::vector<std::string> items;
    std::string group;         // radio group name
    std::vector<GuiWidget> children;
};

struct GuiWindow {
    long long id = 0;
    std::string title;
    int width = 800, height = 600;
    bool open = true;
    std::string textFieldBuffer;
    long long focusedField = -1;
    uint32_t blinkTick = 0;
    std::vector<GuiWidget> children;
};

class GuiKit {
public:
    static GuiKit& instance();

    long long createWindow(const std::string& title, int w, int h);
    long long addWidget(long long winId, const std::string& kind,
                        int x, int y, int w, int h, const std::string& text);
    GuiWidget* findWidget(long long winId, long long widgetId);
    GuiWindow* findWindow(long long winId);

    // pump events + render one frame; returns false once every window closed
    bool tick();
    // pop next queued event into a GCMap (or nullptr when empty)
    GCMap* poll();
    void pushCloseEventsIfAny();

    void closeWindow(long long winId);
    std::vector<long long> windowIds() const;

private:
    GuiKit() = default;
    struct Impl;
    Impl* impl_ = nullptr;

};

void defineGuiModule(ForgeVM& vm);

} // namespace forge::fvm
