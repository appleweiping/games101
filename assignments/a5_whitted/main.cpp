// GAMES101 - Assignment 5: Whitted-style ray tracing.
//
// Cast a primary ray per pixel, find the nearest hit (ray-sphere + Moeller-Trumbore
// ray-triangle), and shade recursively: diffuse+glossy Phong surfaces with hard
// shadows, mirror reflection, and glass (reflection+refraction mixed by the Fresnel
// term).  Classic scene: two spheres (one glossy, one glass) over a checkerboard
// floor, lit by two point lights.
#include "image.hpp"
#include "math_utils.hpp"
#include "rt_core.hpp"
#include <cstdio>

using namespace g101;

struct Light { Vec3f pos, intensity; };

static Vec3f reflect(const Vec3f& I, const Vec3f& N) { return I - 2 * I.dot(N) * N; }

static Vec3f refract(const Vec3f& I, const Vec3f& N, float ior) {
    float cosi = clampv(-1.0f, 1.0f, I.dot(N));
    float etai = 1, etat = ior; Vec3f n = N;
    if (cosi < 0) cosi = -cosi; else { std::swap(etai, etat); n = -N; }
    float eta = etai / etat;
    float k = 1 - eta * eta * (1 - cosi * cosi);
    if (k < 0) return Vec3f::Zero();
    return eta * I + (eta * cosi - std::sqrt(k)) * n;
}

static float fresnel(const Vec3f& I, const Vec3f& N, float ior) {
    float cosi = clampv(-1.0f, 1.0f, I.dot(N));
    float etai = 1, etat = ior;
    if (cosi > 0) std::swap(etai, etat);
    float sint = etai / etat * std::sqrt(std::max(0.0f, 1 - cosi * cosi));
    if (sint >= 1) return 1.0f;  // total internal reflection
    float cost = std::sqrt(std::max(0.0f, 1 - sint * sint));
    cosi = std::fabs(cosi);
    float Rs = ((etat * cosi) - (etai * cost)) / ((etat * cosi) + (etai * cost));
    float Rp = ((etai * cosi) - (etat * cost)) / ((etai * cosi) + (etat * cost));
    return (Rs * Rs + Rp * Rp) / 2;
}

static Vec3f cast_ray(const BVH& scene, const std::vector<Light>& lights,
                      const Vec3f& bg, const Ray& ray, int depth) {
    if (depth > 5) return bg;
    Intersection hit = scene.intersect(ray);
    if (!hit.happened) return bg;

    Vec3f p = hit.coords, N = hit.normal;
    Material* m = hit.m;
    auto offset = [&](const Vec3f& d) -> Vec3f {
        if (d.dot(N) < 0) return p - N * RT_EPS;
        return p + N * RT_EPS;
    };

    if (m->type == REFLECTION_REFRACTION) {
        float kr = fresnel(ray.dir, N, m->ior);
        Vec3f rdir = reflect(ray.dir, N).normalized();
        Vec3f tdir = refract(ray.dir, N, m->ior).normalized();
        Vec3f rcol = cast_ray(scene, lights, bg, Ray(offset(rdir), rdir), depth + 1);
        Vec3f tcol = (kr < 1.0f)
                         ? cast_ray(scene, lights, bg, Ray(offset(tdir), tdir), depth + 1)
                         : Vec3f::Zero();
        return rcol * kr + tcol * (1 - kr);
    }
    if (m->type == REFLECTION) {
        float kr = fresnel(ray.dir, N, m->ior);
        Vec3f rdir = reflect(ray.dir, N).normalized();
        return cast_ray(scene, lights, bg, Ray(offset(rdir), rdir), depth + 1) * kr;
    }
    // diffuse + glossy Phong with shadows
    Vec3f lightAmt = Vec3f::Zero(), specColor = Vec3f::Zero();
    Vec3f shadowOrig = ray.dir.dot(N) < 0 ? Vec3f(p + N * RT_EPS) : Vec3f(p - N * RT_EPS);
    for (const Light& L : lights) {
        Vec3f ldir = L.pos - p; float dist2 = ldir.squaredNorm(); ldir.normalize();
        float LdotN = std::max(0.0f, ldir.dot(N));
        Intersection s = scene.intersect(Ray(shadowOrig, ldir));
        bool inShadow = s.happened && s.distance * s.distance < dist2;
        lightAmt += inShadow ? Vec3f::Zero() : cmul(L.intensity, Vec3f::Constant(LdotN));
        Vec3f rr = reflect(-ldir, N);
        specColor += std::pow(std::max(0.0f, -rr.dot(ray.dir)), m->specExp) * L.intensity;
    }
    return cmul(lightAmt, hit.obj->diffuseColor(p)) * m->kd + specColor * m->ks;
}

int main() {
    const int W = 1280, H = 960;
    float fov = 90.0f, scale = std::tan(deg2rad(fov * 0.5f)), aspect = (float)W / H;
    Vec3f bg(0.235f, 0.674f, 0.843f), eye(0, 0, 0);

    Material glossy; glossy.type = DIFFUSE_GLOSSY; glossy.Kd = Vec3f(0.6f, 0.7f, 0.8f);
    glossy.kd = 0.8f; glossy.ks = 0.2f; glossy.specExp = 25.0f;
    Material glass; glass.type = REFLECTION_REFRACTION; glass.ior = 1.5f;
    Material floorMat; floorMat.type = DIFFUSE_GLOSSY; floorMat.kd = 0.8f; floorMat.ks = 0.0f; floorMat.specExp = 0;

    std::vector<Object*> objs;
    objs.push_back(new Sphere(Vec3f(-1, 0, -12), 2.0f, &glossy));
    objs.push_back(new Sphere(Vec3f(0.5f, -0.5f, -8), 1.5f, &glass));
    Vec3f a(-5, -3, -6), b(5, -3, -6), c(5, -3, -16), d(-5, -3, -16);
    objs.push_back(new CheckerTriangle(a, b, c, &floorMat));
    objs.push_back(new CheckerTriangle(a, c, d, &floorMat));
    BVH scene(objs);

    std::vector<Light> lights = {{{-20, 70, 20}, {0.5f, 0.5f, 0.5f}},
                                 {{30, 50, -12}, {0.5f, 0.5f, 0.5f}}};

    Image img(W, H, bg);
    for (int j = 0; j < H; ++j)
        for (int i = 0; i < W; ++i) {
            float x = (2 * (i + 0.5f) / W - 1) * aspect * scale;
            float y = (1 - 2 * (j + 0.5f) / H) * scale;
            Vec3f dir = Vec3f(x, y, -1).normalized();
            img.set(i, j, cast_ray(scene, lights, bg, Ray(eye, dir), 0));
        }
    img.save_png("results/a5_whitted.png");
    for (auto* o : objs) delete o;

    std::printf("A5: Whitted ray tracing, %dx%d, 2 spheres (glossy + glass) + checker floor.\n", W, H);
    std::printf("  ray-sphere + Moeller-Trumbore, Fresnel reflect/refract, hard shadows, depth<=5.\n");
    std::printf("Wrote results/a5_whitted.png\n");
    return 0;
}
