// Minimal Wavefront OBJ loader + a procedural UV-sphere fallback.
// Produces a flat list of triangles carrying per-vertex position / normal / uv.
// Vertex normals are synthesised (area-weighted face normals) when the file
// has none, which is the case for Keenan Crane's spot_triangulated.obj.
#pragma once
#include "math_utils.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace g101 {

struct Vertex { Vec3f pos = Vec3f::Zero(); Vec3f nrm = Vec3f::Zero(); Vec2f uv = Vec2f::Zero(); };
struct Tri { Vertex v[3]; };
struct Mesh { std::vector<Tri> tris; };

inline bool load_obj(const std::string& path, Mesh& mesh) {
    std::ifstream in(path);
    if (!in) return false;
    std::vector<Vec3f> P, N;
    std::vector<Vec2f> T;
    struct Face { int p[3], t[3], n[3]; };
    std::vector<Face> faces;

    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string tag; ss >> tag;
        if (tag == "v") { Vec3f p; ss >> p.x() >> p.y() >> p.z(); P.push_back(p); }
        else if (tag == "vt") { Vec2f t; ss >> t.x() >> t.y(); T.push_back(t); }
        else if (tag == "vn") { Vec3f n; ss >> n.x() >> n.y() >> n.z(); N.push_back(n); }
        else if (tag == "f") {
            int pi[16], ti[16], ni[16], k = 0;
            std::string tok;
            while (ss >> tok && k < 16) {
                pi[k] = ti[k] = ni[k] = 0;
                int a = 0, b = 0, c = 0;
                // parse v, v/vt, v//vn, v/vt/vn
                if (std::sscanf(tok.c_str(), "%d/%d/%d", &a, &b, &c) == 3) {}
                else if (std::sscanf(tok.c_str(), "%d//%d", &a, &c) == 2) { b = 0; }
                else if (std::sscanf(tok.c_str(), "%d/%d", &a, &b) == 2) { c = 0; }
                else { std::sscanf(tok.c_str(), "%d", &a); }
                pi[k] = a; ti[k] = b; ni[k] = c; ++k;
            }
            for (int j = 1; j + 1 < k; ++j) {  // fan-triangulate
                Face f;
                int id[3] = {0, j, j + 1};
                for (int m = 0; m < 3; ++m) { f.p[m] = pi[id[m]]; f.t[m] = ti[id[m]]; f.n[m] = ni[id[m]]; }
                faces.push_back(f);
            }
        }
    }
    auto fix = [](int i, int n) { return i > 0 ? i - 1 : (i < 0 ? n + i : -1); };

    bool haveN = !N.empty();
    std::vector<Vec3f> accN;  // computed normals if file has none
    if (!haveN) {
        accN.assign(P.size(), Vec3f::Zero());
        for (auto& f : faces) {
            Vec3f a = P[fix(f.p[0], P.size())], b = P[fix(f.p[1], P.size())], c = P[fix(f.p[2], P.size())];
            Vec3f fn = (b - a).cross(c - a);  // area-weighted (not normalised)
            for (int m = 0; m < 3; ++m) accN[fix(f.p[m], P.size())] += fn;
        }
        for (auto& v : accN) if (v.squaredNorm() > 0) v.normalize();
    }

    mesh.tris.clear();
    mesh.tris.reserve(faces.size());
    for (auto& f : faces) {
        Tri tri;
        for (int m = 0; m < 3; ++m) {
            int pidx = fix(f.p[m], P.size());
            tri.v[m].pos = P[pidx];
            tri.v[m].uv = f.t[m] ? T[fix(f.t[m], T.size())] : Vec2f(0, 0);
            tri.v[m].nrm = haveN ? N[fix(f.n[m], N.size())] : accN[pidx];
        }
        mesh.tris.push_back(tri);
    }
    return !mesh.tris.empty();
}

// Procedural UV sphere used when no model asset is present, so A3 still runs.
inline Mesh make_uv_sphere(int stacks = 64, int slices = 128, float radius = 1.0f) {
    std::vector<std::vector<Vertex>> grid(stacks + 1, std::vector<Vertex>(slices + 1));
    for (int i = 0; i <= stacks; ++i) {
        float v = (float)i / stacks, phi = v * PI;  // 0..pi
        for (int j = 0; j <= slices; ++j) {
            float u = (float)j / slices, theta = u * 2 * PI;  // 0..2pi
            Vec3f n(std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta));
            Vertex vv; vv.nrm = n; vv.pos = n * radius; vv.uv = Vec2f(u, 1 - v);
            grid[i][j] = vv;
        }
    }
    Mesh m;
    for (int i = 0; i < stacks; ++i)
        for (int j = 0; j < slices; ++j) {
            Tri t1{{grid[i][j], grid[i + 1][j], grid[i + 1][j + 1]}};
            Tri t2{{grid[i][j], grid[i + 1][j + 1], grid[i][j + 1]}};
            m.tris.push_back(t1); m.tris.push_back(t2);
        }
    return m;
}

}  // namespace g101
