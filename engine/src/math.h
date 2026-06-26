#pragma once

#include <cmath>
#include <cstring>

namespace engine {

struct Mat4 {
    float m[16]{};

    static Mat4 identity() {
        Mat4 out{};
        out.m[0] = out.m[5] = out.m[10] = out.m[15] = 1.0f;
        return out;
    }

    static Mat4 perspective(float fovy_rad, float aspect, float near_z, float far_z) {
        const float tan_half = std::tan(fovy_rad * 0.5f);
        Mat4 out{};
        out.m[0] = 1.0f / (aspect * tan_half);
        out.m[5] = 1.0f / tan_half;
        out.m[10] = -(far_z + near_z) / (far_z - near_z);
        out.m[11] = -1.0f;
        out.m[14] = -(2.0f * far_z * near_z) / (far_z - near_z);
        return out;
    }

    static Mat4 look_at(float eye_x, float eye_y, float eye_z,
                        float center_x, float center_y, float center_z,
                        float up_x, float up_y, float up_z) {
        float fx = center_x - eye_x;
        float fy = center_y - eye_y;
        float fz = center_z - eye_z;
        const float flen = std::sqrt(fx * fx + fy * fy + fz * fz);
        fx /= flen;
        fy /= flen;
        fz /= flen;

        float sx = fy * up_z - fz * up_y;
        float sy = fz * up_x - fx * up_z;
        float sz = fx * up_y - fy * up_x;
        const float slen = std::sqrt(sx * sx + sy * sy + sz * sz);
        sx /= slen;
        sy /= slen;
        sz /= slen;

        const float ux = sy * fz - sz * fy;
        const float uy = sz * fx - sx * fz;
        const float uz = sx * fy - sy * fx;

        Mat4 out = identity();
        out.m[0] = sx;
        out.m[1] = ux;
        out.m[2] = -fx;
        out.m[4] = sy;
        out.m[5] = uy;
        out.m[6] = -fy;
        out.m[8] = sz;
        out.m[9] = uz;
        out.m[10] = -fz;
        out.m[12] = -(sx * eye_x + sy * eye_y + sz * eye_z);
        out.m[13] = -(ux * eye_x + uy * eye_y + uz * eye_z);
        out.m[14] = fx * eye_x + fy * eye_y + fz * eye_z;
        return out;
    }

    static Mat4 orthographic(float left, float right, float bottom, float top,
                             float near_z, float far_z) {
        Mat4 out = identity();
        out.m[0] = 2.0f / (right - left);
        out.m[5] = 2.0f / (top - bottom);
        out.m[10] = -2.0f / (far_z - near_z);
        out.m[12] = -(right + left) / (right - left);
        out.m[13] = -(top + bottom) / (top - bottom);
        out.m[14] = -(far_z + near_z) / (far_z - near_z);
        return out;
    }

    static Mat4 rotate_y(float radians) {
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        Mat4 out = identity();
        out.m[0] = c;
        out.m[2] = s;
        out.m[8] = -s;
        out.m[10] = c;
        return out;
    }

    static Mat4 rotate_x(float radians) {
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        Mat4 out = identity();
        out.m[5] = c;
        out.m[6] = -s;
        out.m[9] = s;
        out.m[10] = c;
        return out;
    }

    static Mat4 translate(float x, float y, float z) {
        Mat4 out = identity();
        out.m[12] = x;
        out.m[13] = y;
        out.m[14] = z;
        return out;
    }

    Mat4 operator*(const Mat4 &rhs) const {
        Mat4 out{};
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                out.m[col * 4 + row] =
                    m[row] * rhs.m[col * 4 + 0] +
                    m[4 + row] * rhs.m[col * 4 + 1] +
                    m[8 + row] * rhs.m[col * 4 + 2] +
                    m[12 + row] * rhs.m[col * 4 + 3];
            }
        }
        return out;
    }
};

} // namespace engine
