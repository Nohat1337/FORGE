#pragma once

// ============================================================
// Liquid Glass renderer (desktop / SDL2).
//
// A port of the lens model used by the Android Liquid Glass library
// (github.com/QWEA0/Liquid-Glass-Android, MIT): a real SDF lens that
// refracts the backdrop, disperses the rim into RGB fringes, carries a
// specular rim highlight and tints the material.
//
//   backdrop capture -> separable blur -> per-pixel lens pass
//     1. rounded-rect SDF (signed distance field) of the shape
//     2. coverage: SDF-based anti-aliasing, alpha = 0 outside the shape
//     3. normal: numeric gradient of the SDF -> outward screen normal
//     4. thickness profile: inverse-power (gravitational lens) falloff
//        inside the bevel band, faint magnification tail in the interior
//     5. refraction: sample the backdrop inward along the normal
//     6. dispersion: per-channel refraction scale (1 -/+ dispersion)
//     7. vibrancy: saturation curve on the refracted colour
//     8. specular rim: two symmetric lobes, pow 4.5, 30 deg off horizontal
//
// One backdrop capture + blur per frame; every lens then samples it.
// ============================================================

#include <SDL2/SDL.h>
#include <cstdint>
#include <vector>

namespace forge::fvm {

// Material + optics for one glass surface.
struct GlassStyle {
    // shape
    int   cornerRadius   = 18;      // px
    int   bevelWidth     = 46;      // px, edge band that carries the bend
    // optics
    float refractionHeight = 150.0f; // px, displacement at the rim
    float refractionFalloff = 2.0f;  // >0 inverse-power profile, 0 = square bevel
    float dispersion      = 0.10f;   // chromatic fringing at the rim
    float blurRadius      = 10.0f;   // backdrop blur (px)
    // material
    uint8_t tintR = 255, tintG = 255, tintB = 255;
    float tintStrength = 0.10f;      // 0 = colourless glass
    float vibrancy     = 0.22f;      // saturation boost of the refracted backdrop
    float brightness   = 1.04f;      // material brightening
    float specular     = 1.0f;       // rim highlight strength
    float shadow       = 0.40f;      // drop shadow strength
    float pressScale   = 0.96f;      // scale while pressed (<1 shrinks)
    // light direction (screen space, 30 deg off horizontal, from top-left)
    float lightX = -0.866f, lightY = -0.5f;
};

class GlassEngine {
public:
    // Capture the framebuffer as the backdrop for this frame and blur it.
    // Call once per frame, after the background has been drawn.
    void beginFrame(SDL_Renderer* renderer, int width, int height);

    // Render one glass lens. `press` is 0..1 (1 = fully pressed).
    void drawLens(SDL_Renderer* renderer, int x, int y, int w, int h,
                  const GlassStyle& style, float press = 0.0f, float time = 0.0f);

    bool hasBackdrop() const { return !backdrop_.empty(); }

private:
    std::vector<uint8_t> backdrop_;   // RGBA, the captured frame
    std::vector<uint8_t> blurred_;    // RGBA, blurred copy
    int bw_ = 0, bh_ = 0;

    // reusable upload surface
    SDL_Surface* surf_ = nullptr;
    SDL_Texture* tex_ = nullptr;
    int surfW_ = 0, surfH_ = 0;

    void ensureSurface(int w, int h);
    void blur(int radius);
    void sampleBlur(int x, int y, float* r, float* g, float* b) const;
};

} // namespace forge::fvm
