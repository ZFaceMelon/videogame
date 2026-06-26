#pragma once

#include <cmath>
#include <vector>

namespace engine {

constexpr int kVertexFloats = 9;

inline void append_vertex(std::vector<float> &out,
                          float x, float y, float z,
                          float nx, float ny, float nz,
                          float r, float g, float b) {
  out.insert(out.end(), {x, y, z, nx, ny, nz, r, g, b});
}

inline void normalize3(float &x, float &y, float &z) {
  const float length = std::sqrt(x * x + y * y + z * z);
  if (length > 1e-6f) {
    x /= length;
    y /= length;
    z /= length;
  }
}

inline void append_quad(std::vector<float> &out,
                        float ax, float ay, float az,
                        float bx, float by, float bz,
                        float dx, float dy, float dz,
                        float ex, float ey, float ez,
                        float r, float g, float b) {
  const float ux = bx - ax;
  const float uy = by - ay;
  const float uz = bz - az;
  const float vx = dx - ax;
  const float vy = dy - ay;
  const float vz = dz - az;
  float nx = uy * vz - uz * vy;
  float ny = uz * vx - ux * vz;
  float nz = ux * vy - uy * vx;
  normalize3(nx, ny, nz);

  append_vertex(out, ax, ay, az, nx, ny, nz, r, g, b);
  append_vertex(out, bx, by, bz, nx, ny, nz, r, g, b);
  append_vertex(out, dx, dy, dz, nx, ny, nz, r, g, b);
  append_vertex(out, ax, ay, az, nx, ny, nz, r, g, b);
  append_vertex(out, dx, dy, dz, nx, ny, nz, r, g, b);
  append_vertex(out, ex, ey, ez, nx, ny, nz, r, g, b);
}

inline void append_quad_vnorm(std::vector<float> &out,
                              float ax, float ay, float az, float anx, float any, float anz,
                              float bx, float by, float bz, float bnx, float bny, float bnz,
                              float dx, float dy, float dz, float dnx, float dny, float dnz,
                              float ex, float ey, float ez, float enx, float eny, float enz,
                              float r, float g, float b) {
  append_vertex(out, ax, ay, az, anx, any, anz, r, g, b);
  append_vertex(out, bx, by, bz, bnx, bny, bnz, r, g, b);
  append_vertex(out, dx, dy, dz, dnx, dny, dnz, r, g, b);
  append_vertex(out, ax, ay, az, anx, any, anz, r, g, b);
  append_vertex(out, dx, dy, dz, dnx, dny, dnz, r, g, b);
  append_vertex(out, ex, ey, ez, enx, eny, enz, r, g, b);
}

inline void append_box(std::vector<float> &out,
                       float cx, float cy, float cz,
                       float hx, float hy, float hz,
                       float r, float g, float b) {
  const float x0 = cx - hx, x1 = cx + hx;
  const float y0 = cy - hy, y1 = cy + hy;
  const float z0 = cz - hz, z1 = cz + hz;

  append_quad(out, x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1, r, g, b);
  append_quad(out, x1, y0, z0, x0, y0, z0, x0, y1, z0, x1, y1, z0, r, g, b);
  append_quad(out, x0, y0, z0, x0, y0, z1, x0, y1, z1, x0, y1, z0, r, g, b);
  append_quad(out, x1, y0, z1, x1, y0, z0, x1, y1, z0, x1, y1, z1, r, g, b);
  append_quad(out, x0, y1, z1, x1, y1, z1, x1, y1, z0, x0, y1, z0, r, g, b);
  append_quad(out, x0, y0, z0, x1, y0, z0, x1, y0, z1, x0, y0, z1, r, g, b);
}

} // namespace engine
