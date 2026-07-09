// GAMES101 - Assignment 0: getting started with Eigen and homogeneous transforms.
//
// Task: take the point P = (2, 1), rotate it 45 degrees counter-clockwise about
// the origin, then translate it by (1, 2).  Do it with homogeneous coordinates
// and 2D affine matrices, print the result, and render a small visualisation so
// the assignment produces a real image like every other one in this repo.
#include "draw2d.hpp"
#include "image.hpp"
#include "math_utils.hpp"
#include <cstdio>

using namespace g101;

// 3x3 homogeneous 2D rotation (CCW) and translation.
static Mat3f rotation2d(float deg) {
    float a = deg2rad(deg), c = std::cos(a), s = std::sin(a);
    Mat3f m;
    m << c, -s, 0,
         s,  c, 0,
         0,  0, 1;
    return m;
}
static Mat3f translation2d(float tx, float ty) {
    Mat3f m = Mat3f::Identity();
    m(0, 2) = tx;
    m(1, 2) = ty;
    return m;
}

int main() {
    Eigen::Vector3f P(2.0f, 1.0f, 1.0f);  // homogeneous point

    Mat3f R = rotation2d(45.0f);
    Mat3f T = translation2d(1.0f, 2.0f);
    Mat3f M = T * R;  // rotate first, then translate

    Eigen::Vector3f rotated = R * P;
    Eigen::Vector3f result = M * P;

    std::printf("Input point P            = (%.4f, %.4f)\n", P.x(), P.y());
    std::printf("After rotate 45 deg CCW  = (%.4f, %.4f)\n", rotated.x(), rotated.y());
    std::printf("After translate (1,2)    = (%.4f, %.4f)\n", result.x(), result.y());
    std::printf("Composed matrix M = T*R:\n");
    std::printf("  [% .4f % .4f % .4f]\n", M(0,0), M(0,1), M(0,2));
    std::printf("  [% .4f % .4f % .4f]\n", M(1,0), M(1,1), M(1,2));
    std::printf("  [% .4f % .4f % .4f]\n", M(2,0), M(2,1), M(2,2));

    // ---- visualisation ----
    const int W = 512, H = 512;
    Image img(W, H, Vec3f(0.10f, 0.10f, 0.12f));
    const float ox = 90.0f, oy = H - 90.0f, scale = 62.0f;  // world->pixel
    auto toPix = [&](float wx, float wy, int& px, int& py) {
        px = (int)std::lround(ox + wx * scale);
        py = (int)std::lround(oy - wy * scale);
    };

    // grid + axes
    Vec3f grid(0.20f, 0.20f, 0.24f), axis(0.55f, 0.55f, 0.6f);
    for (int gx = -1; gx <= 6; ++gx) {
        int px, py0, py1, dummy; toPix(gx, -1, px, py0); toPix(gx, 6, dummy, py1);
        draw_line(img, px, py0, px, py1, grid);
    }
    for (int gy = -1; gy <= 6; ++gy) {
        int px0, px1, py, dummy; toPix(-1, gy, px0, py); toPix(6, gy, px1, dummy);
        draw_line(img, px0, py, px1, py, grid);
    }
    { int px0, py0, px1, py1; toPix(-1, 0, px0, py0); toPix(6, 0, px1, py1);
      draw_line(img, px0, py0, px1, py1, axis);
      toPix(0, -1, px0, py0); toPix(0, 6, px1, py1);
      draw_line(img, px0, py0, px1, py1, axis); }

    auto marker = [&](const Eigen::Vector3f& p, const Vec3f& col, const Eigen::Vector3f& from) {
        int px, py, fx, fy; toPix(p.x(), p.y(), px, py); toPix(from.x(), from.y(), fx, fy);
        draw_line(img, fx, fy, px, py, col * 0.6f);
        fill_disk(img, px, py, 6, col);
    };
    Eigen::Vector3f origin(0, 0, 1);
    marker(P, Vec3f(0.30f, 0.60f, 1.00f), origin);        // input (blue)
    marker(rotated, Vec3f(0.35f, 0.90f, 0.40f), origin);  // rotated (green)
    marker(result, Vec3f(1.00f, 0.35f, 0.30f), rotated);  // translated (red)

    img.save_png("results/a0_transform.png");
    std::printf("Wrote results/a0_transform.png\n");
    return 0;
}
