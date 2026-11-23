// Эффекты окна: постобработка готового кадра (буфер BGRA g.dibbits).
// Включается через el_effect("имя", сила); несколько эффектов стопкой.
#include "el_internal.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace eli {

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline int lum(u32 p) { int r = (p >> 16) & 255, g2 = (p >> 8) & 255, b = p & 255; return (r * 77 + g2 * 150 + b * 29) >> 8; }
static inline u32 pack(int a, int r, int g, int b) { return ((u32)a << 24) | ((u32)clampi(r, 0, 255) << 16) | ((u32)clampi(g, 0, 255) << 8) | (u32)clampi(b, 0, 255); }

static uint32_t s_rng = 0x1234567;
static inline uint32_t xr() { s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return s_rng; }

static void copy_tmp(u32 *buf, int w, int h) { g.fx_tmp.assign(buf, buf + (size_t)w * h); }

static void fx_chromatic(u32 *buf, int w, int h, int amt) {
    if (amt < 1) return; copy_tmp(buf, w, h); u32 *s = g.fx_tmp.data();
    for (int y = 0; y < h; y++) { u32 *row = buf + (size_t)y * w, *sr = s + (size_t)y * w;
        for (int x = 0; x < w; x++) {
            int r = (sr[clampi(x - amt, 0, w - 1)] >> 16) & 255;
            int b = sr[clampi(x + amt, 0, w - 1)] & 255;
            int g2 = (sr[x] >> 8) & 255, a = (sr[x] >> 24) & 255;
            row[x] = pack(a, r, g2, b);
        } }
}
static void fx_scanlines(u32 *buf, int w, int h, int amt) {
    double f = 1.0 - clampi(amt, 0, 100) / 100.0 * 0.85;
    for (int y = 0; y < h; y += 2) { u32 *row = buf + (size_t)y * w;
        for (int x = 0; x < w; x++) { u32 p = row[x]; int a = (p >> 24) & 255;
            row[x] = pack(a, (int)(((p >> 16) & 255) * f), (int)(((p >> 8) & 255) * f), (int)((p & 255) * f)); } }
}
static void fx_noise(u32 *buf, int w, int h, int amt) {
    s_rng ^= (uint32_t)(g.now_ms) * 2654435761u;
    for (int i = 0; i < w * h; i++) { u32 p = buf[i]; int a = (p >> 24) & 255; if (!a) continue;
        int n = (int)(xr() % (2 * amt + 1)) - amt;
        buf[i] = pack(a, ((p >> 16) & 255) + n, ((p >> 8) & 255) + n, (p & 255) + n); }
}
static void fx_vignette(u32 *buf, int w, int h, int amt) {
    double cx = w / 2.0, cy = h / 2.0, md = std::sqrt(cx * cx + cy * cy), k = clampi(amt, 0, 100) / 100.0;
    for (int y = 0; y < h; y++) { u32 *row = buf + (size_t)y * w;
        for (int x = 0; x < w; x++) { double d = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)) / md;
            double f = 1.0 - k * d * d; if (f < 0) f = 0; u32 p = row[x]; int a = (p >> 24) & 255;
            row[x] = pack(a, (int)(((p >> 16) & 255) * f), (int)(((p >> 8) & 255) * f), (int)((p & 255) * f)); } }
}
static void fx_glitch(u32 *buf, int w, int h, int amt) {
    copy_tmp(buf, w, h); u32 *s = g.fx_tmp.data();
    s_rng = (uint32_t)(g.now_ms / 90) * 2246822519u + 1;      // новый набop каждые ~90мс
    int bands = amt / 14 + 2;
    for (int bi = 0; bi < bands; bi++) {
        int y0 = xr() % h, bh = 6 + xr() % 26; int dx = (int)(xr() % (amt * 2 + 1)) - amt;
        int cs = (int)(xr() % 7) - 3;                         // доп. расщепление канала в полосе
        for (int y = y0; y < y0 + bh && y < h; y++) { u32 *row = buf + (size_t)y * w, *sr = s + (size_t)y * w;
            for (int x = 0; x < w; x++) { int sx = clampi(x - dx, 0, w - 1);
                u32 p = sr[sx]; int a = (p >> 24) & 255;
                int r = (sr[clampi(sx - cs, 0, w - 1)] >> 16) & 255, g2 = (p >> 8) & 255, b = sr[clampi(sx + cs, 0, w - 1)] & 255;
                row[x] = pack(a, r, g2, b); } }
    }
}
static void fx_mix_mono(u32 *buf, int w, int h, int amt, int mode) { // 0 grayscale,1 sepia,2 invert
    double k = clampi(amt, 0, 100) / 100.0;
    for (int i = 0; i < w * h; i++) { u32 p = buf[i]; int a = (p >> 24) & 255; if (!a) continue;
        int r = (p >> 16) & 255, g2 = (p >> 8) & 255, b = p & 255, nr, ng, nb;
        if (mode == 0) { int y = lum(p); nr = ng = nb = y; }
        else if (mode == 1) { int y = lum(p); nr = y * 112 / 100; ng = y * 94 / 100; nb = y * 70 / 100; }
        else { nr = 255 - r; ng = 255 - g2; nb = 255 - b; }
        buf[i] = pack(a, (int)(r + (nr - r) * k), (int)(g2 + (ng - g2) * k), (int)(b + (nb - b) * k)); }
}
static void fx_pixelate(u32 *buf, int w, int h, int amt) {
    int bs = clampi(amt, 2, 64);
    for (int by = 0; by < h; by += bs) for (int bx = 0; bx < w; bx += bs) {
        long r = 0, g2 = 0, b = 0, a = 0, n = 0;
        for (int y = by; y < by + bs && y < h; y++) for (int x = bx; x < bx + bs && x < w; x++) { u32 p = buf[(size_t)y * w + x]; r += (p >> 16) & 255; g2 += (p >> 8) & 255; b += p & 255; a += (p >> 24) & 255; n++; }
        if (!n) continue; u32 avg = pack((int)(a / n), (int)(r / n), (int)(g2 / n), (int)(b / n));
        for (int y = by; y < by + bs && y < h; y++) for (int x = bx; x < bx + bs && x < w; x++) buf[(size_t)y * w + x] = avg;
    }
}
static void fx_bloom(u32 *buf, int w, int h, int amt) {
    int r = clampi(amt / 10, 2, 10);
    std::vector<u32> bright((size_t)w * h);
    for (int i = 0; i < w * h; i++) { u32 p = buf[i]; int y = lum(p); bright[i] = (y > 170) ? p : 0xFF000000; }
    // раздельное размытие (гориз+верт) ярких точек
    std::vector<u32> tmp((size_t)w * h);
    auto blur = [&](std::vector<u32> &src, std::vector<u32> &dst, bool horiz) {
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            long sr = 0, sg = 0, sb = 0; int n = 0;
            for (int k = -r; k <= r; k++) { int xx = horiz ? clampi(x + k, 0, w - 1) : x, yy = horiz ? y : clampi(y + k, 0, h - 1);
                u32 p = src[(size_t)yy * w + xx]; sr += (p >> 16) & 255; sg += (p >> 8) & 255; sb += p & 255; n++; }
            dst[(size_t)y * w + x] = pack(255, (int)(sr / n), (int)(sg / n), (int)(sb / n));
        } };
    blur(bright, tmp, true); blur(tmp, bright, false);
    double k = clampi(amt, 0, 100) / 100.0 * 0.9;
    for (int i = 0; i < w * h; i++) { u32 p = buf[i], gl = bright[i]; int a = (p >> 24) & 255;
        buf[i] = pack(a, ((p >> 16) & 255) + (int)(((gl >> 16) & 255) * k), ((p >> 8) & 255) + (int)(((gl >> 8) & 255) * k), (p & 255) + (int)((gl & 255) * k)); }
}

void apply_effects() {
    if (g.effects.empty() || !g.dibbits) return;
    int w = g.back_w, h = g.back_h; u32 *buf = (u32 *)g.dibbits;
    for (auto &e : g.effects) {
        const std::string &n = e.name; int a = e.amount;
        if (n == "chromatic" || n == "dispersion") fx_chromatic(buf, w, h, a);
        else if (n == "scanlines") fx_scanlines(buf, w, h, a);
        else if (n == "noise") fx_noise(buf, w, h, a);
        else if (n == "vignette") fx_vignette(buf, w, h, a);
        else if (n == "glitch") fx_glitch(buf, w, h, a);
        else if (n == "grayscale") fx_mix_mono(buf, w, h, a, 0);
        else if (n == "sepia") fx_mix_mono(buf, w, h, a, 1);
        else if (n == "invert") fx_mix_mono(buf, w, h, a, 2);
        else if (n == "pixelate") fx_pixelate(buf, w, h, a);
        else if (n == "bloom" || n == "glow") fx_bloom(buf, w, h, a);
    }
    if (effects_animated()) request_redraw();
}

bool effects_animated() { for (auto &e : g.effects) if (e.name == "glitch" || e.name == "noise") return true; return false; }

void el_effect(const char *name, int amount) {
    if (!name) return;
    bool found = false;
    for (auto &e : g.effects) if (e.name == name) { e.amount = amount; found = true; }
    if (!found && amount > 0) g.effects.push_back(Effect{ name, amount });
    for (size_t i = 0; i < g.effects.size();) { if (g.effects[i].amount <= 0) g.effects.erase(g.effects.begin() + i); else i++; }
    request_redraw();
}
void el_effects_clear(void) { g.effects.clear(); request_redraw(); }

} // namespace eli
