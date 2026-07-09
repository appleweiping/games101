// Small shared math helpers used across the GAMES101 assignments.
#pragma once
#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <random>

namespace g101 {

using Vec2f = Eigen::Vector2f;
using Vec3f = Eigen::Vector3f;
using Vec4f = Eigen::Vector4f;
using Mat3f = Eigen::Matrix3f;
using Mat4f = Eigen::Matrix4f;

constexpr float PI = 3.14159265358979323846f;
constexpr float kInfinity = std::numeric_limits<float>::max();

inline float deg2rad(float d) { return d * PI / 180.0f; }

template <typename T>
inline T clampv(T lo, T hi, T v) { return std::max(lo, std::min(hi, v)); }

// Per-component (Hadamard) product — the ray tracers multiply colours this way.
inline Vec3f cmul(const Vec3f& a, const Vec3f& b) { return a.cwiseProduct(b); }

// Uniform float in [0,1). Each caller keeps its own thread-local generator so the
// path tracer stays deterministic-per-thread and free of data races under OpenMP.
inline float get_random_float() {
    static thread_local std::mt19937 gen(std::random_device{}());
    static thread_local std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(gen);
}

}  // namespace g101
