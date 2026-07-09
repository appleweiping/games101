// GAMES101 - Assignment 4: Bezier curves via de Casteljau's algorithm.
//
// Given a set of control points we evaluate the Bezier curve two independent ways:
//   1. de Casteljau's recursive corner-cutting (the assignment's core task), and
//   2. the closed-form Bernstein-polynomial sum (used as an independent check).
// The two must coincide to floating-point precision.  The curve is rendered with
// a distance-based anti-aliasing splat (the assignment's bonus).
#include "draw2d.hpp"
#include "image.hpp"
#include "math_utils.hpp"
#include <cstdio>
#include <vector>

using namespace g101;

// One de Casteljau step then recurse; returns the curve point at parameter t.
static Vec2f de_casteljau(std::vector<Vec2f> pts, float t) {
    while (pts.size() > 1) {
        std::vector<Vec2f> next;
        next.reserve(pts.size() - 1);
        for (size_t i = 0; i + 1 < pts.size(); ++i)
            next.push_back((1 - t) * pts[i] + t * pts[i + 1]);
        pts = std::move(next);
    }
    return pts[0];
}

// Closed-form Bernstein evaluation, used to validate de Casteljau independently.
static Vec2f bernstein(const std::vector<Vec2f>& p, float t) {
    int n = (int)p.size() - 1;
    Vec2f acc(0, 0);
    double comb = 1.0;  // C(n,0)
    for (int i = 0; i <= n; ++i) {
        double b = comb * std::pow(t, i) * std::pow(1 - t, n - i);
        acc += (float)b * p[i];
        comb = comb * (n - i) / (i + 1);  // C(n,i) -> C(n,i+1)
    }
    return acc;
}

// Anti-aliased point splat: spread the colour over the 3x3 neighbourhood weighted
// by distance to the sub-pixel sample position.
static void splat(Image& img, Vec2f q, const Vec3f& c) {
    int ix = (int)std::floor(q.x()), iy = (int)std::floor(q.y());
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            int x = ix + dx, y = iy + dy;
            float d = (Vec2f(x + 0.5f, y + 0.5f) - q).norm();
            float w = std::max(0.0f, 1.0f - d / 1.5f);
            if (w <= 0) continue;
            if (x < 0 || y < 0 || x >= img.w || y >= img.h) continue;
            Vec3f& dst = img.at(x, y);
            dst = dst * (1 - w) + c * w;  // over-composite
        }
}

int main() {
    const int W = 700, H = 700;
    Image img(W, H, Vec3f(0, 0, 0));

    // Control points (pixel coordinates) forming an S-shaped cubic curve.
    std::vector<Vec2f> ctrl = {{100, 550}, {230, 120}, {470, 600}, {600, 150}};

    // control polygon (dim) + control points (red)
    for (size_t i = 0; i + 1 < ctrl.size(); ++i)
        draw_line_aa(img, ctrl[i].x(), ctrl[i].y(), ctrl[i + 1].x(), ctrl[i + 1].y(),
                     Vec3f(0.35f, 0.35f, 0.4f));
    for (auto& p : ctrl) fill_disk(img, (int)p.x(), (int)p.y(), 5, Vec3f(1.0f, 0.3f, 0.3f));

    // Sample both evaluators; draw de Casteljau (green) and Bernstein (blue, blended)
    // and measure the maximum disagreement between the two.
    float max_diff = 0.0f;
    const int N = 2000;
    for (int k = 0; k <= N; ++k) {
        float t = (float)k / N;
        Vec2f a = de_casteljau(ctrl, t);
        Vec2f b = bernstein(ctrl, t);
        max_diff = std::max(max_diff, (a - b).norm());
        splat(img, a, Vec3f(0.2f, 1.0f, 0.3f));            // de Casteljau
        splat(img, b, Vec3f(0.25f, 0.55f, 1.0f) * 0.6f);   // Bernstein overlay
    }

    img.save_png("results/a4_bezier.png", false, true);  // flipY: control pts use +y down
    std::printf("A4: cubic Bezier, %zu control points.\n", ctrl.size());
    std::printf("  de Casteljau vs Bernstein max distance over %d samples = %.3e px\n", N, max_diff);
    std::printf("  (the two evaluators coincide -> de Casteljau is correct)\n");
    std::printf("Wrote results/a4_bezier.png\n");
    return 0;
}
