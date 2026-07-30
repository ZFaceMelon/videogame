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

    static Mat4 scale(float sx, float sy, float sz) {
        Mat4 out = identity();
        out.m[0] = sx;
        out.m[5] = sy;
        out.m[10] = sz;
        return out;
    }
};

struct Quat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

inline Quat quat_normalize(Quat q) {
    const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len > 1e-6f) {
        q.x /= len;
        q.y /= len;
        q.z /= len;
        q.w /= len;
    }
    return q;
}

inline Quat quat_slerp(Quat a, Quat b, float t) {
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (dot < 0.0f) {
        b.x = -b.x;
        b.y = -b.y;
        b.z = -b.z;
        b.w = -b.w;
        dot = -dot;
    }
    if (dot > 0.9995f) {
        Quat out{a.x + t * (b.x - a.x), a.y + t * (b.y - a.y), a.z + t * (b.z - a.z),
                 a.w + t * (b.w - a.w)};
        return quat_normalize(out);
    }
    const float theta = std::acos(dot);
    const float sin_theta = std::sin(theta);
    const float wa = std::sin((1.0f - t) * theta) / sin_theta;
    const float wb = std::sin(t * theta) / sin_theta;
    return quat_normalize(Quat{wa * a.x + wb * b.x, wa * a.y + wb * b.y, wa * a.z + wb * b.z,
                               wa * a.w + wb * b.w});
}

inline Mat4 mat4_from_trs(const float translation[3], const Quat &rotation,
                          const float scale_vec[3]) {
    const float x = rotation.x;
    const float y = rotation.y;
    const float z = rotation.z;
    const float w = rotation.w;

    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float wx = w * x;
    const float wy = w * y;
    const float wz = w * z;

    Mat4 out = Mat4::identity();
    out.m[0] = (1.0f - 2.0f * (yy + zz)) * scale_vec[0];
    out.m[1] = (2.0f * (xy + wz)) * scale_vec[0];
    out.m[2] = (2.0f * (xz - wy)) * scale_vec[0];
    out.m[4] = (2.0f * (xy - wz)) * scale_vec[1];
    out.m[5] = (1.0f - 2.0f * (xx + zz)) * scale_vec[1];
    out.m[6] = (2.0f * (yz + wx)) * scale_vec[1];
    out.m[8] = (2.0f * (xz + wy)) * scale_vec[2];
    out.m[9] = (2.0f * (yz - wx)) * scale_vec[2];
    out.m[10] = (1.0f - 2.0f * (xx + yy)) * scale_vec[2];
    out.m[12] = translation[0];
    out.m[13] = translation[1];
    out.m[14] = translation[2];
    return out;
}

inline Mat4 mat4_inverse(const Mat4 &m) {
    const float *a = m.m;
    Mat4 inv{};

    inv.m[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] +
               a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
    inv.m[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] -
               a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
    inv.m[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] +
               a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
    inv.m[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] -
                a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
    inv.m[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] -
               a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
    inv.m[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] +
               a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
    inv.m[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] -
               a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
    inv.m[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] +
                a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
    inv.m[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] +
               a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
    inv.m[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] -
               a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
    inv.m[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] +
                a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
    inv.m[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] -
                a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
    inv.m[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] -
               a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
    inv.m[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] +
               a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
    inv.m[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] -
                a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
    inv.m[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] +
                a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];

    const float det = a[0] * inv.m[0] + a[1] * inv.m[4] + a[2] * inv.m[8] + a[3] * inv.m[12];
    if (std::fabs(det) < 1e-8f) {
        return Mat4::identity();
    }
    const float inv_det = 1.0f / det;
    for (float &v : inv.m) {
        v *= inv_det;
    }
    return inv;
}

inline void transform_point(const Mat4 &m, float &x, float &y, float &z) {
    const float ox = x;
    const float oy = y;
    const float oz = z;
    x = m.m[0] * ox + m.m[4] * oy + m.m[8] * oz + m.m[12];
    y = m.m[1] * ox + m.m[5] * oy + m.m[9] * oz + m.m[13];
    z = m.m[2] * ox + m.m[6] * oy + m.m[10] * oz + m.m[14];
}

} // namespace engine
