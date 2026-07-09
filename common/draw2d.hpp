// Tiny 2D primitives (Bresenham / Xiaolin-Wu lines, filled disks) shared by the
// rasterisation-flavoured assignments (A0 transform viz, A1 wireframe, A4 Bezier).
#pragma once
#include "image.hpp"
#include <cmath>

namespace g101 {

inline void draw_line(Image& img, int x0, int y0, int x1, int y1, const Vec3f& c) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        img.set(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Xiaolin Wu anti-aliased line (used to show the smoothness of the Bezier curve).
inline void plot_aa(Image& img, int x, int y, float a, const Vec3f& c) {
    if (x < 0 || y < 0 || x >= img.w || y >= img.h) return;
    Vec3f& dst = img.at(x, y);
    dst = dst * (1.0f - a) + c * a;
}

inline void draw_line_aa(Image& img, float x0, float y0, float x1, float y1, const Vec3f& c) {
    bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
    if (steep) { std::swap(x0, y0); std::swap(x1, y1); }
    if (x0 > x1) { std::swap(x0, x1); std::swap(y0, y1); }
    float dx = x1 - x0, dy = y1 - y0;
    float grad = dx == 0.0f ? 1.0f : dy / dx;
    float inter = y0 + grad * 0.0f;
    for (int x = (int)std::floor(x0); x <= (int)std::ceil(x1); ++x) {
        float y = y0 + grad * (x - x0);
        int iy = (int)std::floor(y);
        float f = y - iy;
        if (steep) { plot_aa(img, iy, x, 1 - f, c); plot_aa(img, iy + 1, x, f, c); }
        else       { plot_aa(img, x, iy, 1 - f, c); plot_aa(img, x, iy + 1, f, c); }
    }
    (void)inter;
}

inline void fill_disk(Image& img, int cx, int cy, int r, const Vec3f& c) {
    for (int y = -r; y <= r; ++y)
        for (int x = -r; x <= r; ++x)
            if (x * x + y * y <= r * r) img.set(cx + x, cy + y, c);
}

}  // namespace g101
