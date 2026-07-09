// GAMES101 - Assignment 8: mass-spring rope (质点弹簧系统) — class skeleton.
//
// This mirrors the official Assignment8 skeleton (Mass / Spring / Rope) but uses
// this repo's Eigen Vec2f typedef instead of CGL::Vector2D, and drops the GLFW
// GUI in favour of a headless PNG driver (see main.cpp).  The two things a
// student implements — the Rope constructor and the two integrators — live in
// rope.cpp.
#pragma once
#include "math_utils.hpp"
#include <vector>

namespace g101 {

// A single point mass.  `last_position` is the previous step's position, needed
// by Verlet integration (which recovers velocity implicitly from x - x_prev).
struct Mass {
    Vec2f position;
    Vec2f last_position;   // x_{n-1}, used by Verlet
    Vec2f velocity;
    Vec2f forces;          // accumulator, zeroed each step
    Vec2f start_position;  // where it was created (for displacement metrics)
    float mass;
    bool  pinned;          // pinned masses are fixed in place

    Mass(const Vec2f& pos, float m, bool p)
        : position(pos), last_position(pos), velocity(Vec2f::Zero()),
          forces(Vec2f::Zero()), start_position(pos), mass(m), pinned(p) {}
};

// A Hooke spring connecting two masses; `rest_length` is captured at construction
// from the initial separation of its endpoints.
struct Spring {
    Mass* m1;
    Mass* m2;
    float k;             // stiffness
    float rest_length;

    Spring(Mass* a, Mass* b, float k_)
        : m1(a), m2(b), k(k_), rest_length((a->position - b->position).norm()) {}
};

class Rope {
public:
    std::vector<Mass*>   masses;
    std::vector<Spring*> springs;

    Rope() = default;
    // Create `num_nodes` masses evenly spaced along start->end, join consecutive
    // masses with springs of stiffness `k`, and pin the listed node indices.
    Rope(const Vec2f& start, const Vec2f& end, int num_nodes,
         float node_mass, float k, const std::vector<int>& pinned_nodes);
    ~Rope();

    // One explicit or semi-implicit Euler step.  When `semi_implicit` is true the
    // velocity is advanced before the position (symplectic Euler) -> stable;
    // otherwise the old velocity moves the position first (forward Euler) ->
    // unstable.  `damping` is an optional viscous coefficient (force = -c*v).
    void simulateEuler(float dt, const Vec2f& gravity,
                       bool semi_implicit, float damping = 0.0f);

    // One Verlet step (position-only integration).  `damping` in [0,1) bleeds off
    // energy via the (1-damping) factor so the rope can settle.  Pinned masses
    // never move.
    void simulateVerlet(float dt, const Vec2f& gravity, float damping = 0.0f);

private:
    // Accumulate Hooke spring forces into every mass->forces (Newton's 3rd law).
    void accumulate_spring_forces();
};

}  // namespace g101
