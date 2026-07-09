// GAMES101 - Assignment 8: mass-spring rope (质点弹簧系统), headless driver.
//
// A rope is a chain of point masses joined by Hooke springs, pinned at both ends
// and hanging under gravity.  We integrate the same rope three ways and contrast
// their numerical stability:
//   * forward (explicit) Euler   -> UNSTABLE: energy grows, the rope explodes to
//                                   ~1e30 / non-finite within a few thousand steps;
//   * semi-implicit (symplectic) -> STABLE:  bounded forever; with light viscous
//                                   damping it settles into a catenary;
//   * Verlet (position-only)      -> STABLE:  settles into the same catenary.
//
// The official Assignment8 skeleton draws with a GLFW/OpenGL window; on this
// headless CPU box we instead step the sim for a fixed number of steps and
// rasterise the rope to PNGs with stb (matching the rest of this repo).
#include "rope.h"
#include "draw2d.hpp"
#include "image.hpp"
#include "math_utils.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>

using namespace g101;

// ---- world <-> screen mapping (world y is up; image row 0 is the top) ------
// The window is fitted to the simulated geometry at runtime (set_window below),
// with an equal x/y scale so the catenary keeps its true shape.
static const int W = 800, H = 800;
static float WX0 = -1.6f, WX1 = 1.6f;   // visible world window, x
static float WY0 = -2.8f, WY1 = 0.6f;   // visible world window, y (up)

// Fit a square, equal-aspect window around [minx,maxx]x[miny,maxy] with margin.
static void set_window(float minx, float maxx, float miny, float maxy, float margin) {
    float cx = 0.5f * (minx + maxx), cy = 0.5f * (miny + maxy);
    float half = 0.5f * std::max(maxx - minx, maxy - miny);
    half = std::max(half, 0.1f) * (1.0f + margin);
    WX0 = cx - half; WX1 = cx + half;
    WY0 = cy - half; WY1 = cy + half;
}

static Vec2f to_px(const Vec2f& p) {
    float sx = (p.x() - WX0) / (WX1 - WX0) * (W - 1);
    float sy = (p.y() - WY0) / (WY1 - WY0) * (H - 1);
    return Vec2f(sx, (H - 1) - sy);           // flip so world +y points up
}

static std::vector<Vec2f> positions_of(const Rope& r) {
    std::vector<Vec2f> v;
    v.reserve(r.masses.size());
    for (const Mass* m : r.masses) v.push_back(m->position);
    return v;
}

// Draw a rope given as a polyline of node positions + a pinned-index set.
static void draw_poly(Image& img, const std::vector<Vec2f>& pos,
                      const std::vector<int>& pinned, const Vec3f& col,
                      const Vec3f& pin_col) {
    for (size_t i = 0; i + 1 < pos.size(); ++i) {
        Vec2f a = to_px(pos[i]), b = to_px(pos[i + 1]);
        if (!std::isfinite(a.x()) || !std::isfinite(a.y()) ||
            !std::isfinite(b.x()) || !std::isfinite(b.y()))
            continue;
        draw_line_aa(img, a.x(), a.y(), b.x(), b.y(), col);
    }
    std::vector<char> is_pin(pos.size(), 0);
    for (int idx : pinned)
        if (idx >= 0 && idx < (int)pos.size()) is_pin[idx] = 1;
    for (size_t i = 0; i < pos.size(); ++i) {
        Vec2f q = to_px(pos[i]);
        if (!std::isfinite(q.x()) || !std::isfinite(q.y())) continue;
        fill_disk(img, (int)q.x(), (int)q.y(), is_pin[i] ? 6 : 3,
                  is_pin[i] ? pin_col : col);
    }
}

// ---- per-rope metrics ------------------------------------------------------
static bool finite_rope(const Rope& r) {
    for (const Mass* m : r.masses)
        if (!std::isfinite(m->position.x()) || !std::isfinite(m->position.y()))
            return false;
    return true;
}
static float max_disp(const Rope& r) {  // max |x - x0| over free masses
    float md = 0.0f;
    for (const Mass* m : r.masses)
        if (!m->pinned) md = std::max(md, (m->position - m->start_position).norm());
    return md;
}
static float max_speed(const Rope& r) {
    float ms = 0.0f;
    for (const Mass* m : r.masses)
        if (!m->pinned) ms = std::max(ms, m->velocity.norm());
    return ms;
}
static float lowest_y(const Rope& r) {
    float y = 1e30f;
    for (const Mass* m : r.masses) y = std::min(y, m->position.y());
    return y;
}

int main(int argc, char** argv) {
    // ---- rope + integration parameters (shared across all integrators) -----
    const Vec2f start(-1.0f, 0.0f), end(1.0f, 0.0f);
    const int   N         = 21;              // odd -> a true centre node at x=0
    const float node_mass = 1.0f;
    const float K         = (argc > 1) ? std::atof(argv[1]) : 500.0f;  // stiffness
    const Vec2f gravity(0.0f, -9.8f);
    const float dt        = 0.01f;           // fixed step, shared by all runs
    const int   STEPS     = 20000;           // settle window (200 s of sim time)
    const std::vector<int> pinned = {0, N - 1};  // pin both ends -> catenary
    const int   mid       = N / 2;

    const Vec3f BG(0.05f, 0.05f, 0.07f);
    const Vec3f PIN(1.00f, 0.82f, 0.20f);    // pinned nodes: amber
    const Vec3f REF(0.30f, 0.30f, 0.36f);    // initial straight rope: grey

    std::vector<Vec2f> initPos;              // straight starting rope (reference)
    {
        Rope r0(start, end, N, node_mass, K, pinned);
        initPos = positions_of(r0);
    }

    // ============ 1) semi-implicit Euler, WITH damping — SETTLES ===========
    const float euler_damping = 2.0f;        // viscous drag coefficient
    Rope rSemi(start, end, N, node_mass, K, pinned);
    for (int step = 0; step < STEPS; ++step)
        rSemi.simulateEuler(dt, gravity, /*semi_implicit=*/true, euler_damping);
    std::vector<Vec2f> semiPos = positions_of(rSemi);
    float semiFinalSpeed = max_speed(rSemi);
    Vec2f semiTip = rSemi.masses[mid]->position;
    float semiSag = start.y() - lowest_y(rSemi);

    // ================= 2) explicit (forward) Euler — UNSTABLE ===============
    // Snapshot the last frame that stays within ~1.5x the catenary's depth, so the
    // saved image shows the rope visibly diverging past the settled shape (it then
    // keeps growing to 1e30 / non-finite).
    const float expl_cap = std::max(1.5f, 1.5f * semiSag);
    Rope rExpl(start, end, N, node_mass, K, pinned);
    long divStep10 = -1, divStep1e3 = -1, divStep1e6 = -1, nonFiniteStep = -1;
    std::vector<Vec2f> explSnap = initPos;   // last in-view (most diverged) frame
    for (int step = 0; step < STEPS; ++step) {
        rExpl.simulateEuler(dt, gravity, /*semi_implicit=*/false, /*damping=*/0.0f);
        if (!finite_rope(rExpl)) { nonFiniteStep = step; break; }
        float md = max_disp(rExpl);
        if (divStep10  < 0 && md > 10.0f)  divStep10  = step;
        if (divStep1e3 < 0 && md > 1e3f)   divStep1e3 = step;
        if (divStep1e6 < 0 && md > 1e6f)   divStep1e6 = step;
        if (md < expl_cap) explSnap = positions_of(rExpl);  // keep latest in-view
    }
    float explFinalMd = max_disp(rExpl);

    // ============= 3) semi-implicit Euler, NO damping — BOUNDED ============
    // Same dt/K/gravity as the explicit run: symplectic Euler conserves a shadow
    // energy, so it oscillates about the catenary forever but stays bounded.
    Rope rSemiUndamped(start, end, N, node_mass, K, pinned);
    float semiUndampedMaxMd = 0.0f;
    for (int step = 0; step < STEPS; ++step) {
        rSemiUndamped.simulateEuler(dt, gravity, /*semi_implicit=*/true, 0.0f);
        semiUndampedMaxMd = std::max(semiUndampedMaxMd, max_disp(rSemiUndamped));
    }
    bool semiUndampedFinite = finite_rope(rSemiUndamped);

    // ==================== 4) Verlet, WITH damping — SETTLES ================
    const float verlet_damping = 0.002f;
    Rope rVer(start, end, N, node_mass, K, pinned);
    for (int step = 0; step < STEPS; ++step)
        rVer.simulateVerlet(dt, gravity, verlet_damping);
    std::vector<Vec2f> verPos = positions_of(rVer);
    Vec2f verTip = rVer.masses[mid]->position;
    float verSag = start.y() - lowest_y(rVer);
    // Verlet has no explicit velocity; estimate tip speed from x - x_prev.
    float verFinalSpeed = 0.0f;
    for (const Mass* m : rVer.masses)
        if (!m->pinned)
            verFinalSpeed = std::max(verFinalSpeed,
                                     (m->position - m->last_position).norm() / dt);

    // ---- fit one shared window to every rendered rope, then draw -----------
    float minx = 1e30f, maxx = -1e30f, miny = 1e30f, maxy = -1e30f;
    auto expand = [&](const std::vector<Vec2f>& p) {
        for (const Vec2f& q : p) {
            if (!std::isfinite(q.x()) || !std::isfinite(q.y())) continue;
            minx = std::min(minx, q.x()); maxx = std::max(maxx, q.x());
            miny = std::min(miny, q.y()); maxy = std::max(maxy, q.y());
        }
    };
    expand(initPos); expand(semiPos); expand(verPos); expand(explSnap);
    set_window(minx, maxx, miny, maxy, /*margin=*/0.12f);

    const Vec3f RED(1.0f, 0.32f, 0.28f), GREEN(0.30f, 1.0f, 0.45f), BLUE(0.35f, 0.75f, 1.0f);

    Image imgSemi(W, H, BG);
    draw_poly(imgSemi, initPos, pinned, REF, REF);
    draw_poly(imgSemi, semiPos, pinned, GREEN, PIN);
    imgSemi.save_png("results/a8_rope_semi_implicit.png");

    Image imgExpl(W, H, BG);
    draw_poly(imgExpl, initPos, pinned, REF, REF);
    draw_poly(imgExpl, explSnap, pinned, RED, PIN);
    imgExpl.save_png("results/a8_rope_explicit_unstable.png");

    Image imgVer(W, H, BG);
    draw_poly(imgVer, initPos, pinned, REF, REF);
    draw_poly(imgVer, verPos, pinned, BLUE, PIN);
    imgVer.save_png("results/a8_rope_verlet.png");

    Image imgCmp(W, H, BG);
    draw_poly(imgCmp, initPos, pinned, REF, REF);
    draw_poly(imgCmp, explSnap, pinned, RED, PIN);    // diverging
    draw_poly(imgCmp, semiPos, pinned, GREEN, PIN);   // settled
    imgCmp.save_png("results/a8_rope_compare.png");

    // ---- console + results/ log -------------------------------------------
    auto say_div = [](long s) -> std::string {
        return s < 0 ? std::string("never") : ("step " + std::to_string(s));
    };
    char catenary[64];
    std::snprintf(catenary, sizeof catenary, "(%.4f, %.4f)", semiTip.x(), semiTip.y());

    std::printf("A8: mass-spring rope, %d nodes, k=%.0f, mass=%.1f, g=%.1f, dt=%.3f, %d steps\n",
                N, K, node_mass, -gravity.y(), dt, STEPS);
    std::printf("  pinned nodes {0,%d} (both ends) -> hangs into a catenary\n", N - 1);
    std::printf("  [explicit Euler ]  UNSTABLE: |disp|>10 by %s, >1e3 by %s, >1e6 by %s;\n",
                say_div(divStep10).c_str(), say_div(divStep1e3).c_str(),
                say_div(divStep1e6).c_str());
    std::printf("                     non-finite by %s; final max|disp| = %.3e\n",
                say_div(nonFiniteStep).c_str(), explFinalMd);
    std::printf("  [semi-implicit  ]  BOUNDED (no damping): max|disp| = %.4f over %d steps, finite=%s\n",
                semiUndampedMaxMd, STEPS, semiUndampedFinite ? "yes" : "NO");
    std::printf("  [semi-implicit  ]  SETTLES (damping %.1f): tip(mid)=%s, sag=%.4f, max speed=%.3e\n",
                euler_damping, catenary, semiSag, semiFinalSpeed);
    std::printf("  [Verlet         ]  SETTLES (damping %.4f): tip(mid)=(%.4f, %.4f), sag=%.4f, max speed=%.3e\n",
                verlet_damping, verTip.x(), verTip.y(), verSag, verFinalSpeed);
    std::printf("Wrote results/a8_rope_{explicit_unstable,semi_implicit,verlet,compare}.png + a8_rope.txt\n");

    std::ofstream log("results/a8_rope.txt");
    log << "GAMES101 Assignment 8 - mass-spring rope, measured results\n";
    log << "==========================================================\n";
    log << "config: " << N << " nodes, node_mass=" << node_mass << ", k=" << K
        << ", gravity=(0,-" << -gravity.y() << "), dt=" << dt
        << ", steps=" << STEPS << ", pinned={0," << (N - 1) << "} (both ends)\n\n";
    log << "explicit (forward) Euler, no damping  -> UNSTABLE\n";
    log << "  max|disp|>10   at " << say_div(divStep10) << "\n";
    log << "  max|disp|>1e3  at " << say_div(divStep1e3) << "\n";
    log << "  max|disp|>1e6  at " << say_div(divStep1e6) << "\n";
    log << "  non-finite     at " << say_div(nonFiniteStep) << "\n";
    log << "  final max|disp| = " << explFinalMd << "  (rope explodes)\n\n";
    log << "semi-implicit (symplectic) Euler, no damping -> STABLE / BOUNDED\n";
    log << "  max|disp| over " << STEPS << " steps = " << semiUndampedMaxMd
        << "  (finite=" << (semiUndampedFinite ? "yes" : "NO") << ")\n\n";
    log << "semi-implicit Euler, damping=" << euler_damping << " -> SETTLES to catenary\n";
    log << "  tip (midpoint) = (" << semiTip.x() << ", " << semiTip.y() << ")\n";
    log << "  sag = " << semiSag << ", final max speed = " << semiFinalSpeed << "\n\n";
    log << "Verlet, damping=" << verlet_damping << " -> SETTLES to catenary\n";
    log << "  tip (midpoint) = (" << verTip.x() << ", " << verTip.y() << ")\n";
    log << "  sag = " << verSag << ", final max speed = " << verFinalSpeed << "\n\n";
    log << "images: a8_rope_explicit_unstable.png (grey=initial, red=diverging),\n";
    log << "        a8_rope_semi_implicit.png (green=settled catenary),\n";
    log << "        a8_rope_verlet.png (blue=settled catenary),\n";
    log << "        a8_rope_compare.png (red exploding vs green settled).\n";
    log.close();
    return 0;
}
