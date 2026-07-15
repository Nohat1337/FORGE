// ============================================================
// sdl-fvm : Forge FVM SDL2 default window
//
// Shown when running `forge --sdl` with no .fclass file.
// Displays the Forge logo and a usage message, similar to
// how `java` (javaw) shows the Java logo by default.
// ============================================================

#include "fvm/runtime.hpp"
#include "fvm/sdl2_ui.hpp"
#include <SDL2/SDL.h>
#include <iostream>
#include <string>

namespace forge::fvm {

int runSdlGui(const std::string& screenshotPath) {
    SDL2UI& ui = SDL2UI::instance();
    if (!ui.init(720, 520, "Forge")) {
        std::cerr << "Failed to initialize SDL2 UI\n";
        return 1;
    }

    // Try to load the Forge logo from assets
    SDL_Surface* logoSurface = nullptr;
    const char* logoPaths[] = {
        "assets/icon.bmp",
        "../assets/icon.bmp",
        "assets/forge-icon.bmp",
        "../assets/forge-icon.bmp",
        "/usr/local/share/forge/icon.bmp",
    };
    for (auto path : logoPaths) {
        logoSurface = SDL_LoadBMP(path);
        if (logoSurface) break;
    }

    SDL_Texture* logoTexture = nullptr;
    if (logoSurface && ui.renderer()) {
        logoTexture = SDL_CreateTextureFromSurface(ui.renderer(), logoSurface);
        SDL_FreeSurface(logoSurface);
    }

    bool running = true;
    SDLKeyEvent key;
    SDLMouseEvent mouse;
    SDLWindowEvent win;

    while (running) {
        while (true) {
            bool alive = ui.pumpEvents(&key, &mouse, &win);
            if (!alive || win.closed) { running = false; break; }
            if (win.resized) { /* will redraw */ }
            if (key.keycode == 27) { running = false; break; } // ESC
            break;
        }
        if (!running) break;

        int W, H;
        ui.getSize(W, H);

        // Background — dark
        ui.clear(24, 24, 30);

        // Draw logo centered in upper portion
        int logoSize = 128;
        if (logoTexture) {
            int logoX = (W - logoSize) / 2;
            int logoY = (H / 2) - logoSize - 40;
            SDL_Rect dst{ logoX, logoY, logoSize, logoSize };
            SDL_RenderCopy(ui.renderer(), logoTexture, nullptr, &dst);
        } else {
            // Fallback: draw a colored rectangle as placeholder logo
            int logoX = (W - logoSize) / 2;
            int logoY = (H / 2) - logoSize - 40;
            ui.drawRoundedRect(logoX, logoY, logoSize, logoSize, 16,
                               98, 160, 234);
            // Draw "F" in the center of the box
            ui.drawText(logoX + 52, logoY + 48, "F",
                        255, 255, 255);
        }

        // "Forge" text below logo
        std::string title = "Forge";
        int titleW = ui.textWidth(title);
        ui.drawText((W - titleW) / 2, (H / 2) + 16,
                    title, 220, 220, 230);

        // Version
        std::string version = "Forge Programming Language";
        int verW = ui.textWidth(version);
        ui.drawText((W - verW) / 2, (H / 2) + 36,
                    version, 120, 120, 140);

        // Usage message (like JVM says "Usage: java [options] <mainclass>")
        std::string usage = "Usage: forge --sdl <file.fclass>";
        int usageW = ui.textWidth(usage);
        ui.drawText((W - usageW) / 2, (H / 2) + 64,
                    usage, 160, 160, 180);

        // Error message
        std::string error = "Error: No .fclass file provided";
        int errW = ui.textWidth(error);
        ui.drawText((W - errW) / 2, (H / 2) + 84,
                    error, 230, 80, 80);

        // Hint at bottom
        std::string hint = "Press ESC to quit";
        int hintW = ui.textWidth(hint);
        ui.drawText((W - hintW) / 2, H - 40,
                    hint, 80, 80, 100);

        ui.present();

        // Headless screenshot mode
        if (!screenshotPath.empty()) {
            if (ui.saveScreenshot(screenshotPath))
                std::cerr << "Screenshot saved: " << screenshotPath << "\n";
            else
                std::cerr << "Screenshot failed\n";
            if (logoTexture) SDL_DestroyTexture(logoTexture);
            ui.shutdown();
            return 0;
        }

        ui.delay(16);
    }

    if (logoTexture) SDL_DestroyTexture(logoTexture);
    ui.shutdown();
    return 0;
}

} // namespace forge::fvm
