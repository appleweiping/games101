// Minimal linear-RGB framebuffer + PNG writer.
//
// The original GAMES101 framework uses OpenCV's cv::Mat purely as an off-screen
// buffer plus cv::imwrite / cv::imshow.  Those highgui paths need a display and
// pull in a heavy dependency, so on this headless CPU box we replace them with a
// tiny std::vector framebuffer and stb_image_write for PNG output.  The rendering
// maths is unchanged — only the pixel container / file I/O differs.
#pragma once
#include "math_utils.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include "stb_image_write.h"

namespace g101 {

struct Image {
    int w, h;
    std::vector<Vec3f> buf;  // linear RGB, nominally [0,1]

    Image(int w_, int h_, const Vec3f& fill = Vec3f::Zero())
        : w(w_), h(h_), buf(static_cast<size_t>(w_) * h_, fill) {}

    Vec3f& at(int x, int y) { return buf[static_cast<size_t>(y) * w + x]; }
    const Vec3f& at(int x, int y) const { return buf[static_cast<size_t>(y) * w + x]; }

    // y is measured from the top of the image.
    void set(int x, int y, const Vec3f& c) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        buf[static_cast<size_t>(y) * w + x] = c;
    }

    // gamma:  apply 1/2.2 encoding (use for physically-linear renders).
    // flipY:  treat buffer row 0 as the BOTTOM of the image (OpenGL convention).
    bool save_png(const std::string& path, bool gamma = false, bool flipY = false) const {
        std::vector<uint8_t> pix(static_cast<size_t>(w) * h * 3);
        for (int y = 0; y < h; ++y) {
            int srcY = flipY ? (h - 1 - y) : y;
            for (int x = 0; x < w; ++x) {
                const Vec3f& c = buf[static_cast<size_t>(srcY) * w + x];
                for (int k = 0; k < 3; ++k) {
                    float v = clampv(0.0f, 1.0f, c[k]);
                    if (gamma) v = std::pow(v, 1.0f / 2.2f);
                    pix[(static_cast<size_t>(y) * w + x) * 3 + k] =
                        static_cast<uint8_t>(v * 255.0f + 0.5f);
                }
            }
        }
        return stbi_write_png(path.c_str(), w, h, 3, pix.data(), w * 3) != 0;
    }
};

}  // namespace g101
