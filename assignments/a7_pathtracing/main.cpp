// GAMES101 - Assignment 7: Monte-Carlo path tracing (global illumination).
//
// The classic Cornell box, generated procedurally with the standard coordinates:
// white floor/ceiling/back wall, red left wall, green right wall, an emissive
// ceiling light, and a tall + short white block.  Each pixel integrates many paths
// with next-event estimation (direct light sampling) + uniform-hemisphere indirect
// bounces terminated by Russian roulette.  Renders colour bleeding, soft shadows
// and diffuse inter-reflection.  Parallelised with OpenMP.
#include "image.hpp"
#include "math_utils.hpp"
#include "rt_core.hpp"
#include <cstdio>
#include <chrono>

using namespace g101;

// ---- scene container: primitives + emissive subset + BVH ----
struct Scene {
    std::vector<Object*> prims;
    std::vector<Object*> lights;  // emissive prims
    float lightArea = 0;
    BVH* bvh = nullptr;

    void add(Object* o) {
        prims.push_back(o);
        if (o->hasEmit()) { lights.push_back(o); lightArea += o->area(); }
    }
    void build() { bvh = new BVH(prims); }

    Intersection intersect(const Ray& r) const { return bvh->intersect(r); }

    // Sample a point uniformly over the total emissive area; pdf = 1/lightArea.
    void sampleLight(Intersection& pos, float& pdf) const {
        float p = get_random_float() * lightArea, acc = 0;
        for (Object* e : lights) {
            acc += e->area();
            if (p <= acc) { float d; e->sample(pos, d); break; }
        }
        pdf = 1.0f / lightArea;
    }
};

static Vec3f faceforward(const Vec3f& n, const Vec3f& v) {
    return n.dot(v) < 0 ? Vec3f(-n) : n;
}

// Recursive path radiance from point p, with wo pointing back toward the previous
// vertex (camera).  N is the surface normal, mat the surface material.
static Vec3f shade(const Scene& sc, const Vec3f& p, const Vec3f& wo,
                   const Vec3f& Nin, Material* mat, int depth) {
    const float RR = 0.8f;
    Vec3f N = faceforward(Nin, wo);

    // --- direct lighting: sample a point on the light ---
    Vec3f L_dir = Vec3f::Zero();
    Intersection lp; float pdf_l;
    sc.sampleLight(lp, pdf_l);
    Vec3f x = lp.coords, NN = faceforward(lp.normal, p - x);
    Vec3f d = x - p; float dist2 = d.squaredNorm(); Vec3f ws = d.normalized();
    Vec3f shadowOrig = p + N * 0.5f;
    Intersection block = sc.intersect(Ray(shadowOrig, ws));
    // The nearest hit along the shadow ray is either the light itself (unoccluded)
    // or an opaque occluder.  Comparing distances is fragile because the origin is
    // offset off the surface; testing emissivity of the nearest hit is robust.
    if (block.happened && block.m && block.m->hasEmission()) {
        Vec3f f_r = mat->eval(ws, wo, N);
        float cos_p = std::max(0.0f, N.dot(ws));
        float cos_l = std::max(0.0f, NN.dot(-ws));
        if (pdf_l > 1e-8f)
            L_dir = cmul(lp.emit, f_r) * (cos_p * cos_l / dist2 / pdf_l);
    }

    // --- indirect lighting: Russian roulette ---
    Vec3f L_indir = Vec3f::Zero();
    if (depth < 20 && get_random_float() < RR) {
        Vec3f wi = mat->sample(wo, N).normalized();
        float cos_i = N.dot(wi);
        if (cos_i > 0) {
            Ray next(p + N * 0.5f, wi);
            Intersection hit = sc.intersect(next);
            if (hit.happened && !(hit.m && hit.m->hasEmission())) {
                float pdf = mat->pdf(wi, wo, N);
                if (pdf > 1e-8f) {
                    Vec3f f_r = mat->eval(wi, wo, N);
                    L_indir = cmul(shade(sc, hit.coords, -wi, hit.normal, hit.m, depth + 1), f_r) *
                              (cos_i / pdf / RR);
                }
            }
        }
    }
    return L_dir + L_indir;
}

static Vec3f trace(const Scene& sc, const Ray& ray) {
    Intersection inter = sc.intersect(ray);
    if (!inter.happened) return Vec3f::Zero();
    if (inter.m && inter.m->hasEmission()) return inter.emit;  // camera sees the light
    return shade(sc, inter.coords, -ray.dir, inter.normal, inter.m, 0);
}

// add an axis quad a-b-c-d as two triangles
static void add_quad(Scene& sc, const Vec3f& a, const Vec3f& b, const Vec3f& c,
                     const Vec3f& d, Material* m) {
    sc.add(new Triangle(a, b, c, m));
    sc.add(new Triangle(a, c, d, m));
}
// axis-aligned-ish box from 4 top corners (t0..t3, y=height) down to y=0
static void add_box(Scene& sc, const Vec3f t[4], Material* m) {
    Vec3f b[4];
    for (int i = 0; i < 4; ++i) b[i] = Vec3f(t[i].x(), 0, t[i].z());
    add_quad(sc, t[0], t[1], t[2], t[3], m);                 // top
    for (int i = 0; i < 4; ++i) {                            // 4 sides
        int j = (i + 1) % 4;
        add_quad(sc, t[i], t[j], b[j], b[i], m);
    }
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    int W = 512, H = 512, spp = 256;  // override: ./a7.exe <spp>
    if (argc > 1) spp = std::atoi(argv[1]);

    // materials
    Material red;   red.Kd   = Vec3f(0.63f, 0.065f, 0.05f);
    Material green; green.Kd = Vec3f(0.14f, 0.45f, 0.091f);
    Material white; white.Kd = Vec3f(0.725f, 0.71f, 0.68f);
    Material light; light.Kd = Vec3f(0.65f, 0.65f, 0.65f);
    light.emission = 8.0f  * Vec3f(0.747f+0.058f, 0.747f+0.258f, 0.747f) +
                     15.6f * Vec3f(0.740f+0.287f, 0.740f+0.160f, 0.740f) +
                     18.4f * Vec3f(0.737f+0.642f, 0.737f+0.159f, 0.737f);

    Scene sc;
    // floor / ceiling / back wall (white)
    add_quad(sc, {552.8f,0,0},{0,0,0},{0,0,559.2f},{549.6f,0,559.2f}, &white);
    add_quad(sc, {556,548.8f,0},{556,548.8f,559.2f},{0,548.8f,559.2f},{0,548.8f,0}, &white);
    add_quad(sc, {549.6f,0,559.2f},{0,0,559.2f},{0,548.8f,559.2f},{556,548.8f,559.2f}, &white);
    // left wall red (x~556), right wall green (x=0)
    add_quad(sc, {552.8f,0,0},{549.6f,0,559.2f},{556,548.8f,559.2f},{556,548.8f,0}, &red);
    add_quad(sc, {0,0,559.2f},{0,0,0},{0,548.8f,0},{0,548.8f,559.2f}, &green);
    // ceiling light (just below the ceiling)
    add_quad(sc, {343,548.7f,227},{343,548.7f,332},{213,548.7f,332},{213,548.7f,227}, &light);
    // short block + tall block (white)
    Vec3f shortTop[4] = {{130,165,65},{82,165,225},{240,165,272},{290,165,114}};
    Vec3f tallTop[4]  = {{423,330,247},{265,330,296},{314,330,456},{472,330,406}};
    add_box(sc, shortTop, &white);
    add_box(sc, tallTop, &white);
    sc.build();

    std::printf("A7: Cornell box path tracing, %dx%d, %d spp. %zu triangles, light area %.0f.\n",
                W, H, spp, sc.prims.size(), sc.lightArea);
    std::printf("  BVH: %d nodes, depth %d. threads via OpenMP.\n", sc.bvh->nodeCount, sc.bvh->maxDepth);

    Vec3f eye(278, 273, -800);
    float fov = 40.0f, scale = std::tan(deg2rad(fov * 0.5f)), aspect = (float)W / H;
    Image img(W, H, Vec3f::Zero());

    auto t0 = std::chrono::high_resolution_clock::now();
    long done = 0;
    #pragma omp parallel for schedule(dynamic, 4)
    for (int j = 0; j < H; ++j) {
        for (int i = 0; i < W; ++i) {
            Vec3f acc = Vec3f::Zero();
            for (int s = 0; s < spp; ++s) {
                float rx = get_random_float(), ry = get_random_float();
                float x = (2 * (i + rx) / W - 1) * aspect * scale;
                float y = (1 - 2 * (j + ry) / H) * scale;
                Vec3f dir = Vec3f(-x, y, 1).normalized();
                acc += trace(sc, Ray(eye, dir));
            }
            img.at(i, j) = acc / spp;
        }
        #pragma omp atomic
        ++done;
        if ((done & 63) == 0) { std::printf("\r  rows %ld/%d", done, H); }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double sec = std::chrono::duration<double>(t1 - t0).count();

    img.save_png("results/a7_cornell_box.png", /*gamma=*/true);
    std::printf("\r  rendered in %.1f s (%.2f Mrays est).            \n", sec,
                (double)W * H * spp / 1e6);
    std::printf("Wrote results/a7_cornell_box.png\n");
    for (auto* o : sc.prims) delete o;
    return 0;
}
