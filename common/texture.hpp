// Texture wrapper around stb_image with bilinear sampling.  Falls back to a
// procedural pattern when no image file is supplied, so texture/bump/displacement
// still have signal to work with even without the model's texture asset.
#pragma once
#include "math_utils.hpp"
#include <string>
#include <vector>
#include "stb_image.h"

namespace g101 {

struct Texture {
    int w = 0, h = 0;
    std::vector<Vec3f> data;  // linear-ish [0,1] RGB
    bool procedural = false;

    bool load(const std::string& path) {
        int n = 0;
        unsigned char* img = stbi_load(path.c_str(), &w, &h, &n, 3);
        if (!img) { make_procedural(); return false; }
        data.resize((size_t)w * h);
        for (int i = 0; i < w * h; ++i)
            data[i] = Vec3f(img[i * 3], img[i * 3 + 1], img[i * 3 + 2]) / 255.0f;
        stbi_image_free(img);
        return true;
    }

    void make_procedural(int W = 512, int H = 512) {
        procedural = true; w = W; h = H; data.resize((size_t)W * H);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                float u = (float)x / W, v = (float)y / H;
                bool ck = ((x / 32) + (y / 32)) & 1;
                Vec3f base = ck ? Vec3f(0.85f, 0.55f, 0.30f) : Vec3f(0.25f, 0.45f, 0.75f);
                float band = 0.5f + 0.5f * std::sin(v * PI * 6);
                data[(size_t)y * W + x] = base * (0.6f + 0.4f * band);
                (void)u;
            }
    }

    // Nearest fetch with clamp/wrap.
    Vec3f fetch(int x, int y) const {
        x = ((x % w) + w) % w; y = ((y % h) + h) % h;
        return data[(size_t)y * w + x];
    }

    // Bilinear sample; v flipped to match OBJ texcoord convention.
    Vec3f sample(float u, float v) const {
        u = u - std::floor(u);
        v = v - std::floor(v);
        float fx = u * w - 0.5f, fy = (1.0f - v) * h - 0.5f;
        int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
        float tx = fx - x0, ty = fy - y0;
        Vec3f c00 = fetch(x0, y0), c10 = fetch(x0 + 1, y0);
        Vec3f c01 = fetch(x0, y0 + 1), c11 = fetch(x0 + 1, y0 + 1);
        return (c00 * (1 - tx) + c10 * tx) * (1 - ty) + (c01 * (1 - tx) + c11 * tx) * ty;
    }

    // Scalar height = luminance, for bump / displacement gradients.
    float height(float u, float v) const {
        Vec3f c = sample(u, v);
        return 0.2126f * c.x() + 0.7152f * c.y() + 0.0722f * c.z();
    }
};

}  // namespace g101
