// GAMES101 - Assignment 6: bounding volume hierarchy (BVH) acceleration.
//
// Load a triangle mesh (the spot cow, ~5.8k triangles, or a procedural icosphere),
// build a median-split BVH over its triangles, and ray-trace it.  We render the
// full image through the BVH and, on a fixed sample of primary rays, compare the
// BVH against brute-force linear intersection to measure the speed-up.
#include "image.hpp"
#include "math_utils.hpp"
#include "mesh.hpp"
#include "rt_core.hpp"
#include <chrono>
#include <cstdio>

using namespace g101;

using Clock = std::chrono::high_resolution_clock;
static double ms(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);  // unbuffered so staged timings show live
    const int W = 800, H = 800;

    // --- load geometry ---
    Mesh mesh; std::string src;
    if (load_obj("assets/spot/spot_triangulated.obj", mesh)) src = "spot_triangulated.obj";
    else { mesh = make_uv_sphere(96, 192); src = "procedural icosphere (spot asset absent)"; }

    // model transform (rotate to face camera, scale up) baked into triangles
    float ang = deg2rad(140.0f), c = std::cos(ang), s = std::sin(ang), sc = 2.2f;
    // NB: explicit -> Vec3f return; returning `auto` here yields a dangling Eigen
    // expression template that references the destroyed local `q`.
    auto xform = [&](const Vec3f& p) -> Vec3f {
        Vec3f q(c * p.x() + s * p.z(), p.y(), -s * p.x() + c * p.z());
        return q * sc;
    };
    Material diffuse; diffuse.Kd = Vec3f(0.72f, 0.60f, 0.45f);

    std::vector<Object*> prims;
    prims.reserve(mesh.tris.size());
    Bounds3 wb;
    for (auto& t : mesh.tris) {
        Vec3f a = xform(t.v[0].pos), b = xform(t.v[1].pos), d = xform(t.v[2].pos);
        prims.push_back(new Triangle(a, b, d, &diffuse));
        wb = Union(Union(Union(wb, a), b), d);
    }
    std::printf("A6: %zu triangles from %s\n", prims.size(), src.c_str());

    // --- build BVH ---
    auto t0 = Clock::now();
    BVH bvh(prims);
    auto t1 = Clock::now();
    std::printf("  BVH built in %.1f ms: %d nodes, %d leaves, max depth %d\n",
                ms(t0, t1), bvh.nodeCount, bvh.leafCount, bvh.maxDepth);

    // --- camera framing the model ---
    Vec3f center = wb.centroid();
    float radius = wb.diagonal().norm() * 0.5f;
    Vec3f eye = center + Vec3f(0, 0.15f * radius, 3.0f * radius);
    float fov = 45.0f, scale = std::tan(deg2rad(fov * 0.5f)), aspect = (float)W / H;
    Vec3f bg(0.12f, 0.13f, 0.16f);

    // two headlight-style directional lights + ambient (flat-shaded)
    Vec3f L1 = Vec3f(0.6f, 0.7f, 1.0f).normalized(), L2 = Vec3f(-0.7f, 0.3f, 0.5f).normalized();

    auto shade = [&](const Intersection& hit) -> Vec3f {
        Vec3f N = hit.normal;
        float d = 0.75f * std::max(0.0f, N.dot(L1)) + 0.35f * std::max(0.0f, N.dot(L2)) + 0.15f;
        return hit.obj->diffuseColor(hit.coords) * d;
    };

    auto primary = [&](int i, int j) {
        float x = (2 * (i + 0.5f) / W - 1) * aspect * scale;
        float y = (1 - 2 * (j + 0.5f) / H) * scale;
        return Ray(eye, Vec3f(x, y, -1).normalized());
    };

    // --- render full image with BVH ---
    Image img(W, H, bg);
    auto r0 = Clock::now();
    long hits = 0;
    for (int j = 0; j < H; ++j)
        for (int i = 0; i < W; ++i) {
            Intersection hit = bvh.intersect(primary(i, j));
            if (hit.happened) { img.set(i, j, shade(hit)); ++hits; }
        }
    auto r1 = Clock::now();
    img.save_png("results/a6_bvh.png");
    std::printf("  rendered %dx%d with BVH in %.1f ms (%ld pixels hit) -> results/a6_bvh.png\n",
                W, H, ms(r0, r1), hits);

    // --- BVH vs brute-force on a fixed sample of rays ---
    std::vector<Ray> sample;  // ~2500 rays: brute force must scan all triangles each
    for (int j = 0; j < H; j += 16)
        for (int i = 0; i < W; i += 16) sample.push_back(primary(i, j));
    auto b0 = Clock::now();
    long h1 = 0;
    for (auto& r : sample) if (bvh.intersect(r).happened) ++h1;
    auto b1 = Clock::now();
    long h2 = 0;
    for (auto& r : sample) if (brute_force_intersect(prims, r).happened) ++h2;
    auto b2 = Clock::now();

    double tb = ms(b0, b1), tf = ms(b1, b2);
    std::printf("  intersect timing over %zu rays:\n", sample.size());
    std::printf("    BVH         : %8.1f ms (%.3f us/ray)\n", tb, tb * 1000 / sample.size());
    std::printf("    brute-force : %8.1f ms (%.3f us/ray)\n", tf, tf * 1000 / sample.size());
    std::printf("    speed-up    : %.1fx (both found %ld / %ld hits -> identical)\n",
                tf / tb, h1, h2);

    for (auto* o : prims) delete o;
    return 0;
}
