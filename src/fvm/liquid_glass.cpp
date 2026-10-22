#include "liquid_glass.hpp"

#include <algorithm>
#include <cmath>

namespace forge::fvm {

namespace {

inline float sdRoundBox(float px, float py, float bx, float by, float r) {
    float qx = std::fabs(px) - bx;
    float qy = std::fabs(py) - by;
    float ox = std::max(qx, 0.0f);
    float oy = std::max(qy, 0.0f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - r;
}

inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

inline uint8_t toByte(float v) {
    int i = (int)(v + 0.5f);
    if (i < 0) i = 0;
    if (i > 255) i = 255;
    return (uint8_t)i;
}

} // namespace

// ---------------------------------------------------------------
// backdrop capture + blur
// ---------------------------------------------------------------

void GlassEngine::beginFrame(SDL_Renderer* renderer, int width, int height) {
    if (!renderer || width <= 0 || height <= 0) return;
    if (bw_ != width || bh_ != height) {
        bw_ = width;
        bh_ = height;
        backdrop_.assign((size_t)bw_ * bh_ * 4, 0);
        blurred_.assign((size_t)bw_ * bh_ * 4, 0);
    }
    int pitch = bw_ * 4;
    SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32, backdrop_.data(), pitch);
    blur(10);
}

void GlassEngine::blur(int radius) {
    if (backdrop_.empty() || radius <= 0) { blurred_ = backdrop_; return; }
    blurred_ = backdrop_;
    const int w = bw_, h = bh_;
    // three box passes ~ gaussian
    int r = std::max(1, radius / 3);
    std::vector<uint8_t> tmp((size_t)w * h * 4, 0);
    for (int pass = 0; pass < 3; pass++) {
        // horizontal
        for (int y = 0; y < h; y++) {
            const uint8_t* src = &blurred_[(size_t)y * w * 4];
            uint8_t* dst = &tmp[(size_t)y * w * 4];
            int sum[4] = { 0, 0, 0, 0 };
            for (int i = -r; i <= r; i++) {
                int xx = std::clamp(i, 0, w - 1);
                sum[0] += src[xx * 4 + 0];
                sum[1] += src[xx * 4 + 1];
                sum[2] += src[xx * 4 + 2];
                sum[3] += src[xx * 4 + 3];
            }
            int n = r * 2 + 1;
            for (int x = 0; x < w; x++) {
                dst[x * 4 + 0] = (uint8_t)(sum[0] / n);
                dst[x * 4 + 1] = (uint8_t)(sum[1] / n);
                dst[x * 4 + 2] = (uint8_t)(sum[2] / n);
                dst[x * 4 + 3] = (uint8_t)(sum[3] / n);
                // slide the window using the source row
                const uint8_t* rem = src + (size_t)std::clamp(x - r, 0, w - 1) * 4;
                const uint8_t* add = src + (size_t)std::clamp(x + r + 1, 0, w - 1) * 4;
                sum[0] += add[0] - rem[0];
                sum[1] += add[1] - rem[1];
                sum[2] += add[2] - rem[2];
                sum[3] += add[3] - rem[3];
            }
        }
        // vertical
        for (int x = 0; x < w; x++) {
            int sum[4] = { 0, 0, 0, 0 };
            for (int i = -r; i <= r; i++) {
                int yy = std::clamp(i, 0, h - 1);
                const uint8_t* p = &tmp[((size_t)yy * w + x) * 4];
                sum[0] += p[0]; sum[1] += p[1]; sum[2] += p[2]; sum[3] += p[3];
            }
            int n = r * 2 + 1;
            for (int y = 0; y < h; y++) {
                uint8_t* dst = &blurred_[((size_t)y * w + x) * 4];
                for (int c = 0; c < 4; c++) dst[c] = (uint8_t)(sum[c] / n);
                const uint8_t* pr = &tmp[((size_t)std::clamp(y - r, 0, h - 1) * w + x) * 4];
                const uint8_t* pa = &tmp[((size_t)std::clamp(y + r + 1, 0, h - 1) * w + x) * 4];
                for (int c = 0; c < 4; c++) sum[c] += pa[c] - pr[c];
            }
        }
    }
}

void GlassEngine::sampleBlur(int x, int y, float* r, float* g, float* b) const {
    if (blurred_.empty()) { *r = *g = *b = 0.0f; return; }
    int xx = std::clamp(x, 0, bw_ - 1);
    int yy = std::clamp(y, 0, bh_ - 1);
    const uint8_t* p = &blurred_[((size_t)yy * bw_ + xx) * 4];
    *r = p[0]; *g = p[1]; *b = p[2];
}

void GlassEngine::ensureSurface(int w, int h) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (surf_ && surfW_ == w && surfH_ == h) return;
    if (surf_) SDL_FreeSurface(surf_);
    if (tex_) SDL_DestroyTexture(tex_);
    surf_ = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    surfW_ = w;
    surfH_ = h;
    tex_ = nullptr;
}

// ---------------------------------------------------------------
// the lens pass
// ---------------------------------------------------------------

void GlassEngine::drawLens(SDL_Renderer* renderer, int x, int y, int w, int h,
                            const GlassStyle& style, float press, float time) {
    if (!renderer || w <= 0 || h <= 0) return;

    const int pad = 18;
    const int sw = w + pad * 2;
    const int sh = h + pad * 2;
    ensureSurface(sw, sh);

    // press: shrink the lens around its centre
    float scale = 1.0f - (1.0f - style.pressScale) * clamp01(press);
    float cx = w * 0.5f, cy = h * 0.5f;

    float radius = (float)std::min(style.cornerRadius, (int)std::min(w, h) / 2);
    float bevel = std::max(4.0f, (float)style.bevelWidth);
    if (bevel > radius * 2.0f) bevel = radius * 2.0f;
    float k = bevel / 4.0f;
    float falloff = style.refractionFalloff;
    float disp = style.dispersion;
    float refrH = style.refractionHeight;

    // light (points toward the light source)
    float lx = style.lightX, ly = style.lightY;
    float llen = std::sqrt(lx * lx + ly * ly);
    if (llen > 0.0001f) { lx /= llen; ly /= llen; }

    uint8_t* out = (uint8_t*)surf_->pixels;
    const int surfPitch = surf_->pitch;

    auto sdfAt = [&](float qx, float qy) {
        // undo the press scale so the shape shrinks smoothly
        float sx = (qx - cx) / scale + cx;
        float sy = (qy - cy) / scale + cy;
        float bx = std::max(cx - radius, 0.0f);
        float by = std::max(cy - radius, 0.0f);
        return sdRoundBox(sx - cx, sy - cy, bx, by, radius);
    };

    for (int py = 0; py < sh; py++) {
        for (int px = 0; px < sw; px++) {
            float qx = (float)px - (float)pad;
            float qy = (float)py - (float)pad;
            float d = sdfAt(qx, qy);
            float cov = clamp01(0.5f - d);
            uint8_t* px4 = out + (size_t)py * surfPitch + (size_t)px * 4;

            if (cov <= 0.0f) {
                // soft drop shadow outside the shape
                float shA = style.shadow * 0.55f * std::exp(-std::max(d, 0.0f) / 7.0f);
                if (shA < 0.004f) { px4[0] = px4[1] = px4[2] = px4[3] = 0; continue; }
                px4[0] = 0; px4[1] = 0; px4[2] = 0;
                px4[3] = toByte(shA * 255.0f);
                continue;
            }

            // ---- normal from the SDF gradient
            float gx = sdfAt(qx + 0.75f, qy) - sdfAt(qx - 0.75f, qy);
            float gy = sdfAt(qx, qy + 0.75f) - sdfAt(qx, qy - 0.75f);
            float gl = std::sqrt(gx * gx + gy * gy);
            float nx = 0.0f, ny = 0.0f;
            if (gl > 1e-5f) { nx = gx / gl; ny = gy / gl; }

            float edgeDist = -d;                        // distance inside the shape
            float gd = std::min(edgeDist, bevel);
            float prof = (falloff > 0.0f) ? std::pow(1.0f + gd / k, -falloff)
                                          : std::pow(1.0f - gd / bevel, 2.0f);
            float D = refrH * prof;

            // ---- refraction: sample inward along the normal
            float sx = qx + x, sy = qy + y;             // screen position
            float invX = -nx * D, invY = -ny * D;
            float rr, gg, bb;
            if (backdrop_.empty()) {
                rr = gg = bb = 30.0f;
            } else {
                int ix = (int)std::lround(sx + invX * (1.0f + disp));
                int iy = (int)std::lround(sy + invY * (1.0f + disp));
                sampleBlur(ix, iy, &rr, &gg, &bb);
            }
            float br, bg, bb2;
            if (backdrop_.empty()) { br = bg = bb2 = 30.0f; }
            else {
                int ix = (int)std::lround(sx + invX);
                int iy = (int)std::lround(sy + invY);
                sampleBlur(ix, iy, &br, &bg, &bb2);
            }
            float brr, bgg, bbb;
            if (backdrop_.empty()) { brr = bgg = bbb = 30.0f; }
            else {
                int ix = (int)std::lround(sx + invX * (1.0f - disp));
                int iy = (int)std::lround(sy + invY * (1.0f - disp));
                sampleBlur(ix, iy, &brr, &bgg, &bbb);
            }

            // frosted base (blurred at the pixel) mixed with the refracted sample
            float fr, fg, fb;
            sampleBlur((int)std::lround(sx), (int)std::lround(sy), &fr, &fg, &fb);
            float mixR = fr * 0.55f + rr * 0.45f;
            float mixG = fg * 0.55f + gg * 0.45f;
            float mixB = fb * 0.55f + bb2 * 0.45f;

            // ---- vibrancy (saturation curve) + brightness
            float luma = 0.2126f * mixR + 0.7152f * mixG + 0.0722f * mixB;
            float sat = 1.0f + style.vibrancy;
            mixR = luma + (mixR - luma) * sat;
            mixG = luma + (mixG - luma) * sat;
            mixB = luma + (mixB - luma) * sat;
            mixR *= style.brightness;
            mixG *= style.brightness;
            mixB *= style.brightness;

            // ---- tint
            mixR = mixR * (1.0f - style.tintStrength) + style.tintR * style.tintStrength;
            mixG = mixG * (1.0f - style.tintStrength) + style.tintG * style.tintStrength;
            mixB = mixB * (1.0f - style.tintStrength) + style.tintB * style.tintStrength;

            // ---- specular rim: two symmetric lobes, pow 4.5, hugging the edge
            float lobe1 = std::pow(std::max(nx * lx + ny * ly, 0.0f), 4.5f);
            float lobe2 = std::pow(std::max(-(nx * lx + ny * ly), 0.0f), 4.5f);
            float rimMask = std::exp(-edgeDist / 2.6f);
            float glow = lobe1 * std::exp(-edgeDist / 7.0f) * 0.30f;
            float spec = (lobe1 + lobe2 * 0.55f) * rimMask * style.specular + glow;
            spec *= cov;

            px4[0] = toByte(mixR + spec * 210.0f);
            px4[1] = toByte(mixG + spec * 220.0f);
            px4[2] = toByte(mixB + spec * 235.0f);
            px4[3] = toByte(cov * 255.0f);
        }
    }

    // upload + blit
    if (!tex_) {
        tex_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                 SDL_TEXTUREACCESS_STATIC, surfW_, surfH_);
        if (!tex_) return;
        SDL_SetTextureBlendMode(tex_, SDL_BLENDMODE_BLEND);
    }
    SDL_UpdateTexture(tex_, nullptr, surf_->pixels, surf_->pitch);
    SDL_Rect dst{ x - pad, y - pad, surfW_, surfH_ };
    SDL_RenderCopy(renderer, tex_, nullptr, &dst);
}

} // namespace forge::fvm
