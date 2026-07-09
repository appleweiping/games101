// GAMES101 - Assignment 3: Blinn-Phong shading + texture / bump / displacement.
//
// A triangle mesh (Keenan Crane's "spot" cow if the asset is present, otherwise a
// procedural UV sphere) is rasterised with a z-buffer and perspective-correct
// interpolation of world position, normal and texture coordinates.  Five fragment
// shaders are applied:
//   normal        - visualise the interpolated surface normal
//   blinn-phong   - ambient + diffuse + specular from two point lights
//   texture       - Blinn-Phong with albedo sampled from the texture
//   bump          - perturb the normal from the texture's height gradient (TBN)
//   displacement  - genuinely displace the geometry along the normal by the height
//                   field, recompute normals, then Blinn-Phong-with-texture shade it
#include "image.hpp"
#include "math_utils.hpp"
#include "mesh.hpp"
#include "texture.hpp"
#include <cstdio>
#include <limits>

using namespace g101;

static Mat4f view_matrix(const Vec3f& eye) {
    Mat4f v = Mat4f::Identity();
    v(0, 3) = -eye.x(); v(1, 3) = -eye.y(); v(2, 3) = -eye.z();
    return v;
}
static Mat4f proj_matrix(float fov, float aspect, float zNear, float zFar) {
    float n = -zNear, f = -zFar;
    float t = std::tan(deg2rad(fov) / 2.0f) * zNear, r = t * aspect;
    Mat4f p2o, os = Mat4f::Identity(), ot = Mat4f::Identity();
    p2o << n,0,0,0, 0,n,0,0, 0,0,n+f,-n*f, 0,0,1,0;
    os(0,0)=1/r; os(1,1)=1/t; os(2,2)=2/(n-f); ot(2,3)=-(n+f)/2;
    return os * ot * p2o;
}
static Mat4f model_matrix(float angle_deg, float scale) {
    float a = deg2rad(angle_deg), c = std::cos(a), s = std::sin(a);
    Mat4f R = Mat4f::Identity();  // rotate about +Y
    R(0,0)=c; R(0,2)=s; R(2,0)=-s; R(2,2)=c;
    Mat4f S = Mat4f::Identity(); S(0,0)=S(1,1)=S(2,2)=scale;
    return R * S;
}

struct Frag { Vec3f pos; Vec3f nrm; Vec2f uv; };

// Rasterise a mesh into `img` using `model`/`view`/`proj` and a fragment shader.
// The shader is a template parameter (not std::function): type-erasing an
// Eigen-returning std::function crashes MinGW's bfd linker (ld exit 116).
template <class Shader>
static void render_mesh(const Mesh& mesh, const Mat4f& model, const Mat4f& view,
                        const Mat4f& proj, Shader&& shade, Image& img) {
    const int W = img.w, H = img.h;
    std::vector<float> zbuf((size_t)W * H, std::numeric_limits<float>::infinity());
    Mat3f nmat = model.block<3,3>(0,0).inverse().transpose();
    float f1 = (50.0f - 0.1f) / 2.0f, f2 = (50.0f + 0.1f) / 2.0f;

    for (const Tri& tri : mesh.tris) {
        Vec3f wp[3], wn[3], scr[3]; float w[3]; Vec2f uv[3];
        bool skip = false;
        for (int i = 0; i < 3; ++i) {
            Vec4f wp4 = model * Vec4f(tri.v[i].pos.x(), tri.v[i].pos.y(), tri.v[i].pos.z(), 1);
            wp[i] = wp4.head<3>();
            wn[i] = (nmat * tri.v[i].nrm).normalized();
            uv[i] = tri.v[i].uv;
            Vec4f clip = proj * view * wp4;
            if (clip.w() >= -1e-6f) { skip = true; break; }  // keep w<0 (camera looks -z)
            w[i] = clip.w();
            Vec3f ndc = clip.head<3>() / clip.w();
            scr[i] = Vec3f(0.5f * W * (ndc.x() + 1), 0.5f * H * (ndc.y() + 1), ndc.z() * f1 + f2);
        }
        if (skip) continue;

        float minx = std::min({scr[0].x(), scr[1].x(), scr[2].x()});
        float maxx = std::max({scr[0].x(), scr[1].x(), scr[2].x()});
        float miny = std::min({scr[0].y(), scr[1].y(), scr[2].y()});
        float maxy = std::max({scr[0].y(), scr[1].y(), scr[2].y()});
        int x0 = std::max(0, (int)std::floor(minx)), x1 = std::min(W-1, (int)std::ceil(maxx));
        int y0 = std::max(0, (int)std::floor(miny)), y1 = std::min(H-1, (int)std::ceil(maxy));

        auto ef = [](const Vec3f& a, const Vec3f& b, float px, float py) {
            return (px-a.x())*(b.y()-a.y()) - (py-a.y())*(b.x()-a.x());
        };
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                float px = x + 0.5f, py = y + 0.5f;
                float ba = ef(scr[1], scr[2], px, py);
                float bb = ef(scr[2], scr[0], px, py);
                float bc = ef(scr[0], scr[1], px, py);
                float sum = ba + bb + bc;
                if (std::abs(sum) < 1e-9f) continue;
                ba /= sum; bb /= sum; bc /= sum;
                if (ba < 0 || bb < 0 || bc < 0) continue;
                float z = ba*scr[0].z() + bb*scr[1].z() + bc*scr[2].z();
                size_t idx = (size_t)y * W + x;
                if (z >= zbuf[idx]) continue;
                // perspective-correct interpolation
                float iw = ba/w[0] + bb/w[1] + bc/w[2];
                auto pc3 = [&](const Vec3f a3[3]) {
                    return (ba*a3[0]/w[0] + bb*a3[1]/w[1] + bc*a3[2]/w[2]) / iw;
                };
                Frag fr;
                fr.pos = pc3(wp);
                fr.nrm = pc3(wn).normalized();
                fr.uv  = (ba*uv[0]/w[0] + bb*uv[1]/w[1] + bc*uv[2]/w[2]) / iw;
                zbuf[idx] = z;
                Vec3f col = shade(fr);
                img.set(x, H-1-y, Vec3f(clampv(0.f,1.f,col.x()), clampv(0.f,1.f,col.y()), clampv(0.f,1.f,col.z())));
            }
    }
}

// Shared Blinn-Phong core; kd is the diffuse albedo, n the shading normal.
static Vec3f blinn_phong(const Vec3f& point, const Vec3f& n, const Vec3f& kd) {
    struct Light { Vec3f pos, I; };
    Light lights[2] = {{{20,20,20},{500,500,500}}, {{-20,20,0},{500,500,500}}};
    Vec3f ka(0.005f,0.005f,0.005f), ks(0.7937f,0.7937f,0.7937f), amb(10,10,10), eye(0,0,10);
    float p = 150.0f;
    Vec3f result = cmul(ka, amb);
    Vec3f v = (eye - point).normalized();
    for (auto& L : lights) {
        Vec3f l = L.pos - point; float r2 = l.squaredNorm(); l.normalize();
        Vec3f h = (v + l).normalized();
        Vec3f Ir = L.I / r2;
        result += cmul(kd, Ir) * std::max(0.f, n.dot(l));
        result += cmul(ks, Ir) * std::pow(std::max(0.f, n.dot(h)), p);
    }
    return result;
}

int main() {
    const int W = 700, H = 700;
    Mesh mesh; std::string src;
    if (load_obj("assets/spot/spot_triangulated.obj", mesh)) src = "spot_triangulated.obj";
    else { mesh = make_uv_sphere(); src = "procedural UV sphere (spot asset absent)"; }

    Texture tex;
    bool have_tex = tex.load("assets/spot/spot_texture.png");

    std::printf("A3: shading %zu triangles from %s; texture: %s (%dx%d)\n",
                mesh.tris.size(), src.c_str(), have_tex ? "spot_texture.png" : "procedural", tex.w, tex.h);

    Mat4f model = model_matrix(140.0f, 2.5f);
    Mat4f view  = view_matrix(Vec3f(0,0,10));
    Mat4f proj  = proj_matrix(45.0f, (float)W/H, 0.1f, 50.0f);
    Vec3f bg(0.10f, 0.10f, 0.12f);

    // --- normal ---
    { Image img(W,H,bg);
      render_mesh(mesh, model, view, proj,
        [](const Frag& f){ return (f.nrm + Vec3f(1,1,1)) * 0.5f; }, img);
      img.save_png("results/a3_normal.png"); std::printf("  wrote a3_normal.png\n"); }

    // --- blinn-phong (flat albedo) ---
    { Image img(W,H,bg); Vec3f kd = Vec3f(148,121,92)/255.0f;
      render_mesh(mesh, model, view, proj,
        [&](const Frag& f){ return blinn_phong(f.pos, f.nrm, kd); }, img);
      img.save_png("results/a3_phong.png"); std::printf("  wrote a3_phong.png\n"); }

    // --- texture ---
    { Image img(W,H,bg);
      render_mesh(mesh, model, view, proj,
        [&](const Frag& f){ return blinn_phong(f.pos, f.nrm, tex.sample(f.uv.x(), f.uv.y())); }, img);
      img.save_png("results/a3_texture.png"); std::printf("  wrote a3_texture.png\n"); }

    // --- bump (perturbed normal visualised) ---
    { Image img(W,H,bg); float kh=2.0f, kn=1.0f;
      render_mesh(mesh, model, view, proj,
        [&](const Frag& f){
            Vec3f n = f.nrm; float x=n.x(),y=n.y(),z=n.z();
            float d = std::sqrt(x*x+z*z) + 1e-6f;
            Vec3f t(x*y/d, d, z*y/d); Vec3f b = n.cross(t);
            Mat3f TBN; TBN.col(0)=t.normalized(); TBN.col(1)=b.normalized(); TBN.col(2)=n;
            float du = kh*kn*(tex.height(f.uv.x()+1.0f/tex.w, f.uv.y()) - tex.height(f.uv.x(), f.uv.y()));
            float dv = kh*kn*(tex.height(f.uv.x(), f.uv.y()+1.0f/tex.h) - tex.height(f.uv.x(), f.uv.y()));
            Vec3f ln(-du, -dv, 1.0f);
            Vec3f pn = (TBN*ln).normalized();
            return (pn + Vec3f(1,1,1)) * 0.5f;
        }, img);
      img.save_png("results/a3_bump.png"); std::printf("  wrote a3_bump.png\n"); }

    // --- displacement (genuine geometry displacement) ---
    { Mesh disp = mesh; float amp = 0.06f;
      // displace along per-vertex normal by (height-0.5)
      for (auto& tr : disp.tris)
          for (int i = 0; i < 3; ++i) {
              float hgt = tex.height(tr.v[i].uv.x(), tr.v[i].uv.y());
              tr.v[i].pos += tr.v[i].nrm * (amp * (hgt - 0.5f));
          }
      // recompute per-triangle normals -> smooth by averaging into a position map
      // (simple: use face normals for the displaced surface)
      for (auto& tr : disp.tris) {
          Vec3f fn = (tr.v[1].pos - tr.v[0].pos).cross(tr.v[2].pos - tr.v[0].pos).normalized();
          for (int i = 0; i < 3; ++i) tr.v[i].nrm = fn;
      }
      Image img(W,H,bg);
      render_mesh(disp, model, view, proj,
        [&](const Frag& f){ return blinn_phong(f.pos, f.nrm, tex.sample(f.uv.x(), f.uv.y())); }, img);
      img.save_png("results/a3_displacement.png"); std::printf("  wrote a3_displacement.png\n"); }

    std::printf("Done. 5 shader images in results/.\n");
    return 0;
}
