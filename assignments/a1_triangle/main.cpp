// GAMES101 - Assignment 1: the MVP pipeline.
//
// Build the model (rotation), view (camera) and perspective-projection matrices,
// run three triangle vertices through model->view->projection->perspective-divide->
// viewport, and draw the resulting wireframe triangle.  Also implements the bonus:
// rotation about an arbitrary axis via Rodrigues' formula.
#include "draw2d.hpp"
#include "image.hpp"
#include "math_utils.hpp"
#include <array>
#include <cstdio>

using namespace g101;

static Mat4f get_view_matrix(const Vec3f& eye) {
    Mat4f v = Mat4f::Identity();
    v(0, 3) = -eye.x();
    v(1, 3) = -eye.y();
    v(2, 3) = -eye.z();
    return v;
}

// Rotation about the +Z axis by `angle` degrees (the required model transform).
static Mat4f get_model_matrix(float angle_deg) {
    float a = deg2rad(angle_deg), c = std::cos(a), s = std::sin(a);
    Mat4f m = Mat4f::Identity();
    m(0, 0) = c;  m(0, 1) = -s;
    m(1, 0) = s;  m(1, 1) = c;
    return m;
}

// Bonus: rotation of `angle` degrees about an arbitrary unit axis n (Rodrigues).
static Mat4f get_rotation(Vec3f axis, float angle_deg) {
    axis.normalize();
    float a = deg2rad(angle_deg), c = std::cos(a), s = std::sin(a);
    Mat3f N;
    N <<        0, -axis.z(),  axis.y(),
         axis.z(),         0, -axis.x(),
        -axis.y(),  axis.x(),         0;
    Mat3f R = c * Mat3f::Identity() + (1 - c) * (axis * axis.transpose()) + s * N;
    Mat4f m = Mat4f::Identity();
    m.block<3, 3>(0, 0) = R;
    return m;
}

// Perspective projection (the corrected GAMES101 formulation).  eye_fov in degrees;
// zNear/zFar are positive distances with the camera looking down -z.
static Mat4f get_projection_matrix(float eye_fov, float aspect, float zNear, float zFar) {
    float n = -zNear, f = -zFar;
    float t = std::tan(deg2rad(eye_fov) / 2.0f) * zNear;  // top of near plane
    float r = t * aspect;

    Mat4f persp2ortho;
    persp2ortho << n, 0, 0, 0,
                   0, n, 0, 0,
                   0, 0, n + f, -n * f,
                   0, 0, 1, 0;
    Mat4f ortho_scale = Mat4f::Identity();
    ortho_scale(0, 0) = 1.0f / r;
    ortho_scale(1, 1) = 1.0f / t;
    ortho_scale(2, 2) = 2.0f / (n - f);
    Mat4f ortho_trans = Mat4f::Identity();
    ortho_trans(2, 3) = -(n + f) / 2.0f;
    return ortho_scale * ortho_trans * persp2ortho;
}

struct Vtx { Vec3f p; };

static void render_triangle(const std::string& path, const Mat4f& model,
                            const std::array<Vec3f, 3>& tri, bool verbose) {
    const int W = 700, H = 700;
    Image img(W, H, Vec3f(0.0f, 0.0f, 0.0f));

    Vec3f eye(0, 0, 5);
    Mat4f mvp = get_projection_matrix(45.0f, 1.0f, 0.1f, 50.0f) *
                get_view_matrix(eye) * model;

    std::array<Vec3f, 3> scr;
    float f1 = (50.0f - 0.1f) / 2.0f, f2 = (50.0f + 0.1f) / 2.0f;
    for (int i = 0; i < 3; ++i) {
        Vec4f v(tri[i].x(), tri[i].y(), tri[i].z(), 1.0f);
        Vec4f c = mvp * v;
        c /= c.w();  // perspective divide -> NDC
        // viewport transform to pixel coordinates
        float sx = 0.5f * W * (c.x() + 1.0f);
        float sy = 0.5f * H * (c.y() + 1.0f);
        float sz = c.z() * f1 + f2;
        scr[i] = Vec3f(sx, sy, sz);
        if (verbose)
            std::printf("  vtx%d world(% .1f,% .1f,% .1f) -> screen(%.1f, %.1f)\n",
                        i, tri[i].x(), tri[i].y(), tri[i].z(), sx, sy);
    }

    Vec3f col(0.2f, 1.0f, 0.4f);  // classic GAMES101 green wireframe
    for (int i = 0; i < 3; ++i) {
        int j = (i + 1) % 3;
        // flip y so +y points up in the saved PNG
        draw_line(img, (int)scr[i].x(), H - 1 - (int)scr[i].y(),
                       (int)scr[j].x(), H - 1 - (int)scr[j].y(), col);
    }
    img.save_png(path);
    std::printf("Wrote %s\n", path.c_str());
}

int main() {
    std::array<Vec3f, 3> tri = {Vec3f(2, 0, -2), Vec3f(0, 2, -2), Vec3f(-2, 0, -2)};

    std::printf("A1: MVP pipeline. Camera eye=(0,0,5), fov=45, near=0.1, far=50.\n");
    std::printf("Upright triangle (model angle 0):\n");
    render_triangle("results/a1_triangle.png", get_model_matrix(0.0f), tri, true);

    std::printf("Model rotated 45 deg about +Z:\n");
    render_triangle("results/a1_rotate_z.png", get_model_matrix(45.0f), tri, false);

    std::printf("Bonus: rotated 60 deg about arbitrary axis (1,1,1) [Rodrigues]:\n");
    render_triangle("results/a1_rotate_axis.png", get_rotation(Vec3f(1, 1, 1), 60.0f), tri, false);
    return 0;
}
