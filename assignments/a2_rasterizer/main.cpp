// GAMES101 - Assignment 2: triangle rasterisation with a z-buffer + MSAA.
//
// Two overlapping triangles at different depths are rasterised via bounding-box +
// edge-function inside test, with barycentric depth interpolation resolved by a
// z-buffer.  We render once with a single sample per pixel (aliased) and once with
// 2x2 MSAA (per-subsample colour + depth, then box-filter resolve) to show the
// anti-aliasing on the silhouette and on the triangle-triangle overlap edge.
#include "image.hpp"
#include "math_utils.hpp"
#include <array>
#include <cstdio>
#include <limits>

using namespace g101;

static Mat4f get_view_matrix(const Vec3f& eye) {
    Mat4f v = Mat4f::Identity();
    v(0, 3) = -eye.x(); v(1, 3) = -eye.y(); v(2, 3) = -eye.z();
    return v;
}
static Mat4f get_projection_matrix(float fov, float aspect, float zNear, float zFar) {
    float n = -zNear, f = -zFar;
    float t = std::tan(deg2rad(fov) / 2.0f) * zNear, r = t * aspect;
    Mat4f p2o, os = Mat4f::Identity(), ot = Mat4f::Identity();
    p2o << n,0,0,0, 0,n,0,0, 0,0,n+f,-n*f, 0,0,1,0;
    os(0,0)=1/r; os(1,1)=1/t; os(2,2)=2/(n-f);
    ot(2,3)=-(n+f)/2;
    return os * ot * p2o;
}

// Signed edge function; >0 on one side, <0 on the other.
static float edge(const Vec3f& a, const Vec3f& b, float px, float py) {
    return (px - a.x()) * (b.y() - a.y()) - (py - a.y()) * (b.x() - a.x());
}

// Barycentric weights of point (x,y) w.r.t. screen-space triangle v.
static std::array<float, 3> barycentric(const std::array<Vec3f, 3>& v, float x, float y) {
    float a = edge(v[1], v[2], x, y);
    float b = edge(v[2], v[0], x, y);
    float c = edge(v[0], v[1], x, y);
    float s = a + b + c;
    if (std::abs(s) < 1e-9f) return {-1, -1, -1};
    return {a / s, b / s, c / s};
}

struct Rasterizer {
    int W, H, ss;  // ss = subsamples per axis (1 = no AA, 2 = 2x2 MSAA)
    std::vector<Vec3f> color;
    std::vector<float> depth;
    Vec3f bg;

    Rasterizer(int w, int h, int ss_, Vec3f bg_ = Vec3f::Zero())
        : W(w), H(h), ss(ss_), bg(bg_) {
        color.assign((size_t)W * H * ss * ss, bg);
        depth.assign((size_t)W * H * ss * ss, std::numeric_limits<float>::infinity());
    }

    void raster_triangle(const std::array<Vec3f, 3>& scr, const Vec3f& col) {
        float minx = std::min({scr[0].x(), scr[1].x(), scr[2].x()});
        float maxx = std::max({scr[0].x(), scr[1].x(), scr[2].x()});
        float miny = std::min({scr[0].y(), scr[1].y(), scr[2].y()});
        float maxy = std::max({scr[0].y(), scr[1].y(), scr[2].y()});
        int x0 = std::max(0, (int)std::floor(minx)), x1 = std::min(W - 1, (int)std::ceil(maxx));
        int y0 = std::max(0, (int)std::floor(miny)), y1 = std::min(H - 1, (int)std::ceil(maxy));

        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                for (int sy = 0; sy < ss; ++sy)
                    for (int sx = 0; sx < ss; ++sx) {
                        float px = x + (sx + 0.5f) / ss;
                        float py = y + (sy + 0.5f) / ss;
                        auto bc = barycentric(scr, px, py);
                        if (bc[0] < 0 || bc[1] < 0 || bc[2] < 0) continue;  // outside
                        float z = bc[0] * scr[0].z() + bc[1] * scr[1].z() + bc[2] * scr[2].z();
                        size_t idx = (((size_t)y * W + x) * ss * ss) + (size_t)sy * ss + sx;
                        if (z < depth[idx]) { depth[idx] = z; color[idx] = col; }
                    }
    }

    // Box-filter resolve of the subsamples; returns the number of pixels whose
    // subsamples disagree (i.e. anti-aliased edge pixels).
    Image resolve(int& aa_pixels) const {
        Image img(W, H, bg);
        aa_pixels = 0;
        int n = ss * ss;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                Vec3f acc = Vec3f::Zero();
                const Vec3f& first = color[(((size_t)y * W + x) * n)];
                bool mixed = false;
                for (int s = 0; s < n; ++s) {
                    const Vec3f& c = color[(((size_t)y * W + x) * n) + s];
                    acc += c;
                    if ((c - first).squaredNorm() > 1e-8f) mixed = true;
                }
                img.set(x, H - 1 - y, acc / n);  // flip y for PNG (+y up)
                if (mixed) ++aa_pixels;
            }
        return img;
    }
};

int main() {
    const int W = 700, H = 700;
    std::vector<Vec3f> pos = {
        {2, 0, -2}, {0, 2, -2}, {-2, 0, -2},
        {3.5f, -1, -5}, {2.5f, 1.5f, -5}, {-1, 0.5f, -5}};
    std::vector<std::array<int, 3>> ind = {{0, 1, 2}, {3, 4, 5}};
    std::vector<Vec3f> cols = {
        Vec3f(217, 238, 185) / 255.0f, Vec3f(185, 217, 238) / 255.0f};

    Mat4f mvp = get_projection_matrix(45.0f, 1.0f, 0.1f, 50.0f) *
                get_view_matrix(Vec3f(0, 0, 5)) * Mat4f::Identity();
    float f1 = (50.0f - 0.1f) / 2.0f, f2 = (50.0f + 0.1f) / 2.0f;

    auto to_screen = [&](int i) {
        Vec4f c = mvp * Vec4f(pos[i].x(), pos[i].y(), pos[i].z(), 1.0f);
        c /= c.w();
        return Vec3f(0.5f * W * (c.x() + 1.0f), 0.5f * H * (c.y() + 1.0f), c.z() * f1 + f2);
    };

    auto render = [&](int ss, const char* path) {
        Rasterizer r(W, H, ss);
        for (auto& t : ind) {
            std::array<Vec3f, 3> scr = {to_screen(t[0]), to_screen(t[1]), to_screen(t[2])};
            r.raster_triangle(scr, cols[&t - &ind[0]]);
        }
        int aa = 0;
        r.resolve(aa).save_png(path);
        return aa;
    };

    std::printf("A2: two triangles (green z=-2 in front, blue z=-5 behind), %dx%d.\n", W, H);
    int aa0 = render(1, "results/a2_no_aa.png");
    std::printf("  no-AA   -> results/a2_no_aa.png (single sample/pixel)\n");
    int aa2 = render(2, "results/a2_msaa.png");
    std::printf("  2x2 MSAA-> results/a2_msaa.png : %d anti-aliased (partially covered) edge pixels\n", aa2);
    std::printf("  (no-AA has %d such pixels — MSAA blends the silhouette + overlap edge)\n", aa0);
    return 0;
}
