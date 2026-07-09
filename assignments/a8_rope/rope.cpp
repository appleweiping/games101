// GAMES101 - Assignment 8: the Rope constructor + Euler/Verlet integrators.
#include "rope.h"

namespace g101 {

Rope::Rope(const Vec2f& start, const Vec2f& end, int num_nodes,
           float node_mass, float k, const std::vector<int>& pinned_nodes) {
    // Evenly place num_nodes point masses along the segment start->end.
    masses.reserve(num_nodes);
    for (int i = 0; i < num_nodes; ++i) {
        float t = (num_nodes > 1) ? float(i) / float(num_nodes - 1) : 0.0f;
        Vec2f pos = start + t * (end - start);
        masses.push_back(new Mass(pos, node_mass, false));
    }
    // Connect each consecutive pair with a spring (rest length = initial spacing).
    for (int i = 0; i + 1 < num_nodes; ++i)
        springs.push_back(new Spring(masses[i], masses[i + 1], k));
    // Pin the requested nodes so they stay fixed.
    for (int idx : pinned_nodes)
        if (idx >= 0 && idx < num_nodes) masses[idx]->pinned = true;
}

Rope::~Rope() {
    for (Spring* s : springs) delete s;
    for (Mass* m : masses) delete m;
}

void Rope::accumulate_spring_forces() {
    // Hooke's law per spring: f = k * (|d| - L0) * d_hat, pulling the endpoints
    // toward their rest separation.  Equal and opposite on the two masses.
    for (Spring* s : springs) {
        Vec2f d = s->m2->position - s->m1->position;
        float len = d.norm();
        if (len < 1e-12f) continue;
        Vec2f f = s->k * (len - s->rest_length) * (d / len);
        s->m1->forces += f;   // m1 pulled toward m2
        s->m2->forces -= f;   // m2 pulled toward m1
    }
}

void Rope::simulateEuler(float dt, const Vec2f& gravity,
                         bool semi_implicit, float damping) {
    accumulate_spring_forces();
    for (Mass* m : masses) {
        if (!m->pinned) {
            // gravity is an acceleration -> turn it into a force so springs and
            // gravity combine in one f = m*a.  Optional viscous drag opposes v.
            Vec2f f = m->forces + m->mass * gravity - damping * m->velocity;
            Vec2f a = f / m->mass;
            if (semi_implicit) {
                m->velocity += a * dt;             // update v first ...
                m->position += m->velocity * dt;   // ... then step x with new v
            } else {
                m->position += m->velocity * dt;   // step x with old v ...
                m->velocity += a * dt;             // ... then update v
            }
        }
        m->forces = Vec2f::Zero();
    }
}

void Rope::simulateVerlet(float dt, const Vec2f& gravity, float damping) {
    accumulate_spring_forces();
    for (Mass* m : masses) {
        if (!m->pinned) {
            Vec2f a = m->forces / m->mass + gravity;
            Vec2f temp = m->position;
            // x_{n+1} = x_n + (1-damping)*(x_n - x_{n-1}) + a*dt^2
            m->position += (1.0f - damping) * (m->position - m->last_position) +
                           a * dt * dt;
            m->last_position = temp;
        }
        m->forces = Vec2f::Zero();
    }
}

}  // namespace g101
