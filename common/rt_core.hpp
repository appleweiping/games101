// Shared ray-tracing core for A5 (Whitted), A6 (BVH) and A7 (path tracing):
// Ray, axis-aligned Bounds3, Material (with Monte-Carlo sample/pdf/eval),
// Intersection, the Object hierarchy (Sphere / Triangle), and a BVH accelerator.
// Uses Eigen::Vector3f throughout; component-wise colour products via cmul().
#pragma once
#include "math_utils.hpp"
#include <algorithm>
#include <memory>
#include <vector>

namespace g101 {

constexpr float RT_EPS = 1e-4f;

struct Ray {
    Vec3f origin, dir, inv;
    Ray() = default;
    Ray(const Vec3f& o, const Vec3f& d) : origin(o), dir(d.normalized()) {
        inv = dir.cwiseInverse();
    }
    Vec3f at(float t) const { return origin + dir * t; }
};

// ---------------- Material ----------------
enum MatType { DIFFUSE, REFLECTION_REFRACTION, REFLECTION, DIFFUSE_GLOSSY };

struct Material {
    MatType type = DIFFUSE;
    Vec3f Kd = Vec3f(0.5f, 0.5f, 0.5f);   // diffuse albedo
    Vec3f Ks = Vec3f::Zero();
    Vec3f emission = Vec3f::Zero();
    float ior = 1.3f;
    float specExp = 25.0f;
    float kd = 0.8f, ks = 0.2f;           // Whitted diffuse/specular split

    bool hasEmission() const { return emission.maxCoeff() > RT_EPS; }

    // Build an orthonormal basis around N and map a local hemisphere dir to world.
    static Vec3f toWorld(const Vec3f& a, const Vec3f& N) {
        Vec3f B, C;
        if (std::fabs(N.x()) > std::fabs(N.y())) {
            float inv = 1.0f / std::sqrt(N.x() * N.x() + N.z() * N.z());
            C = Vec3f(N.z() * inv, 0, -N.x() * inv);
        } else {
            float inv = 1.0f / std::sqrt(N.y() * N.y() + N.z() * N.z());
            C = Vec3f(0, N.z() * inv, -N.y() * inv);
        }
        B = C.cross(N);
        return a.x() * B + a.y() * C + a.z() * N;
    }

    // Uniform-hemisphere importance sampling of the next bounce direction (diffuse).
    Vec3f sample(const Vec3f& /*wi*/, const Vec3f& N) const {
        float x1 = get_random_float(), x2 = get_random_float();
        float z = std::fabs(1.0f - 2.0f * x1);
        float r = std::sqrt(std::max(0.0f, 1.0f - z * z)), phi = 2 * PI * x2;
        Vec3f local(r * std::cos(phi), r * std::sin(phi), z);
        return toWorld(local, N);
    }
    float pdf(const Vec3f& /*wi*/, const Vec3f& wo, const Vec3f& N) const {
        return wo.dot(N) > 0.0f ? 0.5f / PI : 0.0f;  // uniform hemisphere
    }
    Vec3f eval(const Vec3f& /*wi*/, const Vec3f& wo, const Vec3f& N) const {
        if (wo.dot(N) > 0.0f) return Kd / PI;  // Lambertian
        return Vec3f::Zero();
    }
};

// ---------------- Bounds3 (AABB) ----------------
struct Bounds3 {
    Vec3f pMin, pMax;
    Bounds3() {
        pMin = Vec3f::Constant(kInfinity);
        pMax = Vec3f::Constant(-kInfinity);
    }
    explicit Bounds3(const Vec3f& p) : pMin(p), pMax(p) {}
    Bounds3(const Vec3f& a, const Vec3f& b) : pMin(a.cwiseMin(b)), pMax(a.cwiseMax(b)) {}

    Vec3f diagonal() const { return pMax - pMin; }
    int maxExtent() const {
        Vec3f d = diagonal();
        return d.x() > d.y() && d.x() > d.z() ? 0 : (d.y() > d.z() ? 1 : 2);
    }
    Vec3f centroid() const { return 0.5f * pMin + 0.5f * pMax; }
    double surfaceArea() const {
        Vec3f d = diagonal();
        return 2.0 * (d.x() * d.y() + d.y() * d.z() + d.z() * d.x());
    }
    // Slab test: returns true if [tEnter,tExit] overlaps the positive ray range.
    bool intersectP(const Ray& ray) const {
        float t0 = 0.0f, t1 = kInfinity;
        for (int a = 0; a < 3; ++a) {
            float tNear = (pMin[a] - ray.origin[a]) * ray.inv[a];
            float tFar = (pMax[a] - ray.origin[a]) * ray.inv[a];
            if (tNear > tFar) std::swap(tNear, tFar);
            t0 = std::max(t0, tNear);
            t1 = std::min(t1, tFar);
            if (t0 > t1) return false;
        }
        return true;
    }
};
inline Bounds3 Union(const Bounds3& a, const Bounds3& b) {
    Bounds3 r; r.pMin = a.pMin.cwiseMin(b.pMin); r.pMax = a.pMax.cwiseMax(b.pMax); return r;
}
inline Bounds3 Union(const Bounds3& a, const Vec3f& p) {
    Bounds3 r; r.pMin = a.pMin.cwiseMin(p); r.pMax = a.pMax.cwiseMax(p); return r;
}

// ---------------- Object hierarchy ----------------
struct Object;
struct Intersection {
    bool happened = false;
    Vec3f coords = Vec3f::Zero();
    Vec3f normal = Vec3f::Zero();
    Vec3f emit = Vec3f::Zero();
    float distance = kInfinity;
    const Object* obj = nullptr;
    Material* m = nullptr;
};

struct Object {
    Material* material = nullptr;
    virtual ~Object() = default;
    virtual bool intersect(const Ray& ray, Intersection& hit) const = 0;
    virtual Bounds3 bounds() const = 0;
    virtual float area() const = 0;
    virtual void sample(Intersection& pos, float& pdf) const = 0;
    // Surface albedo at a hit point; overridden by the checkerboard floor in A5.
    virtual Vec3f diffuseColor(const Vec3f& /*p*/) const {
        return material ? material->Kd : Vec3f(0.5f, 0.5f, 0.5f);
    }
    bool hasEmit() const { return material && material->hasEmission(); }
};

struct Sphere : Object {
    Vec3f center; float radius, radius2, m_area;
    Sphere(const Vec3f& c, float r, Material* mat) : center(c), radius(r), radius2(r * r) {
        material = mat; m_area = 4 * PI * r * r;
    }
    bool intersect(const Ray& ray, Intersection& hit) const override {
        Vec3f L = ray.origin - center;
        float a = ray.dir.dot(ray.dir);
        float b = 2 * ray.dir.dot(L);
        float c = L.dot(L) - radius2;
        float disc = b * b - 4 * a * c;
        if (disc < 0) return false;
        float s = std::sqrt(disc);
        float t0 = (-b - s) / (2 * a), t1 = (-b + s) / (2 * a);
        if (t0 > t1) std::swap(t0, t1);
        float t = t0 > RT_EPS ? t0 : (t1 > RT_EPS ? t1 : -1);
        if (t < 0) return false;
        hit.happened = true; hit.distance = t; hit.coords = ray.at(t);
        hit.normal = (hit.coords - center).normalized();
        hit.obj = this; hit.m = material; hit.emit = material->emission;
        return true;
    }
    Bounds3 bounds() const override {
        return Bounds3(center - Vec3f::Constant(radius), center + Vec3f::Constant(radius));
    }
    float area() const override { return m_area; }
    void sample(Intersection& pos, float& pdf) const override {
        float t1 = 2 * PI * get_random_float(), t2 = PI * get_random_float();
        Vec3f d(std::cos(t1) * std::sin(t2), std::sin(t1) * std::sin(t2), std::cos(t2));
        pos.coords = center + radius * d; pos.normal = d; pos.emit = material->emission;
        pdf = 1.0f / m_area;
    }
};

struct Triangle : Object {
    Vec3f v0, v1, v2, e1, e2, normal; float m_area;
    Triangle(const Vec3f& a, const Vec3f& b, const Vec3f& c, Material* mat)
        : v0(a), v1(b), v2(c), e1(b - a), e2(c - a) {
        material = mat;
        Vec3f n = e1.cross(e2);
        m_area = n.norm() * 0.5f;
        normal = n.normalized();
    }
    bool intersect(const Ray& ray, Intersection& hit) const override {  // Moeller-Trumbore
        Vec3f pvec = ray.dir.cross(e2);
        float det = e1.dot(pvec);
        if (std::fabs(det) < 1e-8f) return false;
        float invDet = 1.0f / det;
        Vec3f tvec = ray.origin - v0;
        float u = tvec.dot(pvec) * invDet;
        if (u < 0 || u > 1) return false;
        Vec3f qvec = tvec.cross(e1);
        float v = ray.dir.dot(qvec) * invDet;
        if (v < 0 || u + v > 1) return false;
        float t = e2.dot(qvec) * invDet;
        if (t < RT_EPS) return false;
        hit.happened = true; hit.distance = t; hit.coords = ray.at(t);
        hit.normal = normal; hit.obj = this; hit.m = material; hit.emit = material->emission;
        return true;
    }
    Bounds3 bounds() const override { return Union(Bounds3(v0, v1), v2); }
    float area() const override { return m_area; }
    void sample(Intersection& pos, float& pdf) const override {
        float x = std::sqrt(get_random_float()), y = get_random_float();
        pos.coords = v0 * (1 - x) + v1 * (x * (1 - y)) + v2 * (x * y);
        pos.normal = normal; pos.emit = material->emission;
        pdf = 1.0f / m_area;
    }
};

// Checkerboard floor for the Whitted scene (A5): albedo alternates on an x/z grid.
struct CheckerTriangle : Triangle {
    using Triangle::Triangle;
    Vec3f diffuseColor(const Vec3f& p) const override {
        float scale = 5.0f;
        bool pat = (std::fmod(p.x() * scale, 2.0f) > 1.0f) ^ (std::fmod(p.z() * scale, 2.0f) > 1.0f);
        return pat ? Vec3f(0.815f, 0.235f, 0.031f) : Vec3f(0.937f, 0.937f, 0.231f);
    }
};

// ---------------- BVH ----------------
struct BVHNode {
    Bounds3 box;
    BVHNode* left = nullptr;
    BVHNode* right = nullptr;
    Object* obj = nullptr;  // non-null only in leaves
};

class BVH {
public:
    std::vector<Object*> prims;
    BVHNode* root = nullptr;
    int nodeCount = 0, leafCount = 0, maxDepth = 0;

    explicit BVH(std::vector<Object*> objects) : prims(std::move(objects)) {
        if (!prims.empty()) root = build(prims, 0);
    }

    Intersection intersect(const Ray& ray) const {
        Intersection isect;
        if (root) traverse(root, ray, isect);
        return isect;
    }
    Bounds3 worldBound() const { return root ? root->box : Bounds3(); }

private:
    BVHNode* build(std::vector<Object*>& objs, int depth) {
        ++nodeCount; maxDepth = std::max(maxDepth, depth);
        BVHNode* node = new BVHNode();
        Bounds3 b;
        for (auto* o : objs) b = Union(b, o->bounds());
        if (objs.size() == 1) { node->box = objs[0]->bounds(); node->obj = objs[0]; ++leafCount; return node; }
        if (objs.size() == 2) {
            std::vector<Object*> l{objs[0]}, r{objs[1]};
            node->left = build(l, depth + 1); node->right = build(r, depth + 1);
            node->box = Union(node->left->box, node->right->box); return node;
        }
        Bounds3 cbox;
        for (auto* o : objs) cbox = Union(cbox, o->bounds().centroid());
        int axis = cbox.maxExtent();
        std::sort(objs.begin(), objs.end(), [axis](Object* a, Object* c) {
            return a->bounds().centroid()[axis] < c->bounds().centroid()[axis];
        });
        auto mid = objs.begin() + objs.size() / 2;
        std::vector<Object*> l(objs.begin(), mid), r(mid, objs.end());
        node->left = build(l, depth + 1);
        node->right = build(r, depth + 1);
        node->box = Union(node->left->box, node->right->box);
        return node;
    }
    void traverse(BVHNode* node, const Ray& ray, Intersection& best) const {
        if (!node->box.intersectP(ray)) return;
        if (node->obj) {
            Intersection h;
            if (node->obj->intersect(ray, h) && h.distance < best.distance) best = h;
            return;
        }
        traverse(node->left, ray, best);
        traverse(node->right, ray, best);
    }
};

// Linear (brute-force) intersection over a primitive list — used to measure the
// BVH speed-up in A6.
inline Intersection brute_force_intersect(const std::vector<Object*>& prims, const Ray& ray) {
    Intersection best;
    for (auto* o : prims) {
        Intersection h;
        if (o->intersect(ray, h) && h.distance < best.distance) best = h;
    }
    return best;
}

}  // namespace g101
